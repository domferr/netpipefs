#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <poll.h>
#include "../include/netpipe.h"
#include "../include/utils.h"
#include "../include/openfiles.h"
#include "../include/netpipefs_socket.h"

#define NOT_OPEN -1

extern struct netpipefs_socket netpipefs_socket;

/** Linked list of poll handles */
struct poll_handle {
    void *ph;
    struct poll_handle *next;
};

typedef struct netpipe_req {
    char *buf;
    size_t bytes_processed;
    size_t size;
    struct netpipe_req *next;
    int error;
} netpipe_req_t;

static netpipe_req_t *netpipe_add_request(struct netpipe *file, char *buf, size_t size, int mode) {
    netpipe_req_t *new_req = (netpipe_req_t *) malloc(sizeof(netpipe_req_t));
    if (new_req == NULL) return NULL;

    new_req->size = size;
    new_req->buf = buf;
    new_req->bytes_processed = 0;
    new_req->error = 0;

    if (mode == O_RDONLY) {
        new_req->next = file->rd_req;
        file->rd_req = new_req;
    } else {
        new_req->next = file->wr_req;
        file->wr_req = new_req;
    }

    return new_req;
}

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

    if ((err = pthread_cond_init(&(file->wr), NULL)) != 0) {
        errno = err;
        pthread_cond_destroy(&(file->canopen));
        goto error;
    }

    if ((err = pthread_cond_init(&(file->rd), NULL)) != 0) {
        errno = err;
        pthread_cond_destroy(&(file->canopen));
        pthread_cond_destroy(&(file->wr));
        goto error;
    }

    file->buffer = cbuf_alloc(netpipefs_options.pipecapacity);
    if (file->buffer == NULL) {
        pthread_cond_destroy(&(file->canopen));
        pthread_cond_destroy(&(file->wr));
        pthread_cond_destroy(&(file->rd));
        goto error;
    }

    file->open_mode = NOT_OPEN;
    file->force_exit = 0;
    file->writers = 0;
    file->readers = 0;
    file->remotesize = 0;
    file->remotecapacity = netpipefs_socket.remotepipecapacity;
    file->poll_handles = NULL;
    file->wr_req = NULL;
    file->rd_req = NULL;

    return file;

error:
    free((void*) file->path);
    pthread_mutex_destroy(&(file->mtx));
    free(file);
    return NULL;
}

int netpipe_free(struct netpipe *file) {
    int ret = 0, err;

    cbuf_free(file->buffer);
    free((void*) file->path);

    struct poll_handle *ph = file->poll_handles;
    struct poll_handle *oldph;
    while(ph != NULL) {
        netpipefs_poll_destroy(ph->ph);
        oldph = ph;
        ph = ph->next;
        free(oldph);
    }

    /* free pending read requests */
    netpipe_req_t *rd_list = file->rd_req;
    netpipe_req_t *oldreq;
    while(rd_list != NULL) {
        oldreq = rd_list;
        rd_list = rd_list->next;
        free(oldreq);
    }

    /* free pending write requests */
    netpipe_req_t *wr_list = file->wr_req;
    while(wr_list != NULL) {
        oldreq = wr_list;
        wr_list = wr_list->next;
        free(oldreq);
    }

    if ((err = pthread_mutex_destroy(&(file->mtx))) != 0) { errno = err; ret = -1; }
    if ((err = pthread_cond_destroy(&(file->canopen))) != 0) { errno = err; ret = -1; }
    if ((err = pthread_cond_destroy(&(file->wr))) != 0) { errno = err; ret = -1; }
    if ((err = pthread_cond_destroy(&(file->rd))) != 0) { errno = err; ret = -1; }

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

    /* both read and write access is not allowed */
    if (mode == O_RDWR) {
        errno = EPERM;
        return NULL;
    }

    /* get the file struct or create it */
    file = netpipefs_get_or_create_open_file(path, &just_created);
    if (file == NULL) return NULL;

    NOTZERO(netpipe_lock(file), goto error)

    if (file->force_exit) {
        errno = ENOENT;
        goto error;
    }

    if (file->open_mode != -1 && file->open_mode != mode) {
        errno = EPERM;
        goto error;
    }

    /* Update readers and writers */
    if (mode == O_RDONLY) file->readers++;
    else if (mode == O_WRONLY) file->writers++;

    if (nonblock && (file->readers == 0 || file->writers == 0)) {
        errno = EAGAIN;
        goto undo_open;
    }
    DEBUGFILE(file);

    /* Notify who's waiting for readers/writers */
    PTH(err, pthread_cond_broadcast(&(file->canopen)), goto undo_open)

    bytes = send_open_message(&netpipefs_socket, path, mode);
    if (bytes <= 0) { // cannot write over socket
        goto undo_open;
    }

    file->open_mode = mode;
    /* Wait for at least one writer and one reader */
    while (!file->force_exit && (file->readers == 0 || file->writers == 0)) {
        PTH(err, pthread_cond_wait(&(file->canopen), &(file->mtx)), goto undo_open)
    }

    if (file->force_exit) {
        errno = ENOENT;
        goto undo_open;
    }

    NOTZERO(netpipe_unlock(file), goto undo_open)

    return file;

undo_open:
    if (mode == O_RDONLY) {
        file->readers--;
        if (file->readers == 0) file->open_mode = NOT_OPEN;
    } else if (mode == O_WRONLY) {
        file->writers--;
        if (file->writers == 0) file->open_mode = NOT_OPEN;
    }

error:
    if (just_created) {
        netpipefs_remove_open_file(path);
        netpipe_unlock(file);
        netpipe_free(file); // for sure there is no poll handle
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
        netpipe_free(file); // for sure there is no poll handle
    }
    return NULL;
}

