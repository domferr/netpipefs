#define FUSE_USE_VERSION 26
#define DEBUGGING 1
#include <fuse.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include "../include/utils.h"

static char *filepath = NULL;
static char *filecontent = NULL;

/**
 * Initialize filesystem
 *
 * The return value will passed in the private_data field of
 * fuse_context to all file operations and as a parameter to the
 * destroy() method.
 *
 * Introduced in version 2.3
 * Changed in version 2.6
 */
// Undocumented but extraordinarily useful fact:  the fuse_context is
// set up before this function is called, and
// fuse_get_context()->private_data returns the user_data passed to
// fuse_main().  Really seems like either it should be a third
// parameter coming in here, or else the fact should be documented
// (and this might as well return void, as it did in older versions of
// FUSE).
static void* init_callback(struct fuse_conn_info *conn)
{
    DEBUG("init() callback\n")
    return 0;
}

/**
 * Clean up filesystem
 *
 * Called on filesystem exit.
 *
 * Introduced in version 2.3
 */
static void destroy_callback(void *privatedata)
{
    DEBUG("destroy() callback\n")
    if (filepath) free(filepath);
    if (filecontent) free(filecontent);
}

/** Get file attributes.
 *
 * Similar to stat().  The 'st_dev' and 'st_blksize' fields are
 * ignored.	 The 'st_ino' field is ignored except if the 'use_ino'
 * mount option is given.
 */
static int getattr_callback(const char *path, struct stat *stbuf) {
    memset(stbuf, 0, sizeof(struct stat));

    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        return 0;
    }

    if (filepath != NULL && strcmp(path, filepath) == 0) {
        stbuf->st_mode = S_IFREG | 0777;
        stbuf->st_nlink = 1;
        stbuf->st_size = filecontent? strlen(filecontent):0;
        return 0;
    }

    return -ENOENT;
}

/** Read data from an open file
 *
 * Read should return exactly the number of bytes requested except
 * on EOF or error, otherwise the rest of the data will be
 * substituted with zeroes.	 An exception to this is when the
 * 'direct_io' mount option is specified, in which case the return
 * value of the read system call will reflect the return value of
 * this operation.
 */
static int read_callback(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    if (strcmp(path, filepath) == 0) {
        size_t len = strlen(filecontent);
        if (offset >= len) {
            return 0;
        }

        if (offset + size > len) {
            memcpy(buf, filecontent + offset, len - offset);
            return len - offset;
        }

        memcpy(buf, filecontent + offset, size);
        return size;
    }

    return -ENOENT;
}

/** Write data to an open file
 *
 * Write should return exactly the number of bytes requested
 * except on error.	 An exception to this is when the 'direct_io'
 * mount option is specified (see read operation).
 *
 * Changed in version 2.2
 */
static int write_callback(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    //writes up to size bytes from the buffer starting at buf to the file pointed by path at offset offset.
    if (strcmp(path, filepath) == 0) {
        size_t cpy_len = strlen(buf);
        if (size < cpy_len) {
            cpy_len = size;
        }

        if (filecontent) free(filecontent);
        filecontent = malloc(sizeof(char)*(cpy_len+1));
        if (filecontent == NULL) return -errno;
        memcpy(filecontent, buf, cpy_len);
        filecontent[cpy_len] = '\0';
        DEBUG("Sending %s", filecontent)
        return cpy_len;
    }

    return -ENOENT;
}

/** File open operation
 *
 * No creation (O_CREAT, O_EXCL) and by default also no
 * truncation (O_TRUNC) flags will be passed to open(). If an
 * application specifies O_TRUNC, fuse first calls truncate()
 * and then open(). Only if 'atomic_o_trunc' has been
 * specified and kernel version is 2.6.24 or later, O_TRUNC is
 * passed on to open.
 *
 * Unless the 'default_permissions' mount option is given,
 * open should check if the operation is permitted for the
 * given flags. Optionally open may also return an arbitrary
 * filehandle in the fuse_file_info structure, which will be
 * passed to all file operations.
 */
static int open_callback(const char *path, struct fuse_file_info *fi) {
    //The file does not exist so create_callback was called before
    if (filepath != NULL && strcmp(path, filepath) != 0) {
        free(filepath);
        DEBUG("filepath freed\n")
    }

    if (filepath == NULL) {
        filepath = strdup(path);
        DEBUG("filepath set to: %s\n", filepath)
    }

    return 0;
}

/**
 * Create and open a file
 *
 * If the file does not exist, first create it with the specified
 * mode, and then open it.
 *
 * If this method is not implemented or under Linux kernel
 * versions earlier than 2.6.15, the mknod() and open() methods
 * will be called instead.
 */
static int create_callback(const char *path, mode_t mode, struct fuse_file_info *fi) {
    DEBUG("create_callback() -> %s\n", path)
    //Let's remember the filepath instead of creating it
    if (filepath && strcmp(path, filepath) != 0) {
        free(filepath);
    }
    if (!filepath)
        filepath = strdup(path);
    return 0;
}

/** Release an open file
 *
 * Release is called when there are no more references to an open
 * file: all file descriptors are closed and all memory mappings
 * are unmapped.
 *
 * For every open() call there will be exactly one release() call
 * with the same flags and file descriptor.	 It is possible to
 * have a file opened more than once, in which case only the last
 * release will mean, that no more reads/writes will happen on the
 * file.  The return value of release is ignored.
 *
 * Changed in version 2.2
 */
static int release_callback(const char *path, struct fuse_file_info *fi) {
    if (filepath && strcmp(path, filepath) == 0) {
        free(filepath);
        filepath = NULL;
        if (filecontent) {
            free(filecontent);
            filecontent = NULL;
        }
    }
    return 0;
}

/** Change the size of a file
 *
 * (It is ignored but it seems to be required..)
 */
static int truncate_callback(const char *path, off_t offset) {
    return 0;
}

static struct fuse_operations my_operations = {
    .init = init_callback,
    .destroy = destroy_callback,
    .getattr = getattr_callback,
    .open = open_callback,
    .create = create_callback,
    .read = read_callback,
    .write = write_callback,
    .release = release_callback,
    .truncate = truncate_callback
};

int main(int argc, char** argv) {
    printf("Learning FUSE...\n");
    int fuse_stat = fuse_main(argc, argv, &my_operations, NULL);
    DEBUG("fuse_main returned %d\n", fuse_stat)
    //TODO handle program end
    return fuse_stat;
}
