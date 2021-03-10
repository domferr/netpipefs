#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include "../include/fspipe_file.h"
#include "../include/utils.h"
#include "../include/icl_hash.h"
#include "../include/scfiles.h"

#define READ_END 0
#define WRITE_END 1
#define NBUCKETS 128 // number of buckets used for the open files hash table

static icl_hash_t *open_files_table = NULL; // hash table with all the open files. Each file has its path as key
static pthread_mutex_t open_files_mtx = PTHREAD_MUTEX_INITIALIZER;

/**
 * Allocates new memory for a new file structure with the given path.
 *
 * @param path file's path
 *
 * @return the created file structure or NULL on error and it sets errno
 */
static struct fspipe_file *fspipe_file_alloc(const char *path) {
    int err;
    struct fspipe_file *file = (struct fspipe_file *) malloc(sizeof(struct fspipe_file));
    EQNULL(file, return NULL)

    EQNULL(file->path = strdup(path), free(file); return NULL)
    if ((err = pthread_mutex_init(&(file->mtx), NULL) != 0)) {
        errno = err;
        free((void*) file->path);
        free(file);
        return NULL;
    }
    if ((err = pthread_cond_init(&(file->canopen), NULL)) != 0) {
        errno = err;
        free((void*) file->path);
        pthread_mutex_destroy(&(file->mtx)); free(file);
        return NULL;
    }

    file->writers = 0;
    file->readers = 0;
    file->remote_error = 0;
    file->pipefd[READ_END] = -1;
    file->pipefd[WRITE_END] = -1;

    return file;
}

/**
 * Frees the memory allocated for the given file.
 *
 * @param file the file structure
 *
 * @return 0 on success, -1 on error and it sets errno
 */
static int fspipe_file_free(struct fspipe_file *file) {
    int ret = 0, err;
    if ((err = pthread_mutex_destroy(&(file->mtx))) != 0) { errno = err; ret = -1; }
    if ((err = pthread_cond_destroy(&(file->canopen))) != 0) { errno = err; ret = -1; }
    free((void*) file->path);
    free(file);

    return ret;
}

int fspipe_open_files_table_init(void) {
    // destroys the table if it exists
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
 * Creates a new file and adds it into the open files table. The path is used as file's key.
 *
 * @param path file's path used as file's key
 *
 * @return the pointer to the new file or NULL on error
 */
static struct fspipe_file *fspipe_add_file(const char *path) {
    int err;
    struct fspipe_file *file = fspipe_file_alloc(path);
    EQNULL(file, return NULL)

    PTH(err, pthread_mutex_lock(&open_files_mtx), fspipe_file_free(file); return NULL)

    if (icl_hash_insert(open_files_table, (void*) file->path, file) == NULL) {
        fspipe_file_free(file);
        return NULL;
    }

    PTH(err, pthread_mutex_unlock(&open_files_mtx), fspipe_file_free(file); return NULL)

    return file;
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
 * Removes the file with key path from the open file table. The file structure is also freed.
 *
 * @param path file's path
 *
 * @return 0 on success, -1 on error
 */
static int fspipe_remove_file(char *path) {
    int deleted, err;
    PTH(err, pthread_mutex_lock(&open_files_mtx), return -1)

    deleted = icl_hash_delete(open_files_table, path, NULL, NULL);

    PTH(err, pthread_mutex_unlock(&open_files_mtx), return -1)
    return deleted;
}

static int fspipe_file_lock(struct fspipe_file *file) {
    int err = pthread_mutex_lock(&(file->mtx));
    if (err != 0) errno = err;
    return err;
}

static int fspipe_file_unlock(struct fspipe_file *file) {
    int err = pthread_mutex_unlock(&(file->mtx));
    if (err != 0) errno = err;
    return err;
}

struct fspipe_file *fspipe_file_open_local(const char *path, int mode) {
    int err, just_created = 0;
    struct fspipe_file *file = NULL;

    // both read and write mode is not allowed
    if (mode == O_RDWR) {
        errno = EPERM;
        return NULL;
    }

    if ((file = fspipe_get_open_file(path)) == NULL) {
        // file doesn't exist then create it
        EQNULL(file = fspipe_add_file(path), return NULL)
        just_created = 1;
    }

    NOTZERO(err = fspipe_file_lock(file), goto end)

    // wait until there is at least one reader and one writer or an error occurs
    while (file->remote_error == 0 && (file->writers == 0 || file->readers == 0)) {
        fprintf(stderr, "LET'S SLEEP | %s\n", path);
        PTH(err, pthread_cond_wait(&(file->canopen), &(file->mtx)), fspipe_file_unlock(file); goto end)
        fprintf(stderr, "WOKE UP     | %s\n", path);
    }

    // if an error occurred remotely then reset the error flag and set errno
    if (file->remote_error) {
        errno = file->remote_error;
        file->remote_error = 0;
        err = -1;
    }

    NOTZERO(fspipe_file_unlock(file), err = -1)

end:
    if (!err)
        return file;
    if (just_created) { // file was just created but an error occurred
        fspipe_remove_file((void *) file->path);
        fspipe_file_free(file);
    }
    return NULL;
}

struct fspipe_file *fspipe_file_open_remote(const char *path, int mode, int error) {
    int failure, just_created = 0;
    struct fspipe_file *file = NULL;

    if ((file = fspipe_get_open_file(path)) == NULL) {
        // file doesn't exist then create it
        EQNULL(file = fspipe_add_file(path), return NULL)
        just_created = 1;
    }

    NOTZERO(fspipe_file_lock(file), failure = 1; goto end)

    if (error) file->remote_error = error;
    else if (mode == O_RDONLY) file->readers++;
    else if (mode == O_WRONLY) file->writers++;

    if ((failure = pthread_cond_broadcast(&(file->canopen))) != 0) errno = failure;

    NOTZERO(fspipe_file_unlock(file), failure = 1)

end:
    if (!failure) return file;

    if (just_created) {
        fspipe_remove_file((void *) file->path);
        fspipe_file_free(file);
    }
    return NULL;
}

int fspipe_file_close(const char *path, int mode) {
    int free_memory = 0;
    struct fspipe_file *file = fspipe_get_open_file(path);
    if (file == NULL) return -1;

    NOTZERO(fspipe_file_lock(file), return -1)

    if (mode == O_WRONLY) {
        file->writers--;
    } else if (mode == O_RDONLY) {
        file->readers--;
    }
    if (file->readers == 0 && file->writers == 0) {
        fspipe_remove_file((void*) file->path);
        if (file->pipefd[READ_END] != -1) {
            close(file->pipefd[READ_END]);
            file->pipefd[READ_END] = -1;
        }

        if (file->pipefd[WRITE_END] != -1) {
            close(file->pipefd[WRITE_END]);
            file->pipefd[WRITE_END] = -1;
        }

        free_memory = 1;
    }
    NOTZERO(fspipe_file_unlock(file), return -1)
    if (free_memory) fspipe_file_free(file);

    return 0;
}

int fspipe_file_write(const char *path, char *buf, size_t size) {
    struct fspipe_file *file = fspipe_get_open_file(path);
    if (file == NULL) return 0; // file is not open!

    return writen(file->pipefd[WRITE_END], buf, size);    // TODO handle concurrent writes
}

int fspipe_file_read(const char *path, char *buf, size_t size) {
    struct fspipe_file *file = fspipe_get_open_file(path);
    if (file == NULL) return 0; // file is not open!

    return readn(file->pipefd[READ_END], buf, size);    // TODO handle concurrent reads
}