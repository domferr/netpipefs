#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include "../include/netpipefs_file.h"
#include "../include/utils.h"
#include "../include/socketconn.h"
#include "../include/openfiles.h"
#include "../include/scfiles.h"

extern struct netpipefs_socket netpipefs_socket;

struct netpipefs_file *netpipefs_file_alloc(const char *path) {
    int err;
    struct netpipefs_file *file = (struct netpipefs_file *) malloc(sizeof(struct netpipefs_file));
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
        goto error;
    }

    if ((err = pthread_cond_init(&(file->isfull), NULL)) != 0) {
        errno = err;
        pthread_cond_destroy(&(file->canopen));
        goto error;
    }

    if ((err = pthread_cond_init(&(file->isempty), NULL)) != 0) {
        errno = err;
        pthread_cond_destroy(&(file->canopen));
        pthread_cond_destroy(&(file->isfull));
        goto error;
    }

    file->buffer = cbuf_alloc(netpipefs_options.pipecapacity);
    if (file->buffer == NULL) {
        pthread_cond_destroy(&(file->canopen));
        pthread_cond_destroy(&(file->isfull));
        pthread_cond_destroy(&(file->isempty));
        goto error;
    }

    file->writers = 0;
    file->readers = 0;
    file->remotesize = 0;
    file->remotecapacity = netpipefs_socket.remotepipecapacity;

    return file;

error:
    free((void*) file->path);
    pthread_mutex_destroy(&(file->mtx));
    free(file);
    return NULL;
}

int netpipefs_file_free(struct netpipefs_file *file) {
    int ret = 0, err;
    if ((err = pthread_mutex_destroy(&(file->mtx))) != 0) { errno = err; ret = -1; }
    if ((err = pthread_cond_destroy(&(file->canopen))) != 0) { errno = err; ret = -1; }
    if ((err = pthread_cond_destroy(&(file->isfull))) != 0) { errno = err; ret = -1; }
    if ((err = pthread_cond_destroy(&(file->isempty))) != 0) { errno = err; ret = -1; }
    cbuf_free(file->buffer);
    free((void*) file->path);
    free(file);

    return ret;
}

int netpipefs_file_lock(struct netpipefs_file *file) {
    int err = pthread_mutex_lock(&(file->mtx));
    if (err != 0) errno = err;
    return err;
}

int netpipefs_file_unlock(struct netpipefs_file *file) {
    int err = pthread_mutex_unlock(&(file->mtx));
    if (err != 0) errno = err;
    return err;
}

struct netpipefs_file *netpipefs_file_open(const char *path, int mode) {
    int err, bytes, just_created = 0;
    struct netpipefs_file *file;

    if (mode == O_RDWR) {
        errno = EINVAL;
        return NULL;
    }

    /* get the file struct or create it */
    file = netpipefs_get_or_create_open_file(path, &just_created);
    if (file == NULL) return NULL;

    NOTZERO(netpipefs_file_lock(file), goto error)

    /* update readers and writers and notify who's waiting for readers/writers */
    if (mode == O_RDONLY) file->readers++;
    else if (mode == O_WRONLY) file->writers++;
    DEBUGFILE(file);
    PTH(err, pthread_cond_broadcast(&(file->canopen)), netpipefs_file_unlock(file); goto error)

    PTH(err, pthread_mutex_lock(&(netpipefs_socket.writemtx)), netpipefs_file_unlock(file); goto error)
    bytes = write_socket_message(netpipefs_socket.fd_skt, OPEN, path, mode);
    PTH(err, pthread_mutex_unlock(&(netpipefs_socket.writemtx)), netpipefs_file_unlock(file); goto error)
    if (bytes <= 0) { // cannot write over socket
        netpipefs_file_unlock(file);
        goto error;
    }

    DEBUG("sent: OPEN %s %d\n", path, mode);

    /* wait for at least one writer and one reader */
    while (file->readers == 0 || file->writers == 0) {
        PTH(err, pthread_cond_wait(&(file->canopen), &(file->mtx)), netpipefs_file_unlock(file); goto error)
    }

    NOTZERO(netpipefs_file_unlock(file), goto error)

    return file;

    error:
    if (just_created) {
        netpipefs_remove_open_file(path);
        netpipefs_file_free(file);
    }
    return NULL;
}

struct netpipefs_file *netpipefs_file_open_update(const char *path, int mode) {
    int err, just_created = 0;
    struct netpipefs_file *file;

