#ifndef OPENFILES_H
#define OPENFILES_H

#include "netpipe.h"

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
struct netpipe *netpipefs_get_open_file(const char *path);

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
struct netpipe *netpipefs_get_or_create_open_file(const char *path, int *just_created);

/**
 * Destroy given poll handle
 * @param ph poll handle to be destroyed
 */
void netpipefs_poll_destroy(void *ph);

/**
 * Notify who is interested to the given poll handle which will be destroyed after notification.
 * @param ph poll handle
 */
void netpipefs_poll_notify(void *ph);

/**
 * Forces all the operations on any netpipe to immediately end.
 *
 * @return 0 on success, -1 on error
 */
int netpipefs_shutdown(void);

#endif //OPENFILES_H
