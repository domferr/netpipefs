#ifndef OPENFILES_H
#define OPENFILES_H

#include "netpipefs_file.h"

/**
 * Initialize the open files table
 *
 * @return 0 on success, -1 on error and sets errno
 */
int netpipefs_open_files_table_init(void);

/**
 * Destroy the open files tables. All the files are freed.
 *
 * @return 0 on success, -1 on error and sets errno
 */
int netpipefs_open_files_table_destroy(void);

struct netpipefs_file *netpipefs_file_open_local(const char *path, int mode);

struct netpipefs_file *netpipefs_file_open_remote(const char *path, int mode);

int netpipefs_file_write_local(const char *path, char *buf, size_t size);

int netpipefs_file_write_remote(struct netpipefs_file *file, const char *path, char *buf, size_t size);

int netpipefs_file_read_local(struct netpipefs_file *file, char *buf, size_t size);

int netpipefs_file_read_remote(const char* path, size_t size);

int netpipefs_file_close_local(struct netpipefs_file *file, int mode);

int netpipefs_file_close_remote(const char *path, int mode);

#endif //OPENFILES_H