    if (mode == O_RDWR) {
        errno = EINVAL;
        return NULL;
    }

    file = netpipefs_get_or_create_open_file(path, &just_created);
    if (file == NULL) return NULL;

    NOTZERO(netpipefs_file_lock(file), goto error)

    if (mode == O_RDONLY) file->readers++;
    else if (mode == O_WRONLY) file->writers++;
    DEBUGFILE(file);
    PTH(err, pthread_cond_broadcast(&(file->canopen)), netpipefs_file_unlock(file); goto error)

    NOTZERO(netpipefs_file_unlock(file), goto error)

    return file;

    error:
    if (just_created) {
        netpipefs_remove_open_file(path);
        netpipefs_file_free(file);
    }
    return NULL;
}

int netpipefs_file_send(struct netpipefs_file *file, const char *buf, size_t size) {
    int err, bytes;

    NOTZERO(netpipefs_file_lock(file), return -1)

    /* wait for enough remote space if there is at least one reader */
    while (file->remotesize + size > file->remotecapacity && file->readers > 0) { //TODO block if the pipe is full but send portions of data
        PTH(err, pthread_cond_wait(&(file->isfull), &(file->mtx)), netpipefs_file_unlock(file); return -1)
    }

    /* if there are no readers then errno = EPIPE and return -1 */
    if (file->readers == 0) {
        errno = EPIPE;
        netpipefs_file_unlock(file);
        return -1;
    }

    /* write data over socket */
    PTH(err, pthread_mutex_lock(&(netpipefs_socket.writemtx)), netpipefs_file_unlock(file); return -1)
    bytes = write_socket_message(netpipefs_socket.fd_skt, WRITE, file->path, -1);
    if (bytes > 0) {
        bytes = socket_write_h(netpipefs_socket.fd_skt, (void*) buf, size);
    }
    PTH(err, pthread_mutex_unlock(&(netpipefs_socket.writemtx)), netpipefs_file_unlock(file); return -1)
    if (bytes > 0) {
        DEBUG("sent: WRITE %s %ld DATA\n", file->path, size);

        file->remotesize += bytes;
        DEBUGFILE(file);
        PTH(err, pthread_cond_broadcast(&(file->isempty)), netpipefs_file_unlock(file); return -1)
    }
    NOTZERO(netpipefs_file_unlock(file), return -1)

    return bytes;
}

int netpipefs_file_recv(struct netpipefs_file *file) {
    int err;
    void *buf = NULL;
    int size = socket_read_h(netpipefs_socket.fd_skt, &buf);
    if (size <= 0) return size;

    NOTZERO(netpipefs_file_lock(file), free(buf); return -1)

    while (cbuf_size(file->buffer) + size > netpipefs_options.pipecapacity && file->readers > 0) { //TODO block if the pipe is full but send portions of data
        fprintf(stderr, "cannot write locally: file size %ld < %d. Something is wrong!\n", cbuf_size(file->buffer), size);
        PTH(err, pthread_cond_wait(&(file->isfull), &(file->mtx)), netpipefs_file_unlock(file); free(buf); return -1)
    }

    // return -1 and sets errno to EPIPE if there are no readers
    if (file->readers == 0) {
        errno = EPIPE;
        netpipefs_file_unlock(file);
        free(buf);
        return -1;
    }

    size_t dataput = cbuf_put(file->buffer, buf, size);
    free(buf);
    if (dataput != size) {
        fprintf(stderr, "put less data than required\n");
        netpipefs_file_unlock(file);
        return -1;
    }

    DEBUGFILE(file);
    PTH(err, pthread_cond_broadcast(&(file->isempty)), netpipefs_file_unlock(file); return -1)
    NOTZERO(netpipefs_file_unlock(file), return -1)

    return size;
}

