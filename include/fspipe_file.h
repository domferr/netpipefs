#ifndef FSPIPE_FILE_H
#define FSPIPE_FILE_H

struct fspipe_file {
    const char *path;
    int writers;
    int readers;
    pthread_cond_t canopen;
    pthread_mutex_t mtx;
    int remote_error;
};

int fspipe_open_files_table_init(void);
int fspipe_open_files_table_destroy(void);
struct fspipe_file *fspipe_file_open_local(const char *path, int mode);
struct fspipe_file *fspipe_file_open_remote(const char *path, int mode, int error);
int fspipe_file_close(const char *path, int mode);
int fspipe_file_close_p(const char *path, int mode);

#endif //FSPIPE_FILE_H