/**
 * Flush data. Be sure to call this function only if there is data available locally and
 * remote host has not a full buffer.
 *
 * @param file the file to be flushed
 * @return how many bytes were flushed or 0 if connection was lost or -1 on error
 */
static ssize_t do_flush(struct netpipe *file) {
    ssize_t datasent;
    size_t available_remote = file->remotecapacity - file->remotesize;
    size_t available_locally = cbuf_size(file->buffer);
    size_t tobesent = available_locally < available_remote ? available_locally:available_remote;

    if (tobesent == 0) {
        errno = EINVAL;
        return 0;
    }

    datasent = send_flush_message(&netpipefs_socket, file, tobesent);
    if (datasent > 0) file->remotesize += datasent;

    return datasent;
}

ssize_t netpipe_flush(struct netpipe *file, int nonblock) {
    int wasfull_locally, err, bytes;
    ssize_t datasent = 0;

    NOTZERO(netpipe_lock(file), return -1)

    if (file->force_exit) {
        errno = EPIPE;
        netpipe_unlock(file);
        return -1;
    }

    while(!cbuf_empty(file->buffer) && file->readers > 0) {
        if (nonblock && !file->force_exit && file->remotecapacity == file->remotesize) {
            if (datasent == 0) {
                // the entire data cannot be written
                datasent = -1; // will return -1
                errno = EAGAIN;
            }
            break;
        }

        wasfull_locally = cbuf_full(file->buffer);
        /* file is full on the remote host but there is local data to be flushed */
        while(!file->force_exit && file->remotecapacity == file->remotesize && !cbuf_empty(file->buffer) && file->readers > 0) {
            PTH(err, pthread_cond_wait(&(file->wr), &(file->mtx)), netpipe_unlock(file); return -1)
        }

        if (file->force_exit) break;

        /* send data */
        if (file->readers > 0 && !cbuf_empty(file->buffer)) {
            bytes = do_flush(file);
            if (bytes <= 0) break;

            datasent += bytes;
            /* wake up waiting writers */
            if (wasfull_locally) PTH(err, pthread_cond_broadcast(&(file->wr)), netpipe_unlock(file); return -1)

            DEBUGFILE(file);
        }
    }

    NOTZERO(netpipe_unlock(file), return -1)

    return datasent;
}

static ssize_t do_send(struct netpipe *file, char *bufptr, size_t size) {
    size_t available_remote, tobesent, datasent;

    available_remote = file->remotecapacity - file->remotesize;
    tobesent = size < available_remote ? size : available_remote;
    if (tobesent == 0) return 0;

    datasent = send_write_message(&netpipefs_socket, file->path, bufptr, tobesent);
    if (datasent > 0) file->remotesize += datasent;

    return datasent;
}

