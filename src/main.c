#include "../include/options.h"
#include <fuse.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "../include/signal_handler.h"
#include "../include/utils.h"
#include "../include/dispatcher.h"
#include "../include/netpipe.h"
#include "../include/openfiles.h"
#include "../include/netpipefs_socket.h"

/* Socket communication */
struct netpipefs_socket netpipefs_socket;

/**
 * Initialize filesystem
 *
 * The return value will passed in the `private_data` field of
 * `struct fuse_context` to all file operations, and as a
 * parameter to the destroy() method. It overrides the initial
 * value provided to fuse_main() / fuse_new().
 */
static void* init_callback(struct fuse_conn_info *conn) {
    /* Useful fact: the fuse_context is set up before this function is called, and fuse_get_context()->private_data
     * returns the user_data passed to fuse_main(). */
    struct fuse *fuse = fuse_get_context()->fuse;
    int err;
    if (netpipefs_options.delayconnect) {
        /* Connect */
        err = establish_socket_connection(&netpipefs_socket, netpipefs_options.timeout);
        if (err == -1) {
            perror("unable to establish socket communication");
            fuse_exit(fuse);
            return 0;
        }
    }

    /* Create open files table */
    if (netpipefs_open_files_table_init() == -1) {
        perror("failed to create file table");
        fuse_exit(fuse);
        return 0;
    }

    /* Run dispatcher */
    err = netpipefs_dispatcher_run();
    if (err == -1) {
        perror("failed to run dispatcher");
        fuse_exit(fuse);
        return 0;
    }

    /* Print a resume */
    DEBUG("dispatcher running\n");
    DEBUG("connection established: %s\n", (strcmp(netpipefs_options.hostip, "localhost") == 0 ? AF_UNIX_LABEL:AF_INET_LABEL));
    DEBUG("host=%s:%d\n", netpipefs_options.hostip, netpipefs_options.hostport);
    DEBUG("local port=%d\n", netpipefs_options.port);
    DEBUG("local pipe capacity=%ld\n", netpipefs_options.pipecapacity);
    DEBUG("host pipe capacity=%ld\n", netpipefs_socket.remotepipecapacity);

    return 0;
}

/**
 * Clean up filesystem
 *
 * Called on filesystem exit.
 */
static void destroy_callback(void *privatedata) {
    int err;
    DEBUG("destroy() callback\n");

    /* Stop dispatcher thread */
    err = netpipefs_dispatcher_stop();
    if (err == -1) perror("failed to stop dispatcher thread");

    /* Destroy open files table */
    err = netpipefs_open_files_table_destroy();
    if (err == -1) perror("failed to destroy file table");

    /* Destroy socket and socket's mutex */
    err = end_socket_connection(&netpipefs_socket);
    if (err == -1) perror("failed to close socket connection");

    PTH(err, pthread_mutex_destroy(&(netpipefs_socket.wr_mtx)), perror("failed to destroy socket's mutex"))
}

/**
 * Get file attributes.
 *
 * Similar to stat().  The 'st_dev' and 'st_blksize' fields are
 * ignored. The 'st_ino' field is ignored except if the 'use_ino'
 * mount option is given. In that case it is passed to userspace,
 * but libfuse and the kernel will still assign a different
 * inode for internal use (called the "nodeid").
 */
static int getattr_callback(const char *path, struct stat *stbuf) {
    memset(stbuf, 0, sizeof(struct stat));

    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
    } else {
        stbuf->st_mode = S_IFREG | 0444;
        stbuf->st_nlink = 1;
    }

    return 0;
}

