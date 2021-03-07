#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include "../include/fspipe_file.h"
#include "../include/utils.h"

static struct fspipe_file *fspipeFile = NULL;   //TODO remove this line

static struct fspipe_file *fspipe_file_alloc(const char *path) {
    int err;
    struct fspipe_file *file = (struct fspipe_file *) malloc(sizeof(struct fspipe_file));
    EQNULL(file, return NULL)

    EQNULL(file->path = strdup(path), free(file); return NULL)
    PTH(err, pthread_mutex_init(&(file->mtx), NULL), free((void*)file->path); free(file); return NULL)
    PTH(err, pthread_cond_init(&(file->canopen), NULL), free((void*)file->path); free(file); return NULL)

    return file;
}

static int fspipe_file_free(struct fspipe_file *file) {
    int err;
    free((void*) file->path);
    if ((err = pthread_mutex_destroy(&(file->mtx))) != 0) errno = err;
    if ((err = pthread_cond_destroy(&(file->canopen))) != 0) errno = err;
    free(file);
    return 0;
}

static struct fspipe_file *fspipe_new_file(const char *path) {
    fspipeFile = fspipe_file_alloc(path);
    return fspipeFile;
}

static int fspipe_get_file(const char *path, struct fspipe_file **file_found) {
    *file_found = NULL;
    *file_found = fspipeFile;
    return 0;
}

static int fspipe_remove_file(struct fspipe_file *file) {
    return 0;
}

int fspipe_file_lock(struct fspipe_file *file) {
    int err = pthread_mutex_lock(&(file->mtx));
    errno = err;
    return err;
}

int fspipe_file_unlock(struct fspipe_file *file) {
    int err = pthread_mutex_unlock(&(file->mtx));
    errno = err;
    return err;
}

struct fspipe_file *fspipe_file_open_local(const char *path, int mode) {
    int err;
    struct fspipe_file *file = NULL;

    // both read and write mode is not allowed
    if (mode == O_RDWR) {
        errno = EPERM;
        return NULL;
    }

    //TODO remove the file on error if it was created now
    MINUS1(fspipe_get_file(path, &file), return NULL)
    EQNULL(file, file = fspipe_new_file(path)) // file doesn't exist then create it
    EQNULL(file, return NULL) // failed to create the new file structure

    NOTZERO(fspipe_file_lock(file), return NULL)

    // wait until there is at least one reader and one writer or an error occurs
    while (file->remote_error == 0 && (file->writers == 0 || file->readers == 0)) {
        PTH(err, pthread_cond_wait(&(file->canopen), &(file->mtx)), return NULL)
    }

    // if an error occurred remotely then reset the error flag, set errno and return -1
    if (file->remote_error) {
        errno = file->remote_error;
        file->remote_error = 0;
        return NULL;
    }

    NOTZERO(fspipe_file_unlock(file), return NULL)

    return file;
}

struct fspipe_file *fspipe_file_open_remote(const char *path, int mode, int error) {
    int err;
    struct fspipe_file *file = NULL;

    MINUS1(fspipe_get_file(path, &file), return NULL)
    EQNULL(file, file = fspipe_new_file(path)) // file doesn't exist then create it
    EQNULL(file, return NULL) // failed to create the new file structure

    NOTZERO(fspipe_file_lock(file), return NULL)

    if (error) file->remote_error = error;
    else if (mode == O_RDONLY) file->readers++;
    else if (mode == O_WRONLY) file->writers++;

    if ((err = pthread_cond_broadcast(&(file->canopen))) != 0) errno = err;

    NOTZERO(fspipe_file_unlock(file), return NULL)

    if (err != 0)
        return NULL;
    return file;
}

int fspipe_file_close(struct fspipe_file *file, int mode) {
    NOTZERO(fspipe_file_lock(file), return -1)

    if (mode == O_WRONLY) {
        file->writers--;
        // if (fspipe_file->writers == 0) { close(fspipe_file->fd[WRITE_END]); fd[WRITE_END] = -1; }
    } else if (mode == O_RDONLY) {
        file->readers--;
        // if (fspipe_file->readers == 0) { close(fspipe_file->fd[READ_END]); fd[READ_END] = -1; }
    }

    NOTZERO(fspipe_file_unlock(file), return -1)
    //TODO without any interest in this file do fspipe_remove_file

    return 0;
}

int fspipe_file_close_p(const char *path, int mode) {
    struct fspipe_file *file = NULL;
    MINUS1(fspipe_get_file(path, &file), return -1)
    return fspipe_file_close(file, mode);
}