#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include "../include/fspipe_file.h"
#include "../include/utils.h"

#define READ_END 0
#define WRITE_END 1

struct fspipe_file *fspipe_file_alloc(const char *path) {
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

    if ((err = pthread_cond_init(&(file->canopen), NULL)) != 0
        || (err = pthread_cond_init(&(file->isfull), NULL)) != 0
        || (err = pthread_cond_init(&(file->isempty), NULL)) != 0) {
        errno = err;
        free((void*) file->path);
        pthread_mutex_destroy(&(file->mtx));
        free(file);
        return NULL;
    }

    if (pipe(file->pipefd) == -1) {
        free((void*) file->path);
        pthread_cond_destroy(&(file->canopen));
        pthread_cond_destroy(&(file->isfull));
        pthread_cond_destroy(&(file->isempty));
        pthread_mutex_destroy(&(file->mtx));
        free(file);
        return NULL;
    }

    file->writers = 0;
    file->readers = 0;
    file->size = 0;

    return file;
}

int fspipe_file_free(struct fspipe_file *file) {
    int ret = 0, err;
    if ((err = pthread_mutex_destroy(&(file->mtx))) != 0) { errno = err; ret = -1; }
    if ((err = pthread_cond_destroy(&(file->canopen))) != 0) { errno = err; ret = -1; }
    if ((err = pthread_cond_destroy(&(file->isfull))) != 0) { errno = err; ret = -1; }
    if ((err = pthread_cond_destroy(&(file->isempty))) != 0) { errno = err; ret = -1; }
    free((void*) file->path);
    free(file);

    return ret;
}

int fspipe_file_lock(struct fspipe_file *file) {
    int err = pthread_mutex_lock(&(file->mtx));
    if (err != 0) errno = err;
    return err;
}

int fspipe_file_unlock(struct fspipe_file *file) {
    int err = pthread_mutex_unlock(&(file->mtx));
    if (err != 0) errno = err;
    return err;
}