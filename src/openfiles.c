#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include "../include/openfiles.h"
#include "../include/utils.h"
#include "../include/socketconn.h"
#include "../include/icl_hash.h"
#include "../include/scfiles.h"

#define NBUCKETS 128 // number of buckets used for the open files hash table

extern struct fspipe_socket fspipe_socket;

static icl_hash_t *open_files_table = NULL; // hash table with all the open files. Each file has its path as key
static pthread_mutex_t open_files_mtx = PTHREAD_MUTEX_INITIALIZER;

int fspipe_open_files_table_init(void) {
    // destroys the table if it already exists
    if (open_files_table) MINUS1(fspipe_open_files_table_destroy(), return -1)

    open_files_table = icl_hash_create(NBUCKETS, NULL, NULL);
    if (open_files_table == NULL) return -1;

    return 0;
}

int fspipe_open_files_table_destroy(void) {
    if (icl_hash_destroy(open_files_table, NULL, (void (*)(void *)) &fspipe_file_free) == -1)
        return -1;
    open_files_table = NULL;
    return 0;
}

/**
 * Removes the file with key path from the open file table. The file structure is also freed.
 *
 * @param path file's path
 *
 * @return 0 on success, -1 on error
 */
int fspipe_remove_open_file(const char *path) {
    int deleted, err;
    PTH(err, pthread_mutex_lock(&open_files_mtx), return -1)

    deleted = icl_hash_delete(open_files_table, (char*) path, NULL, NULL);

    PTH(err, pthread_mutex_unlock(&open_files_mtx), return -1)
    return deleted;
}

/**
 * Returns the file structure for the given path or NULL if it doesn't exist
 *
 * @param path file's path
 *
 * @return the file structure or NULL if it doesn't exist
 */
static struct fspipe_file *fspipe_get_open_file(const char *path) {
    int err;
    struct fspipe_file *file;

    PTH(err, pthread_mutex_lock(&open_files_mtx), return NULL)

    file = icl_hash_find(open_files_table, (char*) path);

    PTH(err, pthread_mutex_unlock(&open_files_mtx), return NULL)

    return file;
}

/**
 * Returns the file structure for the given path or NULL if it doesn't exist
 *
 * @param path file's path
 *
 * @return the file structure or NULL if it doesn't exist
 */
static struct fspipe_file *fspipe_get_or_create_open_file(const char *path, int *just_created) {
    int err;
    struct fspipe_file *file;
    *just_created = 0;

    PTH(err, pthread_mutex_lock(&open_files_mtx), return NULL)

    file = icl_hash_find(open_files_table, (char*) path);
    EQNULL(file, file = fspipe_file_alloc(path); *just_created = 1)

    if (file != NULL && *just_created) {
        if (icl_hash_insert(open_files_table, (void*) file->path, file) == NULL) {
            fspipe_file_free(file);
            file = NULL;
        }
    }

    PTH(err, pthread_mutex_unlock(&open_files_mtx), return NULL)

    return file;
}

struct fspipe_file *fspipe_file_open_local(const char *path, int mode) {
    int err, bytes, just_created = 0;
    struct fspipe_file *file;

    if (mode == O_RDWR) {
        errno = EINVAL;
        return NULL;
    }

    /* open the file or create it */
    file = fspipe_get_or_create_open_file(path, &just_created);
    if (file == NULL) return NULL;

    NOTZERO(fspipe_file_lock(file), goto error)

    /* update readers and writers and notify who's waiting for readers/writers */
    if (mode == O_RDONLY) file->readers++;
    else if (mode == O_WRONLY) file->writers++;
    DEBUGFILE(file);
    PTH(err, pthread_cond_broadcast(&(file->canopen)), fspipe_file_unlock(file); goto error)

    PTH(err, pthread_mutex_lock(&(fspipe_socket.writesktmtx)), fspipe_file_unlock(file); goto error)
    bytes = write_socket_message(fspipe_socket.fd_skt, OPEN, path, mode);
    PTH(err, pthread_mutex_unlock(&(fspipe_socket.writesktmtx)), fspipe_file_unlock(file); goto error)
    if (bytes <= 0) { // cannot write over socket
        fspipe_file_unlock(file);
        goto error;
    }

    DEBUG("sent: OPEN %s %d\n", path, mode);

    /* wait for at least one writer and one reader */
    while (file->readers == 0 || file->writers == 0) {
        PTH(err, pthread_cond_wait(&(file->canopen), &(file->mtx)), fspipe_file_unlock(file); goto error)
    }