ssize_t netpipe_send(struct netpipe *file, const char *buf, size_t size, int nonblock) {
    int err;
    size_t already_sent;
    ssize_t ret;
    char *bufptr = (char *) buf;

    NOTZERO(netpipe_lock(file), return -1)

    if (file->force_exit) {
        errno = EPIPE;
        netpipe_unlock(file);
        return -1;
    }

    ret = do_send(file, bufptr, size);
    if (ret == -1 || ret == (ssize_t) size || nonblock) {
        if (nonblock && ret == 0) errno = EAGAIN;
        netpipe_unlock(file);
        return ret;
    }

    DEBUG("Sent %ld bytes before sleeping\n", ret);
    bufptr += ret;
    size -= ret;
    already_sent = ret;

    netpipe_req_t *request = netpipe_add_request(file, bufptr, size, O_WRONLY);
    while(!file->force_exit && request->bytes_processed != size && !request->error) {
        PTH(err, pthread_cond_wait(&(file->wr), &(file->mtx)), netpipe_unlock(file); return -1)
    }

    if (request->error) {
        errno = request->error;
        ret = -1;
    } else if (request->bytes_processed == 0 && file->force_exit) {
        errno = EPIPE;
        ret = -1;
    } else {
        ret = request->bytes_processed + already_sent;
    }

    free(request);
    NOTZERO(netpipe_unlock(file), return -1)
    return ret;
}

/**
 * Notify each poll handle that something is changed
 *
 * @param file file which is changed
 * @param poll_notify function used to notify
 */
static void loop_poll_notify(struct netpipe *file) {
    struct poll_handle *currph = file->poll_handles;
    struct poll_handle *oldph;
    while(currph) {
        netpipefs_poll_notify(currph->ph); // caller should free currph->ph
        oldph = currph;
        currph = currph->next;
        free(oldph);
    }
    file->poll_handles = NULL;
}

int netpipe_recv(struct netpipe *file, size_t size) {
    int err, wasempty;

    NOTZERO(netpipe_lock(file), return -1)

    size_t remaining = size;
    ssize_t dataput;
    while (remaining > 0 && file->readers > 0) {
        wasempty = cbuf_empty(file->buffer);

        /* file is full. wait for data */
        while(cbuf_full(file->buffer) && file->readers > 0) {
            DEBUG("cannot write locally: file is full. SOMETHING IS WRONG!\n");
            PTH(err, pthread_cond_wait(&(file->wr), &(file->mtx)), netpipe_unlock(file); return -1)
        }

        /* file is not full */
        if (file->readers > 0) {
            dataput = cbuf_readn(netpipefs_socket.fd, file->buffer, remaining);
            if (dataput <= 0) {
                netpipe_unlock(file);
                return dataput;
            }
            remaining -= dataput;

            /* wake up waiting readers */
            if (wasempty) PTH(err, pthread_cond_broadcast(&(file->rd)), netpipe_unlock(file); return -1)

            DEBUGFILE(file);
        }
    }

    /* return -1 and sets errno to EPIPE if there are no readers */
    if (remaining > 0 && file->readers == 0) {
        errno = EPIPE;
        netpipe_unlock(file);
        return -1;
    }

    loop_poll_notify(file);

    NOTZERO(netpipe_unlock(file), return -1)
    return size;
}

