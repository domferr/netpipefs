#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <poll.h>
#include "../include/netpipe.h"
#include "../include/utils.h"
#include "../include/openfiles.h"
#include "../include/scfiles.h"
#include "../include/netpipefs_socket.h"

extern struct netpipefs_socket netpipefs_socket;

struct netpipe *netpipe_alloc(const char *path) {
    int err;
    struct netpipe *file = (struct netpipe *) malloc(sizeof(struct netpipe));
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
    file->poll_handles = NULL;

    return file;

error:
    free((void*) file->path);
    pthread_mutex_destroy(&(file->mtx));
    free(file);
    return NULL;
}

int netpipe_free(struct netpipe *file, void (*free_pollhandle)(void *)) {
    int ret = 0, err;

    if ((err = pthread_mutex_destroy(&(file->mtx))) != 0) { errno = err; ret = -1; }
    if ((err = pthread_cond_destroy(&(file->canopen))) != 0) { errno = err; ret = -1; }
    if ((err = pthread_cond_destroy(&(file->isfull))) != 0) { errno = err; ret = -1; }
    if ((err = pthread_cond_destroy(&(file->isempty))) != 0) { errno = err; ret = -1; }
    cbuf_free(file->buffer);
    free((void*) file->path);

    struct poll_handle *ph = file->poll_handles;
    struct poll_handle *oldph;
    while(ph != NULL) {
        if (free_pollhandle) free_pollhandle(ph->ph);
        oldph = ph;
        ph = ph->next;
        free(oldph);
    }

    free(file);

    return ret;
}

int netpipe_lock(struct netpipe *file) {
    int err = pthread_mutex_lock(&(file->mtx));
    if (err != 0) errno = err;
    return err;
}

int netpipe_unlock(struct netpipe *file) {
    int err = pthread_mutex_unlock(&(file->mtx));
    if (err != 0) errno = err;
    return err;
}

struct netpipe *netpipe_open(const char *path, int mode, int nonblock) {
    int err, bytes, just_created = 0;
    struct netpipe *file;

    // both read and write access is not allowed
    if (mode == O_RDWR) {
        errno = EPERM;
        return NULL;
    }

    /* get the file struct or create it */
    file = netpipefs_get_or_create_open_file(path, &just_created);
    if (file == NULL) return NULL;

    NOTZERO(netpipe_lock(file), goto error)

    /* update readers and writers and notify who's waiting for readers/writers */
    if (mode == O_RDONLY) file->readers++;
    else if (mode == O_WRONLY) file->writers++;
    if (nonblock && (file->readers == 0 || file->writers == 0)) {
        errno = EAGAIN;
        netpipe_unlock(file);
        goto unopen;
    }
    DEBUGFILE(file);
    PTH(err, pthread_cond_broadcast(&(file->canopen)), netpipe_unlock(file); goto unopen)

    bytes = send_open_message(&netpipefs_socket, path, mode);

    if (bytes <= 0) { // cannot write over socket
        netpipe_unlock(file);
        goto unopen;
    }

    /* wait for at least one writer and one reader */
    while (file->readers == 0 || file->writers == 0) {
        PTH(err, pthread_cond_wait(&(file->canopen), &(file->mtx)), netpipe_unlock(file); goto unopen)
    }

    NOTZERO(netpipe_unlock(file), goto unopen)

    return file;

unopen:
    if (mode == O_RDONLY) file->readers--;
    else if (mode == O_WRONLY) file->writers--;
error:
    if (just_created) {
        netpipefs_remove_open_file(path);
        netpipe_free(file, NULL); // for sure there is no poll handle
    }
    return NULL;
}

struct netpipe *netpipe_open_update(const char *path, int mode) {
    int err, just_created = 0;
    struct netpipe *file;

    if (mode == O_RDWR) {
        errno = EPERM;
        return NULL;
    }

    file = netpipefs_get_or_create_open_file(path, &just_created);
    if (file == NULL) return NULL;

    NOTZERO(netpipe_lock(file), goto error)

    if (mode == O_RDONLY) file->readers++;
    else if (mode == O_WRONLY) file->writers++;
    DEBUGFILE(file);
    PTH(err, pthread_cond_broadcast(&(file->canopen)), netpipe_unlock(file); goto error)

    NOTZERO(netpipe_unlock(file), goto error)

    return file;

    error:
    if (just_created) {
        netpipefs_remove_open_file(path);
        netpipe_free(file, NULL); // for sure there is no poll handle
    }
    return NULL;
}