/**
 * Open a file. Open flags are available in fi->flags. The following rules
 * apply.
 *
 *  - Creation (O_CREAT, O_EXCL, O_NOCTTY) flags will be
 *    filtered out / handled by the kernel.
 *
 *  - Access modes (O_RDONLY, O_WRONLY, O_RDWR, O_EXEC, O_SEARCH)
 *    should be used by the filesystem to check if the operation is
 *    permitted.  If the ``-o default_permissions`` mount option is
 *    given, this check is already done by the kernel before calling
 *    open() and may thus be omitted by the filesystem.
 *
 *  - When writeback caching is enabled, the kernel may send
 *    read requests even for files opened with O_WRONLY. The
 *    filesystem should be prepared to handle this.
 *
 *  - When writeback caching is disabled, the filesystem is
 *    expected to properly handle the O_APPEND flag and ensure
 *    that each write is appending to the end of the file.
 *
 *  - When writeback caching is enabled, the kernel will
 *    handle O_APPEND. However, unless all changes to the file
 *    come through the kernel this will not work reliably. The
 *    filesystem should thus either ignore the O_APPEND flag
 *    (and let the kernel handle it), or return an error
 *    (indicating that reliably O_APPEND is not available).
 *
 * Filesystem may store an arbitrary file handle (pointer,
 * index, etc) in fi->fh, and use this in other all other file
 * operations (read, write, flush, release, fsync).
 *
 * Filesystem may also implement stateless file I/O and not store
 * anything in fi->fh.
 *
 * There are also some flags (direct_io, keep_cache) which the
 * filesystem may set in fi, to change the way the file is opened.
 * See fuse_file_info structure in <fuse_common.h> for more details.
 *
 * If this request is answered with an error code of ENOSYS
 * and FUSE_CAP_NO_OPEN_SUPPORT is set in
 * `fuse_conn_info.capable`, this is treated as success and
 * future calls to open will also succeed without being send
 * to the filesystem process.
 *
 */
static int open_callback(const char *path, struct fuse_file_info *fi) {
    int mode = fi->flags & O_ACCMODE;
    int nonblock = fi->flags & O_NONBLOCK;
    struct netpipe *file = NULL;

    file = netpipe_open(path, mode, nonblock);
    if (file == NULL) return -errno;

    fi->fh = (uint64_t) file;
    fi->direct_io = 1;   // avoid kernel caching
    fi->nonseekable = 1; // seeking will not be allowed

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
    DEBUG("create() callback\n");
    return open_callback(path, fi);
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
    //path is NULL because flag_nullpath_ok = 1
    struct netpipe *file = (struct netpipe *) fi->fh;
    int nonblock = fi->flags & O_NONBLOCK;

    int bytes = netpipe_read(file, buf, size, nonblock);
    if (bytes == -1) return -errno;
    return bytes;
}

/** Write data to an open file
 *
 * Write should return exactly the number of bytes requested
 * except on error.	 An exception to this is when the 'direct_io'
 * mount option is specified (see read operation).
 *
 * Unless FUSE_CAP_HANDLE_KILLPRIV is disabled, this method is
 * expected to reset the setuid and setgid bits.
 */
static int write_callback(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    //path is NULL because flag_nullpath_ok = 1
    struct netpipe *file = (struct netpipe *) fi->fh;
    int nonblock = fi->flags & O_NONBLOCK;

    int bytes = netpipe_send(file, buf, size, nonblock);
    if (bytes == -1) return -errno;
    return bytes;
}

/**
 * Poll for IO readiness events
 *
 * Note: If ph is non-NULL, the client should notify
 * when IO readiness events occur by calling
 * fuse_notify_poll() with the specified ph.
 *
 * Regardless of the number of times poll with a non-NULL ph
 * is received, single notification is enough to clear all.
 * Notifying more times incurs overhead but doesn't harm
 * correctness.
 *
 * The callee is responsible for destroying ph with
 * fuse_pollhandle_destroy() when no longer in use.
 *
 * Introduced in version 2.8
 */
static int poll_callback(const char *path, struct fuse_file_info *fi, struct fuse_pollhandle *ph, unsigned *reventsp) {
    //path is NULL because flag_nullpath_ok = 1
    struct netpipe *file = (struct netpipe *) fi->fh;
    if (ph == NULL) return 0;

    int err = netpipe_poll(file, ph, reventsp);
    if (err == -1) return -errno;

    return 0;
}

/** Release an open file
 *
 * Release is called when there are no more references to an open
 * file: all file descriptors are closed and all memory mappings
 * are unmapped.
 *
 * For every open() call there will be exactly one release() call
 * with the same flags and file handle.  It is possible to
 * have a file opened more than once, in which case only the last
 * release will mean, that no more reads/writes will happen on the
 * file.  The return value of release is ignored.
 */
