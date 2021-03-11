#ifndef FSPIPE_FILE_H
#define FSPIPE_FILE_H

#include <pthread.h>
#include "../include/options.h"
#define DEBUGFILE(file) DEBUG("   %s: %d readers, %d writers, %ld bytes\n", file->path, file->readers, file->writers, file->size)

struct fspipe_file {
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
struct fspipe_file *fspipe_file_alloc(const char *path);

/**
 * Frees the memory allocated for the given file.
 *
 * @param file the file structure
 *
 * @return 0 on success, -1 on error and it sets errno
 */
int fspipe_file_free(struct fspipe_file *file);

/**
 * Lock the given file
 *
 * @param file file to be locked
 *
 * @return 0 on success, -1 on error and sets errno
 */
int fspipe_file_lock(struct fspipe_file *file);

/**
 * Unlock the given file
 *
 * @param file file to be unlocked
 *
 * @return 0 on success, -1 on error and sets errno
 */
int fspipe_file_unlock(struct fspipe_file *file);

#endif //FSPIPE_FILE_H