ssize_t netpipe_send(struct netpipe *file, const char *buf, size_t size, int nonblock) {
    int err;

    NOTZERO(netpipe_lock(file), return -1)

    size_t remaining = size;
    size_t datasent;
    char *bufptr = (char *) buf;
    while (remaining > 0 && file->readers > 0) {
        if (nonblock && file->remotesize == file->remotecapacity) {
            // buffer is full and the entire data cannot be written
            if (remaining == size) {
                remaining += 1; // will return -1 (size - size - 1)
                errno = EAGAIN;
            }
            // else: buffer is full and some data has been written
            break;
        }
        /* file is full on the remote host. wait for enough space */
        while(file->remotesize == file->remotecapacity && file->readers > 0) {
            DEBUG("cannot send: remote buffer is full\n");
            PTH(err, pthread_cond_wait(&(file->isfull), &(file->mtx)), netpipe_unlock(file); return -1)
        }

        /* file is not full anymore. can send data */
        if (file->readers > 0) {
            size_t available = file->remotecapacity - file->remotesize;
            size_t tobesent = remaining < available ? remaining:available;
            /* send data via socket */
            datasent = send_write_message(&netpipefs_socket, file->path, bufptr, tobesent);
            if (datasent <= 0) {
                netpipe_unlock(file);
                return datasent;
            }
            remaining -= datasent;
            bufptr += datasent;

            file->remotesize += datasent;
            /* wake up waiting readers */ // TODO this is probably not needed
            PTH(err, pthread_cond_broadcast(&(file->isempty)), netpipe_unlock(file); return -1)
            DEBUGFILE(file);
        }
    }

    /* return -1 and sets errno to EPIPE if there are no readers */
    if (remaining == size && file->readers == 0) {
        errno = EPIPE;
        netpipe_unlock(file);
        return -1;
    }

    NOTZERO(netpipe_unlock(file), return -1)

    return size - remaining;
}

static void loop_poll_notify(struct netpipe *file, void (*poll_notify)(void *)) {
    if (!poll_notify) return;

    struct poll_handle *currph = file->poll_handles;
    struct poll_handle *oldph;
    while(currph) {
        poll_notify(currph->ph); // caller should free currph->ph
        oldph = currph;
        currph = currph->next;
        free(oldph);
    }
    file->poll_handles = NULL;
}

int netpipe_recv(struct netpipe *file, void (*poll_notify)(void *)) {
    int err;
    size_t size = 0;
    err = readn(netpipefs_socket.fd, &size, sizeof(size_t));
    if (err <= 0) return err;
    if (size <= 0) return -1;

    NOTZERO(netpipe_lock(file), return -1)

    size_t remaining = size;
    ssize_t dataput;
    int wasempty = cbuf_empty(file->buffer);
    while (remaining > 0 && file->readers > 0) {
        /* file is full. wait for data */
        while(cbuf_full(file->buffer) && file->readers > 0) {
            DEBUG("cannot write locally: file is full. SOMETHING IS WRONG!\n");
            PTH(err, pthread_cond_wait(&(file->isfull), &(file->mtx)), netpipe_unlock(file); return -1)
        }

        /* file is not full */
        if (file->readers > 0) {
            dataput = cbuf_readn(netpipefs_socket.fd, file->buffer, remaining);
            if (dataput <= 0) {
                netpipe_unlock(file);
                return -1;
            }
            remaining -= dataput;

            /* wake up waiting readers */
            if (wasempty) PTH(err, pthread_cond_broadcast(&(file->isempty)), netpipe_unlock(file); return -1)

            DEBUGFILE(file);
        }
    }

    /* return -1 and sets errno to EPIPE if there are no readers */
    if (remaining > 0 && file->readers == 0) {
        errno = EPIPE;
        netpipe_unlock(file);
        return -1;
    }

    loop_poll_notify(file, poll_notify);

    NOTZERO(netpipe_unlock(file), return -1)
    return size;
}

ssize_t netpipe_read(struct netpipe *file, char *buf, size_t size, int nonblock) {
    int err, bytes_wrote, canread;
    char *bufptr = buf;

    NOTZERO(netpipe_lock(file), return -1)

    size_t remaining = size;
    size_t datagot;
    canread = file->writers > 0 || !cbuf_empty(file->buffer);
    while(remaining > 0 && canread) {
        if (nonblock && cbuf_empty(file->buffer) && file->writers > 0) {
            // buffer is empty and the entire data cannot be read
            if (remaining == size) {
                remaining += 1; // will return -1 (size - size - 1)
                errno = EAGAIN;
            }
            // else: buffer is empty and some data has been read
            break;
        }
        /* file is empty. wait for data */
        while(cbuf_empty(file->buffer) && file->writers > 0) {
            DEBUG("cannot read: file is empty\n");
            PTH(err, pthread_cond_wait(&(file->isempty), &(file->mtx)), netpipe_unlock(file); return -1)
        }

        /* file is not empty */
        if (!cbuf_empty(file->buffer)) {
            datagot = cbuf_get(file->buffer, bufptr, remaining);
            remaining -= datagot;
            bufptr += datagot;

            /* wake up waiting writers */ // TODO this is not needed
            PTH(err, pthread_cond_broadcast(&(file->isfull)), netpipe_unlock(file); return -1)

            /* Send READ message */
            bytes_wrote = send_read_message(&netpipefs_socket, file->path, datagot);
            if (bytes_wrote <= 0) {
                netpipe_unlock(file);
                return -1;
            }

            DEBUGFILE(file);
        }

        canread = file->writers > 0 || !cbuf_empty(file->buffer);
    }

    NOTZERO(netpipe_unlock(file), return -1)

    return size - remaining;
}

