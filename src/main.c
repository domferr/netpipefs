#define FUSE_USE_VERSION 29 //fuse version 2.9. Needed by fuse.h

#include <fuse.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include "../include/utils.h"
#include "../include/socketconn.h"
#include "../include/dispatcher.h"
#include "../include/options.h"
#include "../include/netpipefs_file.h"
#include "../include/openfiles.h"
#include "../include/scfiles.h"
#include "../include/netpipefs_socket.h"

/* Command line options */
struct netpipefs_options netpipefs_options;
/* Socket communication */
struct netpipefs_socket netpipefs_socket;
/* Dispatcher thread */
struct dispatcher *dispatcher = NULL;

/**
 * Compares the given ip addresses and returns 1, 0 or -1 if the first is greater or equal or less than the second.
 * If the two ip addresses are equal then returns 1, 0 or -1 if firstport greater or equal or less than secondport.
 * If the ip addresses are not valid then 0 is returned.
 */
static int hostcmp(char *firsthost, int firstport, char *secondhost, int secondport) {
    int firstaddr[4], secondaddr[4];

    MINUS1(ipv4_address_to_array(firsthost, firstaddr), return 0)
    MINUS1(ipv4_address_to_array(secondhost, secondaddr), return 0)

    for (int i = 0; i < 4; i++) {
        if (firstaddr[i] != secondaddr[i]) return firstaddr[i] - secondaddr[i];
    }

    return firstport - secondport;
}

static int establish_socket_connection(long timeout) {
    int err, fd_server, fd_accepted, fd_skt;
    char *host_received = NULL;
    size_t host_len = strlen(netpipefs_options.hostip);
    if (host_len == 0) return -1;

    MINUS1(fd_server = socket_listen(netpipefs_options.port), return -1)
    MINUS1(fd_skt = socket(AF_UNIX, SOCK_STREAM, 0), socket_destroy(fd_server, netpipefs_options.port); return -1)

    /* try to connect */
    MINUS1(socket_connect_interval(fd_skt, netpipefs_options.hostport, timeout), close(fd_skt); socket_destroy(fd_server, netpipefs_options.port); return -1)

    /* try to accept */
    MINUS1(fd_accepted = socket_accept(fd_server, timeout), goto error)

    /* send host */
    err = socket_write_h(fd_skt, (void*) netpipefs_options.hostip, sizeof(char)*(1+host_len));
    if (err <= 0) goto error;

    /* read other host */
    err = socket_read_h(fd_accepted, (void**) &host_received);
    if (err <= 0) goto error;

    /* compare the hosts */
    int comparison = hostcmp(netpipefs_options.hostip, netpipefs_options.port, host_received, netpipefs_options.hostport);
    if (comparison > 0) {
        MINUS1(close(fd_skt), goto error)
        // not needed to accept other connections anymore
        MINUS1(close(fd_server), goto error)
        netpipefs_socket.fd_skt = fd_accepted;
        netpipefs_socket.port  = netpipefs_options.port;
    } else if (comparison < 0) {
        // not needed to accept other connections anymore
        MINUS1(close(fd_accepted), goto error)
        MINUS1(socket_destroy(fd_server, netpipefs_options.port), goto error)
        netpipefs_socket.fd_skt = fd_skt;
        netpipefs_socket.port  = -1;
    } else {
        errno = EINVAL;
        goto error;
    }

    /* send local pipe capacity */
    err = writen(netpipefs_socket.fd_skt, &netpipefs_options.pipecapacity, sizeof(size_t));
    if (err <= 0) goto error;

    /* read remote pipe capacity */
    err = readn(netpipefs_socket.fd_skt, &netpipefs_socket.remotepipecapacity, sizeof(size_t));
    if (err <= 0) goto error;

    free(host_received);
    return 0;

    error:
    if (fd_accepted != -1) close(fd_accepted);
    socket_destroy(fd_server, netpipefs_options.port);
    socket_destroy(fd_skt, netpipefs_options.hostport);
    if (host_received) free(host_received);
    return -1;
}

