#ifndef NETPIPEFS_FILE_H
#define NETPIPEFS_FILE_H

#include <pthread.h>
#include "options.h"
#include "cbuf.h"

/** Print debug info about the given file */
#define DEBUGFILE(file) DEBUG("   %s: %d readers, %d writers, %ld local bytes, %ld remote bytes\n", file->path, file->readers, file->writers, cbuf_size(file->buffer), file->remotesize)

#define DEFAULT_PIPE_CAPACITY 4096

/** Structure for a file in netpipefs */
struct netpipefs_file {
    const char *path;
    int writers;    // number of writers
    int readers;    // number of readers
    cbuf_t *buffer;   // circular buffer
    size_t remotesize;  // how many bytes there are inside the remote buffer
    size_t remotecapacity;  // how much is the buffer capacity on the remote side
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
struct netpipefs_file *netpipefs_file_alloc(const char *path);

/**
 * Frees the memory allocated for the given file.
 *
 * @param file the file structure
 *
 * @return 0 on success, -1 on error and it sets errno
 */
int netpipefs_file_free(struct netpipefs_file *file);

/**
 * Lock the given file
 *
 * @param file file to be locked
 *
 * @return 0 on success, -1 on error and sets errno
 */
int netpipefs_file_lock(struct netpipefs_file *file);

/**
 * Unlock the given file
 *
 * @param file file to be unlocked
 *
 * @return 0 on success, -1 on error and sets errno
 */
int netpipefs_file_unlock(struct netpipefs_file *file);

struct netpipefs_file *netpipefs_file_open(const char *path, int mode, int nonblock);

struct netpipefs_file *netpipefs_file_open_update(const char *path, int mode);

ssize_t netpipefs_file_send(struct netpipefs_file *file, const char *buf, size_t size, int nonblock);

int netpipefs_file_recv(struct netpipefs_file *file);

ssize_t netpipefs_file_read(struct netpipefs_file *file, char *buf, size_t size, int nonblock);

int netpipefs_file_read_update(struct netpipefs_file *file, size_t size);

int netpipefs_file_close(struct netpipefs_file *file, int mode);

int netpipefs_file_close_update(struct netpipefs_file *file, int mode);

#endif //NETPIPEFS_FILE_H
