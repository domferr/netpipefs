#ifndef FSPIPE_FILE_H
#define FSPIPE_FILE_H

struct fspipe_file {
    const char *path;
    int writers;    // number of writers
    int readers;    // number of readers
    int pipefd[2];  // buffer
    pthread_cond_t canopen; // wait for at least one reader and one writer
    pthread_mutex_t mtx;
    int remote_error;   // an error occurred on remote side
};

int fspipe_open_files_table_init(void);
int fspipe_open_files_table_destroy(void);
struct fspipe_file *fspipe_file_open_local(const char *path, int mode);
struct fspipe_file *fspipe_file_open_remote(const char *path, int mode, int error);
int fspipe_file_close(const char *path, int mode);
int fspipe_file_write(const char *path, char *buf, size_t size);
int fspipe_file_read(const char *path, char *buf, size_t size);

#endif //FSPIPE_FILE_H