    NOTZERO(fspipe_file_unlock(file), goto error)

    return file;

    error:
    if (just_created) {
        fspipe_remove_open_file(path);
        fspipe_file_free(file);
    }
    return NULL;
}

struct fspipe_file *fspipe_file_open_remote(const char *path, int mode) {
    int err, just_created = 0;
    struct fspipe_file *file;

    if (mode == O_RDWR) {
        errno = EINVAL;
        return NULL;
    }

    file = fspipe_get_or_create_open_file(path, &just_created);
    if (file == NULL) return NULL;

    NOTZERO(fspipe_file_lock(file), goto error)

    if (mode == O_RDONLY) file->readers++;
    else if (mode == O_WRONLY) file->writers++;
    DEBUGFILE(file);
    PTH(err, pthread_cond_broadcast(&(file->canopen)), fspipe_file_unlock(file); goto error)

    NOTZERO(fspipe_file_unlock(file), goto error)

    return file;

    error:
    if (just_created) {
        fspipe_remove_open_file(path);
        fspipe_file_free(file);
    }
    return NULL;
}

int fspipe_file_close_local(struct fspipe_file *file, int mode) {
    int bytes, err, free_memory = 0;

    NOTZERO(fspipe_file_lock(file), return -1)

    if (mode == O_WRONLY) {
        file->writers--;
        if (file->writers == 0) PTH(err, pthread_cond_broadcast(&(file->isempty)), fspipe_file_unlock(file); return -1)
    } else if (mode == O_RDONLY) {
        file->readers--;
        if (file->readers == 0) PTH(err, pthread_cond_broadcast(&(file->isfull)), fspipe_file_unlock(file); return -1)
    }

    DEBUGFILE(file);
    if (file->writers == 0 && file->readers == 0) {
        close(file->pipefd[0]);
        close(file->pipefd[1]);
        free_memory = 1;
    }

    PTH(err, pthread_mutex_lock(&(fspipe_socket.writesktmtx)), return -1)
    bytes = write_socket_message(fspipe_socket.fd_skt, CLOSE, file->path, mode);
    PTH(err, pthread_mutex_unlock(&(fspipe_socket.writesktmtx)), return -1)
    if (bytes <= 0) return bytes;

    DEBUG("sent: CLOSE %s %d\n", file->path, mode);

    if (free_memory) {
        MINUS1(fspipe_remove_open_file(file->path), err = -1)
        NOTZERO(fspipe_file_unlock(file), err = -1)
        MINUS1(fspipe_file_free(file), err = -1)
    } else {
        NOTZERO(fspipe_file_unlock(file), err = -1)
    }

    if (err == -1) return -1;

    return bytes;
}

int fspipe_file_close_remote(const char *path, int mode) {
    struct fspipe_file *file = fspipe_get_open_file(path);
    if (file == NULL) return -1;

    int err;

    NOTZERO(fspipe_file_lock(file), return -1)

    if (mode == O_WRONLY) {
        file->writers--;
        if (file->writers == 0) PTH(err, pthread_cond_broadcast(&(file->isempty)), fspipe_file_unlock(file); return -1)
    } else if (mode == O_RDONLY) {
        file->readers--;
        if (file->readers == 0) PTH(err, pthread_cond_broadcast(&(file->isfull)), fspipe_file_unlock(file); return -1)
    }

    DEBUGFILE(file);
    if (file->writers == 0 && file->readers == 0) {
        close(file->pipefd[0]);
        close(file->pipefd[1]);
        MINUS1(fspipe_remove_open_file(file->path), err = -1)
        NOTZERO(fspipe_file_unlock(file), err = -1)
        MINUS1(fspipe_file_free(file), err = -1)
    } else {
        NOTZERO(fspipe_file_unlock(file), err = -1)
    }

    if (err == -1) return -1;

    return 0;
}

int fspipe_file_write_local(const char *path, char *buf, size_t size) {
    struct fspipe_file *file = fspipe_get_open_file(path);
    if (file == NULL) return -1;

    int err, bytes;
    char *bufptr = buf;
    size_t capacity = 1024; //TODO change into real file capacity

    NOTZERO(fspipe_file_lock(file), return -1)

    while (file->size + size > capacity && file->readers > 0) { //TODO block if the pipe is full but send portions of data
        fprintf(stderr, "cannot write: file size %ld < %ld\n", file->size, size);
        PTH(err, pthread_cond_wait(&(file->isfull), &(file->mtx)), fspipe_file_unlock(file); return -1)
    }

    // return -1 and sets errno to EPIPE if there are no readers
    if (file->readers == 0) {
        errno = EPIPE;
        fspipe_file_unlock(file);
        return -1;
    }

    bytes = writen(file->pipefd[1], bufptr, size);
    if (bytes > 0) {
        file->size += bytes;
        DEBUGFILE(file);
        PTH(err, pthread_cond_broadcast(&(file->isempty)), fspipe_file_unlock(file); return -1)
    }
    NOTZERO(fspipe_file_unlock(file), return -1)

    return bytes;
}