ssize_t netpipe_read(struct netpipe *file, char *buf, size_t size, int nonblock) {
    int err, bytes_wrote, canread;
    char *bufptr = buf;

    NOTZERO(netpipe_lock(file), return -1)
    if (file->force_exit) return 0; // EOF

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
        while(!file->force_exit && cbuf_empty(file->buffer) && file->writers > 0) {
            DEBUG("cannot read: file is empty\n");
            PTH(err, pthread_cond_wait(&(file->rd), &(file->mtx)), netpipe_unlock(file); return -1)
        }

        if (file->force_exit)
            break;

        /* file is not empty */
        if (!cbuf_empty(file->buffer)) {
            datagot = cbuf_get(file->buffer, bufptr, remaining);
            remaining -= datagot;
            bufptr += datagot;

            /* Send READ message */
            bytes_wrote = send_read_message(&netpipefs_socket, file->path, datagot);
            if (bytes_wrote == 0) break;
            if (bytes_wrote == -1) {
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

int netpipe_read_update(struct netpipe *file, size_t size) {
    int err;
    char *bufptr;
    netpipe_req_t *req;
    size_t available_remote, remaining, datasent;

    NOTZERO(netpipe_lock(file), return -1)

    /* update remote size and wake up writers */
    file->remotesize -= size;
    available_remote = file->remotecapacity - file->remotesize;
    req = file->wr_req;
    while (req != NULL && available_remote > 0) {
        DEBUG("bytes_processed=%ld/%ld\n", req->bytes_processed, req->size);
        bufptr = req->buf + req->bytes_processed;
        remaining = req->size - req->bytes_processed;
        datasent = do_send(file, bufptr, remaining);
        if (datasent <= 0) {
            if (datasent == 0) errno = ECONNRESET;
            //TODO what happens if (req->bytes_processed == 0)
            req->error = errno;
            netpipe_unlock(file);
            return datasent;
        }

        req->bytes_processed += datasent;
        if (req->bytes_processed == req->size) {
            PTH(err, pthread_cond_broadcast(&(file->wr)), netpipe_unlock(file); return -1)
            req = req->next;
        }
        available_remote = file->remotecapacity - file->remotesize;
    }
    DEBUGFILE(file);
    file->wr_req = req;
    loop_poll_notify(file);

    NOTZERO(netpipe_unlock(file), return -1)

    return 0;
}

int netpipe_close(struct netpipe *file, int mode) {
    int bytes = 1, err = 0;

    NOTZERO(netpipe_lock(file), return -1)

    if (mode == O_WRONLY) {
        file->writers--;
        if (file->writers == 0) {
            // there are no writers, then flush all data before closing
            /*NOTZERO(netpipe_unlock(file), return -1)
            bytes = netpipe_flush(file, 0);
            if (bytes != -1) DEBUG("Flushed %d bytes before closing\n", bytes);
            NOTZERO(netpipe_lock(file), return -1)*/
        }
    } else if (mode == O_RDONLY) {
        file->readers--;
    }

    DEBUGFILE(file);

    bytes = send_close_message(&netpipefs_socket, file->path, mode);
    if (bytes <= 0)
        err = -1;

    if (file->writers == 0 && file->readers == 0) {
        MINUS1(netpipefs_remove_open_file(file->path), err = -1)
        NOTZERO(netpipe_unlock(file), err = -1)
        MINUS1(netpipe_free(file), err = -1)
    } else {
        NOTZERO(netpipe_unlock(file), err = -1)
    }

    if (err == -1) return -1;

    return bytes; // > 0
}

int netpipe_close_update(struct netpipe *file, int mode) {
    int err;
    netpipe_req_t *req;

    NOTZERO(netpipe_lock(file), return -1)

    if (mode == O_WRONLY) {
        file->writers--;
        if (file->writers == 0)
            PTH(err, pthread_cond_broadcast(&(file->rd)), netpipe_unlock(file); return -1)
    } else if (mode == O_RDONLY) {
        file->readers--;
        if (file->readers == 0) {
            req = file->rd_req;
            while(req != NULL) {
                if (req->bytes_processed == 0)
                    req->error = EPIPE;

                req = req->next;
            }
            file->rd_req = NULL;
            PTH(err, pthread_cond_broadcast(&(file->wr)), netpipe_unlock(file); return -1)
        }
    }

    DEBUGFILE(file);

    loop_poll_notify(file);

    if (file->writers == 0 && file->readers == 0) {
        err = 0;
        MINUS1(netpipefs_remove_open_file(file->path), err = -1)
        MINUS1(netpipe_unlock(file), err = -1)
        MINUS1(netpipe_free(file), err = -1)

        return err;
    }

    NOTZERO(netpipe_unlock(file), return -1)

    return 0;
}

int netpipe_force_exit(struct netpipe *file) {
    int err;

    MINUS1(netpipe_lock(file), return -1)

    file->force_exit = 1;
    PTH(err, pthread_cond_broadcast(&(file->canopen)), netpipe_unlock(file); return -1)
    PTH(err, pthread_cond_broadcast(&(file->rd)), netpipe_unlock(file); return -1)
    PTH(err, pthread_cond_broadcast(&(file->wr)), netpipe_unlock(file); return -1)

    MINUS1(netpipe_unlock(file), return -1)

    return 0;
}

int netpipe_poll(struct netpipe *file, void *ph, unsigned int *reventsp) {
    struct poll_handle *newph = (struct poll_handle *) malloc(sizeof(struct poll_handle));
    if (newph == NULL) return -1;
    newph->ph = ph;

    MINUS1(netpipe_lock(file), free(newph); return -1)

    // add poll handle
    newph->next = file->poll_handles;
    file->poll_handles = newph;

    // readable
    if (!cbuf_empty(file->buffer)) {
        // can read because there is data, no matter how many writers there are
        *reventsp |= POLLIN;
    }
    if (file->writers == 0) { // no data is available, can't read
        *reventsp |= POLLHUP;
    }

    // no readers. cannot write
    if (file->readers == 0) {
        *reventsp |= POLLERR;
    }
    // writable
    if (!cbuf_full(file->buffer)) {
        // buffer isn't full. can write
        *reventsp |= POLLOUT;
    }

    MINUS1(netpipe_unlock(file), return -1)

    return 0;
}