int netpipefs_file_read(struct netpipefs_file *file, char *buf, size_t size) {
    int err, bytes_wrote;
    char *bufptr = buf;

    NOTZERO(netpipefs_file_lock(file), return -1)

    /* file has not enough data */
    while(cbuf_size(file->buffer) < size && file->writers > 0) {
        fprintf(stderr, "cannot read: file size %ld < %ld\n", cbuf_size(file->buffer), size);
        PTH(err, pthread_cond_wait(&(file->isempty), &(file->mtx)), netpipefs_file_unlock(file); return -1)
    }

    /* return EOF if there are no writers */
    if (file->writers == 0) {
        NOTZERO(netpipefs_file_unlock(file), return -1)
        return 0; //EOF
    }

    /* read from pipe */
    size_t datagot = cbuf_get(file->buffer, bufptr, size);
    if (datagot != size) {
        fprintf(stderr, "read less data than required\n");
        netpipefs_file_unlock(file);
        return -1;
    }

    /* wake up waiting writers */
    PTH(err, pthread_cond_broadcast(&(file->isfull)), netpipefs_file_unlock(file); return -1)

    PTH(err, pthread_mutex_lock(&(netpipefs_socket.writemtx)), netpipefs_file_unlock(file); return -1)
    bytes_wrote = write_socket_message(netpipefs_socket.fd_skt, READ, file->path, -1);
    if (bytes_wrote > 0) {
        bytes_wrote = writen(netpipefs_socket.fd_skt, &size, sizeof(size_t));
    }
    PTH(err, pthread_mutex_unlock(&(netpipefs_socket.writemtx)), netpipefs_file_unlock(file); return -1)
    if (bytes_wrote > 0) {
        DEBUG("sent: READ %s %ld\n", file->path, size);
        DEBUGFILE(file);
    }

    NOTZERO(netpipefs_file_unlock(file), return -1)
    if (bytes_wrote <= 0) return -1;

    return datagot;
}

int netpipefs_file_read_update(struct netpipefs_file *file, size_t size) {
    int err;
    NOTZERO(netpipefs_file_lock(file), return -1)

    /* update remote size and wake up writers */
    file->remotesize -= size;
    PTH(err, pthread_cond_broadcast(&(file->isfull)), netpipefs_file_unlock(file); return -1)
    DEBUGFILE(file);

    NOTZERO(netpipefs_file_unlock(file), return -1)

    return 0;
}


int netpipefs_file_close(struct netpipefs_file *file, int mode) {
    int bytes, err, free_memory = 0;

    NOTZERO(netpipefs_file_lock(file), return -1)

    if (mode == O_WRONLY) {
        file->writers--;
        if (file->writers == 0) PTH(err, pthread_cond_broadcast(&(file->isempty)), netpipefs_file_unlock(file); return -1)
    } else if (mode == O_RDONLY) {
        file->readers--;
        if (file->readers == 0) PTH(err, pthread_cond_broadcast(&(file->isfull)), netpipefs_file_unlock(file); return -1)
    }

    DEBUGFILE(file);
    if (file->writers == 0 && file->readers == 0) {
        free_memory = 1;
    }

    PTH(err, pthread_mutex_lock(&(netpipefs_socket.writemtx)), return -1)
    bytes = write_socket_message(netpipefs_socket.fd_skt, CLOSE, file->path, mode);
    PTH(err, pthread_mutex_unlock(&(netpipefs_socket.writemtx)), return -1)
    if (bytes <= 0) return bytes;

    DEBUG("sent: CLOSE %s %d\n", file->path, mode);

    if (free_memory) {
        MINUS1(netpipefs_remove_open_file(file->path), err = -1)
        NOTZERO(netpipefs_file_unlock(file), err = -1)
        MINUS1(netpipefs_file_free(file), err = -1)
    } else {
        NOTZERO(netpipefs_file_unlock(file), err = -1)
    }

    if (err == -1) return -1;

    return bytes; // > 0
}

int netpipefs_file_close_update(struct netpipefs_file *file, int mode) {
    int err;

    NOTZERO(netpipefs_file_lock(file), return -1)

    if (mode == O_WRONLY) {
        file->writers--;
        if (file->writers == 0) PTH(err, pthread_cond_broadcast(&(file->isempty)), netpipefs_file_unlock(file); return -1)
    } else if (mode == O_RDONLY) {
        file->readers--;
        if (file->readers == 0) PTH(err, pthread_cond_broadcast(&(file->isfull)), netpipefs_file_unlock(file); return -1)
    }

    DEBUGFILE(file);
    if (file->writers == 0 && file->readers == 0) {
        MINUS1(netpipefs_remove_open_file(file->path), err = -1)
        NOTZERO(netpipefs_file_unlock(file), err = -1)
        MINUS1(netpipefs_file_free(file), err = -1)
    } else {
        NOTZERO(netpipefs_file_unlock(file), err = -1)
    }

    if (err == -1) return -1;

    return 0;
}