static int release_callback(const char *path, struct fuse_file_info *fi) {
    //path is NULL because flag_nullpath_ok = 1
    int mode = fi->flags & O_ACCMODE;
    struct netpipe *file = (struct netpipe *) fi->fh;

    int ret = netpipe_close(file, mode);
    if (ret == -1) return -errno;
    return 0; //ignored
}

/** Change the size of a file */
static int truncate_callback(const char *path, off_t newsize) {
    return 0;
}

/** Read directory
 *
 * The filesystem may choose between two modes of operation:
 *
 * 1) The readdir implementation ignores the offset parameter, and
 * passes zero to the filler function's offset.  The filler
 * function will not return '1' (unless an error happens), so the
 * whole directory is read in a single readdir operation.
 *
 * 2) The readdir implementation keeps track of the offsets of the
 * directory entries.  It uses the offset parameter and always
 * passes non-zero offset to the filler function.  When the buffer
 * is full (or an error happens) the filler function will return
 * '1'.
 */
static int readdir_callback(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
    //path is NULL because flag_nullpath_ok = 1
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    return 0;
}

static const struct fuse_operations netpipefs_oper = {
    .destroy = destroy_callback,
    .init = init_callback,
    .getattr = getattr_callback,
    .open = open_callback,
    .create = create_callback,
    .read = read_callback,
    .write = write_callback,
    .release = release_callback,
    .truncate = truncate_callback,
    .readdir = readdir_callback,
    .poll = poll_callback,
    .flag_nullpath_ok = 1,
    .flag_nopath = 1
    /* The following operations will not receive path information:
     * read, write, flush, release, fallocate, fsync, readdir,
     * releasedir, fsyncdir, lock, ioctl and poll.
     * They will depend on the fuse_file_info structure's fh value instead.
     */
};

int main(int argc, char** argv) {
    int ret, err;
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

    /* Parse options */
    ret = netpipefs_opt_parse(argv[0], &args);
    if (ret != 0) {
        netpipefs_opt_free(&args);
        return ret == -1 ? EXIT_FAILURE:EXIT_SUCCESS;
    }

    /* Init socket mutex */
    PTHERR(err, pthread_mutex_init(&(netpipefs_socket.wr_mtx), NULL), netpipefs_opt_free(&args); return EXIT_FAILURE)

    if (!netpipefs_options.delayconnect) {
        /* Connect before mounting */
        ret = establish_socket_connection(&netpipefs_socket, netpipefs_options.timeout);
        if (ret == -1) {
            perror("unable to establish socket communication");
            netpipefs_opt_free(&args);
            return EXIT_FAILURE;
        }
    }

    /* Mount the filesystem */
    struct fuse_chan *ch = fuse_mount(netpipefs_options.mountpoint, &args);
    if (ch == NULL) {
        ret = -1;
        goto end;
    }

    /* Initialize FUSE */
    struct fuse *fuse = fuse_new(ch, &args, &netpipefs_oper, sizeof(struct fuse_operations), NULL);
    if(fuse == NULL) {
        perror("unable to initialize FUSE");
        fuse_unmount(netpipefs_options.mountpoint, ch);
        goto end;
    }

    /* Run the filesystem in foreground or background */
    ret = fuse_daemonize(netpipefs_options.foreground);
    if (ret == -1) {
        perror("failed to run the filesystem in foreground or background");
        fuse_unmount(netpipefs_options.mountpoint, ch);
        goto destroy;
    }

    /* Handle signals */
    sigset_t set;
    if (netpipefs_set_signal_handlers(&set, ch, fuse) == -1) {
        perror("failed to run signal handler thread");
        fuse_unmount(netpipefs_options.mountpoint, ch);
        goto destroy;
    }

    /* Run fuse loop. Block until CTRL+C or fusermount -u */
    if (netpipefs_options.multithreaded)
        ret = fuse_loop_mt(fuse);
    else
        ret = fuse_loop(fuse);

    if (ret != 0)
        perror("fuse loop");

    if (netpipefs_remove_signal_handlers() == -1)
        perror("unable to stop signal handler");

destroy:
    fuse_destroy(fuse);
end:
    netpipefs_opt_free(&args);
    free(netpipefs_options.mountpoint);

    return ret != 0 ? EXIT_FAILURE:EXIT_SUCCESS;
}