/**
 * Initialize filesystem
 *
 * The return value will passed in the `private_data` field of
 * `struct fuse_context` to all file operations, and as a
 * parameter to the destroy() method. It overrides the initial
 * value provided to fuse_main() / fuse_new().
 */
// Undocumented but extraordinarily useful fact:  the fuse_context is
// set up before this function is called, and
// fuse_get_context()->private_data returns the user_data passed to
// fuse_main().  Really seems like either it should be a third
// parameter coming in here, or else the fact should be documented
// (and this might as well return void, as it did in older versions of
// FUSE).
static void* init_callback(struct fuse_conn_info *conn) {
    struct fuse *fuse = fuse_get_context()->fuse;
    DEBUG("init() callback\n");
    DEBUG("remote pipe capacity: %ld\n", netpipefs_socket.remotepipecapacity);

    /* Create open files table */
    if (netpipefs_open_files_table_init() == -1) {
        perror("failed to create file table");
        fuse_exit(fuse);
    }

    /* Run dispatcher */
    dispatcher = netpipefs_dispatcher_run();
    if (dispatcher == NULL) {
        perror("failed to run dispatcher");
        fuse_exit(fuse);
    }

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

    /* Stop and join dispatcher thread */
    if (dispatcher != NULL) {
        MINUS1(netpipefs_dispatcher_stop(dispatcher), perror("failed to stop dispatcher thread"))
        MINUS1(netpipefs_dispatcher_join(dispatcher, NULL), perror("failed to join dispatcher thread"))
        netpipefs_dispatcher_free(dispatcher);
    }

    /* Destroy open files table */
    MINUS1(netpipefs_open_files_table_destroy(), perror("failed to destroy file table"))

    /* Destroy socket and socket's mutex */
    if (netpipefs_socket.port != -1 && netpipefs_socket.fd_skt != -1) {
        MINUS1(socket_destroy(netpipefs_socket.fd_skt, netpipefs_socket.port),perror("failed to close socket connection"))
    }
    PTH(err, pthread_mutex_destroy(&(netpipefs_socket.writemtx)), perror("failed to destroy socket's mutex"))
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
    struct netpipefs_file *file = NULL;

    if (mode == O_RDWR) {     // open the file for both reading and writing
        DEBUG("both read and write access is not allowed\n");
        return -EINVAL;
    }

    file = netpipefs_file_open(path, mode);
    if (file == NULL) return -errno;

    fi->fh = (uint64_t) file;
    fi->direct_io = 1; // avoid kernel caching
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
    struct netpipefs_file *file = (struct netpipefs_file *) fi->fh;

    int bytes = netpipefs_file_read(file, buf, size);
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
    struct netpipefs_file *file = (struct netpipefs_file *) fi->fh;

    int bytes = netpipefs_file_send(file, buf, size);
    if (bytes == -1) return -errno;
    return bytes;
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
    int mode = fi->flags & O_ACCMODE;
    struct netpipefs_file *file = (struct netpipefs_file *) fi->fh;

    int ret = netpipefs_file_close(file, mode);
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
    .readdir = readdir_callback
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

    /* Remember that socket is not connected */
    netpipefs_socket.fd_skt = -1;
    netpipefs_socket.port = -1;

    /* Connect via socket */
    PTHERR(err, pthread_mutex_init(&(netpipefs_socket.writemtx), NULL), netpipefs_opt_free(&args); return EXIT_FAILURE)
    if (establish_socket_connection(netpipefs_options.timeout) == -1) {
        perror("unable to establish socket communication");
        netpipefs_opt_free(&args);
        return EXIT_FAILURE;
    }

    /* Run fuse loop. Block until CTRL+C or fusermount -u */
    ret = fuse_main(args.argc, args.argv, &netpipefs_oper, NULL);
    if (ret == EXIT_FAILURE) perror("fuse_main()");

    DEBUG("main cleanup\n");
    netpipefs_opt_free(&args);

    return ret;
}