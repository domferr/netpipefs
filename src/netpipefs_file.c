#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include "../include/netpipefs_file.h"
#include "../include/utils.h"
#include "../include/sock.h"
#include "../include/openfiles.h"
#include "../include/scfiles.h"
#include "../include/netpipefs_socket.h"

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

    bytes = send_open_message(&netpipefs_socket, path, mode);

    if (bytes <= 0) { // cannot write over socket
        netpipefs_file_unlock(file);
        goto error;
    }

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
    int err;

    NOTZERO(netpipefs_file_lock(file), return -1)

    size_t remaining = size;
    size_t datasent;
    char *bufptr = (char *) buf;
    while (remaining > 0 && file->readers > 0) {
        /* file is full on the remote host. wait for enough space */
        while(file->remotesize == file->remotecapacity && file->readers > 0) {
            DEBUG("cannot send: remote buffer is full\n");
            PTH(err, pthread_cond_wait(&(file->isfull), &(file->mtx)), netpipefs_file_unlock(file); return -1)
        }

        /* file is not full anymore. can send data */
        if (file->readers > 0) {
            size_t available = file->remotecapacity - file->remotesize;
            size_t tobesent = remaining < available ? remaining:available;
            /* send data via socket */
            datasent = send_write_message(&netpipefs_socket, file->path, bufptr, tobesent);
            if (datasent <= 0) {
                netpipefs_file_unlock(file);
                return datasent;
            }
            remaining -= datasent;
            bufptr += datasent;

            file->remotesize += datasent;
            /* wake up waiting readers */
            PTH(err, pthread_cond_broadcast(&(file->isempty)), netpipefs_file_unlock(file); return -1)
            DEBUGFILE(file);
        }
    }

    /* return -1 and sets errno to EPIPE if there are no readers */
    if (remaining > 0 && file->readers == 0) {
        errno = EPIPE;
        netpipefs_file_unlock(file);
        return -1;
    }

    NOTZERO(netpipefs_file_unlock(file), return -1)

    return size;
}

int netpipefs_file_recv(struct netpipefs_file *file) {
    int err;
    size_t size = 0;
    err = readn(netpipefs_socket.fd_skt, &size, sizeof(size_t));
    if (err <= 0) return err;
    if (size <= 0) return -1;

    NOTZERO(netpipefs_file_lock(file), return -1)

    size_t remaining = size;
    ssize_t dataput;
    while (remaining > 0 && file->readers > 0) {
        /* file is full. wait for data */
        while(cbuf_full(file->buffer) && file->readers > 0) {
            DEBUG("cannot write locally: file is full. SOMETHING IS WRONG!\n");
            PTH(err, pthread_cond_wait(&(file->isfull), &(file->mtx)), netpipefs_file_unlock(file); return -1)
        }

        /* file is not full */
        if (file->readers > 0) {
            dataput = cbuf_readn(netpipefs_socket.fd_skt, file->buffer, remaining);
            if (dataput <= 0) {
                netpipefs_file_unlock(file);
                return -1;
            }
            remaining -= dataput;

            /* wake up waiting readers */
            PTH(err, pthread_cond_broadcast(&(file->isempty)), netpipefs_file_unlock(file); return -1)

            DEBUGFILE(file);
        }
    }

    /* return -1 and sets errno to EPIPE if there are no readers */
    if (remaining > 0 && file->readers == 0) {
        errno = EPIPE;
        netpipefs_file_unlock(file);
        return -1;
    }

    NOTZERO(netpipefs_file_unlock(file), return -1)
    return size;
}

ssize_t netpipefs_file_read(struct netpipefs_file *file, char *buf, size_t size) {
    int err, bytes_wrote, canread;
    char *bufptr = buf;

    NOTZERO(netpipefs_file_lock(file), return -1)

    size_t remaining = size;
    size_t datagot;
    canread = file->writers > 0 || !cbuf_empty(file->buffer);
    while(remaining > 0 && canread) {
        /* file is empty. wait for data */
        while(cbuf_empty(file->buffer) && file->writers > 0) {
            DEBUG("cannot read: file is empty\n");
            PTH(err, pthread_cond_wait(&(file->isempty), &(file->mtx)), netpipefs_file_unlock(file); return -1)
        }

        /* file is not empty */
        if (!cbuf_empty(file->buffer)) {
            datagot = cbuf_get(file->buffer, bufptr, remaining);
            remaining -= datagot;
            bufptr += datagot;

            /* wake up waiting writers */
            PTH(err, pthread_cond_broadcast(&(file->isfull)), netpipefs_file_unlock(file); return -1)

            /* Send READ message */
            bytes_wrote = send_read_message(&netpipefs_socket, file->path, datagot);
            if (bytes_wrote <= 0) {
                netpipefs_file_unlock(file);
                return -1;
            }

            DEBUGFILE(file);
        }

        canread = file->writers > 0 || !cbuf_empty(file->buffer);
    }

    NOTZERO(netpipefs_file_unlock(file), return -1)

    return size - remaining;
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

    bytes = send_close_message(&netpipefs_socket, file->path, mode);
    if (bytes <= 0) return bytes;

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