int fspipe_file_write_remote(struct fspipe_file *file, const char *path, char *buf, size_t size) {
    int err, bytes;

    size_t capacity = 1024; //TODO change into real file capacity

    NOTZERO(fspipe_file_lock(file), return -1)

    /* wait for enough space if there is at least one reader */
    while (file->size + size > capacity && file->readers > 0) { //TODO block if the pipe is full but send portions of data
        PTH(err, pthread_cond_wait(&(file->isfull), &(file->mtx)), fspipe_file_unlock(file); return -1)
    }

    /* if there are no readers then errno = EPIPE and return -1 */
    if (file->readers == 0) {
        errno = EPIPE;
        fspipe_file_unlock(file);
        return -1;
    }

    /* write data over socket */
    PTH(err, pthread_mutex_lock(&(fspipe_socket.writesktmtx)), fspipe_file_unlock(file); return -1)
    bytes = write_socket_message(fspipe_socket.fd_skt, WRITE, path, -1);
    if (bytes > 0) {
        bytes = socket_write_h(fspipe_socket.fd_skt, (void*) buf, size);
    }
    PTH(err, pthread_mutex_unlock(&(fspipe_socket.writesktmtx)), fspipe_file_unlock(file); return -1)
    if (bytes > 0) {
        DEBUG("sent: WRITE %s %ld DATA\n", path, size);

        file->size += bytes;
        DEBUGFILE(file);
        PTH(err, pthread_cond_broadcast(&(file->isempty)), fspipe_file_unlock(file); return -1)
        //TODO wait for a response and return it. if it is a failure then set file->size -= bytes. if it is success then pthread_cond_broadcast
    }
    NOTZERO(fspipe_file_unlock(file), return -1)

    return bytes;
}

int fspipe_file_read_local(struct fspipe_file *file, const char *path, char *buf, size_t size) {
    int err, bytes, bytes_wrote;
    char *bufptr = buf;

    NOTZERO(fspipe_file_lock(file), return -1)

    /* file has not enough data */
    while(file->size < size && file->writers > 0) {
        fprintf(stderr, "cannot read: file size %ld < %ld\n", file->size, size);
        PTH(err, pthread_cond_wait(&(file->isempty), &(file->mtx)), fspipe_file_unlock(file); return -1)
    }

    /* return EOF if there are no writers */
    if (file->writers == 0) {
        NOTZERO(fspipe_file_unlock(file), return -1)
        return 0; //EOF
    }

    /* read from pipe */
    bytes = readn(file->pipefd[0], bufptr, size);
    if (bytes <= 0) {
        fspipe_file_unlock(file);
        return -1;
    }

    /* update size and wake up writers */
    file->size -= bytes;
    PTH(err, pthread_cond_broadcast(&(file->isfull)), fspipe_file_unlock(file); return -1)

    PTH(err, pthread_mutex_lock(&(fspipe_socket.writesktmtx)), fspipe_file_unlock(file); return -1)
    bytes_wrote = write_socket_message(fspipe_socket.fd_skt, READ, path, bytes);
    PTH(err, pthread_mutex_unlock(&(fspipe_socket.writesktmtx)), fspipe_file_unlock(file); return -1)
    if (bytes_wrote > 0) DEBUGFILE(file);

    NOTZERO(fspipe_file_unlock(file), return -1)
    if (bytes_wrote <= 0) return -1;

    DEBUG("sent: READ %s %d\n", path, bytes);

    return bytes;
}

int fspipe_file_read_remote(const char* path, size_t size) {
    int err;
    struct fspipe_file *file = fspipe_get_open_file(path);
    if (file == NULL) return -1;

    NOTZERO(fspipe_file_lock(file), return -1)

    /* update size and wake up writers */
    file->size -= size;
    PTH(err, pthread_cond_broadcast(&(file->isfull)), fspipe_file_unlock(file); return -1)
    DEBUGFILE(file);

    NOTZERO(fspipe_file_unlock(file), return -1)

    return size;
}