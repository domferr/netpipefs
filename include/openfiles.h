#ifndef FILEOP_H
#define FILEOP_H

#include "fspipe_file.h"

int fspipe_open_files_table_init(void);

int fspipe_open_files_table_destroy(void);

struct fspipe_file *fspipe_file_open_local(const char *path, int mode);

struct fspipe_file *fspipe_file_open_remote(const char *path, int mode);

int fspipe_file_write_local(const char *path, char *buf, size_t size);

int fspipe_file_write_remote(struct fspipe_file *file, const char *path, char *buf, size_t size);

int fspipe_file_read_local(struct fspipe_file *file, char *buf, size_t size);

int fspipe_file_read_remote(const char* path, size_t size);

int fspipe_file_close_local(struct fspipe_file *file, int mode);

int fspipe_file_close_remote(const char *path, int mode);

#endif //FILEOP_H
