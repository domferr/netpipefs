#ifndef NETPIPEFS_FILE_H
#define NETPIPEFS_FILE_H

#include <pthread.h>
#include "../include/options.h"

/** Print debug info about the given file */
#define DEBUGFILE(file) DEBUG("   %s: %d readers, %d writers, %ld bytes\n", file->path, file->readers, file->writers, file->size)

/** Structure for a file in netpipefs */
struct netpipefs_file {
    const char *path;
    int writers;    // number of writers
    int readers;    // number of readers
    int pipefd[2];  // buffer
    pthread_cond_t canopen; // wait for at least one reader and one writer
    size_t size;    // how many bytes there are inside the buffer
    pthread_cond_t isempty; // wait if the buffer is empty
    pthread_cond_t isfull; // wait if the buffer is full
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

#endif //NETPIPEFS_FILE_H
