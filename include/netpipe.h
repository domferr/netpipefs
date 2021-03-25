#ifndef NETPIPEFS_FILE_H
#define NETPIPEFS_FILE_H

#include <pthread.h>
#include "options.h"
#include "cbuf.h"

/** Print debug info about the given file */
#define DEBUGFILE(file) DEBUG("   %s: %d readers, %d writers, %ld local bytes, %ld remote bytes\n", file->path, file->readers, file->writers, cbuf_size(file->buffer), file->remotesize)

#define DEFAULT_PIPE_CAPACITY 4096

struct poll_handle {
    void *ph;
    struct poll_handle *next;
};

/** Structure for a file in netpipefs */
struct netpipe {
    const char *path;
    int writers;    // number of writers
    int readers;    // number of readers
    cbuf_t *buffer;   // circular buffer
    size_t remotesize;  // how many bytes there are inside the remote buffer
    size_t remotecapacity;  // how much is the buffer capacity on the remote side
    struct poll_handle *poll_handles;
    pthread_cond_t canopen; // wait for at least one reader and one writer
    pthread_cond_t isempty; // wait if the buffer is empty
    pthread_cond_t isfull;  // wait if the buffer is full
    pthread_mutex_t mtx;
};

/**
 * Allocates new memory for a new file structure with the given path.
 *
 * @param path file's path
 *
 * @return the created file structure or NULL on error and it sets errno
 */
struct netpipe *netpipe_alloc(const char *path);

/**
 * Frees the memory allocated for the given file.
 *
 * @param file the file structure
 * @param free_pollhandle function used to free each pollhandle
 * @return 0 on success, -1 on error and it sets errno
 */
int netpipe_free(struct netpipe *file);

/**
 * Lock the given file
 *
 * @param file file to be locked
 *
 * @return 0 on success, -1 on error and sets errno
 */
int netpipe_lock(struct netpipe *file);

/**
 * Unlock the given file
 *
 * @param file file to be unlocked
 *
 * @return 0 on success, -1 on error and sets errno
 */
int netpipe_unlock(struct netpipe *file);

struct netpipe *netpipe_open(const char *path, int mode, int nonblock);

struct netpipe *netpipe_open_update(const char *path, int mode);

ssize_t netpipe_send(struct netpipe *file, const char *buf, size_t size, int nonblock);

int netpipe_recv(struct netpipe *file);

ssize_t netpipe_read(struct netpipe *file, char *buf, size_t size, int nonblock);

int netpipe_read_update(struct netpipe *file, size_t size);

int netpipe_close(struct netpipe *file, int mode);

int netpipe_close_update(struct netpipe *file, int mode);

#endif //NETPIPEFS_FILE_H