int netpipe_read_update(struct netpipe *file, size_t size, void (*poll_notify)(void *)) {
    int err, wasfull;

    NOTZERO(netpipe_lock(file), return -1)

    /* update remote size and wake up writers */
    wasfull = file->remotesize == file->remotecapacity;
    file->remotesize -= size;
    if (wasfull) PTH(err, pthread_cond_broadcast(&(file->isfull)), netpipe_unlock(file); return -1)
    DEBUGFILE(file);
    loop_poll_notify(file, poll_notify);

    NOTZERO(netpipe_unlock(file), return -1)

    return 0;
}

int netpipe_close(struct netpipe *file, int mode, void (*free_pollhandle)(void *)) {
    int bytes, err, free_memory = 0;

    NOTZERO(netpipe_lock(file), return -1)

    if (mode == O_WRONLY) {
        file->writers--;
        if (file->writers == 0) PTH(err, pthread_cond_broadcast(&(file->isempty)), netpipe_unlock(file); return -1)
    } else if (mode == O_RDONLY) {
        file->readers--;
        if (file->readers == 0) PTH(err, pthread_cond_broadcast(&(file->isfull)), netpipe_unlock(file); return -1)
    }

    DEBUGFILE(file);
    if (file->writers == 0 && file->readers == 0) {
        free_memory = 1;
    }

    bytes = send_close_message(&netpipefs_socket, file->path, mode);
    if (bytes <= 0) return bytes;

    if (free_memory) {
        MINUS1(netpipefs_remove_open_file(file->path), err = -1)
        NOTZERO(netpipe_unlock(file), err = -1)
        MINUS1(netpipe_free(file, free_pollhandle), err = -1)
    } else {
        NOTZERO(netpipe_unlock(file), err = -1)
    }

    if (err == -1) return -1;

    return bytes; // > 0
}

int netpipe_close_update(struct netpipe *file, int mode, void (*poll_notify)(void *), void (*free_pollhandle)(void *)) {
    int err;

    NOTZERO(netpipe_lock(file), return -1)

    if (mode == O_WRONLY) {
        file->writers--;
        if (file->writers == 0) PTH(err, pthread_cond_broadcast(&(file->isempty)), netpipe_unlock(file); return -1)
    } else if (mode == O_RDONLY) {
        file->readers--;
        if (file->readers == 0) PTH(err, pthread_cond_broadcast(&(file->isfull)), netpipe_unlock(file); return -1)
    }

    DEBUGFILE(file);

    loop_poll_notify(file, poll_notify);

    if (file->writers == 0 && file->readers == 0) {
        err = 0;
        MINUS1(netpipefs_remove_open_file(file->path), err = -1)
        MINUS1(netpipe_unlock(file), err = -1)
        MINUS1(netpipe_free(file, free_pollhandle), err = -1)

        return err;
    }

    NOTZERO(netpipe_unlock(file), return -1)

    return 0;
}

int netpipefs_file_poll(struct netpipe *file, void *ph, unsigned int *reventsp) {
    struct poll_handle *newph = (struct poll_handle *) malloc(sizeof(struct poll_handle));
    if (newph == NULL) return -1;
    newph->ph = ph;

    MINUS1(netpipe_lock(file), return -1)

    // add poll handle
    newph->next = file->poll_handles;
    file->poll_handles = newph;

    // readable
    if (!cbuf_empty(file->buffer)) {
        // can read because there is data, no matter how many writers there are
        *reventsp |= POLLIN;
    } else if (file->writers == 0) { // no data is available, can't read
        *reventsp |= POLLHUP;
        DEBUG("pollhup pollhup pollhup pollhup pollhup pollhup pollhup pollhup pollhup pollhup \n");
    }

    // writable
    if (file->readers == 0) { // no readers. cannot write
        *reventsp |= POLLERR;
    } else if (!cbuf_full(file->buffer)) {
        // there is at least one reader and buffer isn't full. can write
        *reventsp |= POLLOUT;
    }

    MINUS1(netpipe_unlock(file), return -1)

    return 0;
}