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

/**
 * Returns the file structure for the given path or NULL if it doesn't exist
 *
 * @param path file's path
 *
 * @return the file structure or NULL if it doesn't exist
 */
struct netpipefs_file *netpipefs_get_open_file(const char *path);

/**
 * Removes the file with key path from the open file table. The file structure is also freed.
 *
 * @param path file's path
 *
 * @return 0 on success, -1 on error
 */
int netpipefs_remove_open_file(const char *path);

/**
 * Returns the file structure for the given path or NULL if it doesn't exist
 *
 * @param path file's path
 *
 * @return the file structure or NULL if it doesn't exist
 */
struct netpipefs_file *netpipefs_get_or_create_open_file(const char *path, int *just_created);

#endif //OPENFILES_H
