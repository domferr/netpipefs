#define FUSE_USE_VERSION 29 //fuse version 2.9. Needed by fuse.h

#include <fuse.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include "../include/utils.h"
#include "../include/socketconn.h"
#include "../include/scfiles.h"

/*
 * Command line options
 *
 * We can't set default values for the char* fields here because
 * fuse_opt_parse would attempt to free() them when the user specifies
 * different values on the command line.
 *
 * From https://github.com/libfuse/libfuse/blob/master/example/hello.c
 */
static struct fspipe {
    const char *host;
    int port;
    int remote_port;
    int show_help;
    int debug;
    long timeout;
} fspipe;

#define FSPIPE_OPT(t, p, v) { t, offsetof(struct fspipe, p), v }

static const struct fuse_opt fspipe_opts[] = {
        FSPIPE_OPT("--host=%s",         host, 0),
        FSPIPE_OPT("--port=%d",         port, 0),
        FSPIPE_OPT("--remote_port=%d",  remote_port, 0),
        FSPIPE_OPT("--timeout=%d",      timeout, 0),
        FSPIPE_OPT("-h",                show_help, 1),
        FSPIPE_OPT("--help",            show_help, 1),
        FSPIPE_OPT("-d",                debug, 1),
        FSPIPE_OPT("debug",             debug, 1),

        FUSE_OPT_END
};

#define DEBUG(...)						\
	do { if (fspipe.debug) fprintf(stderr, ##__VA_ARGS__); } while(0)

struct fspipe_data {
    int fd_server;  //used to accept socket connections
    int fd_skt;     //used to communicate via sockets
} fspipe_data;

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
/*static void* init_callback(struct fuse_conn_info *conn)
{
    DEBUG("init() callback\n")

    return 0;
}*/

/**
 * Clean up filesystem
 *
 * Called on filesystem exit.
 */
static void destroy_callback(void *privatedata) {
    DEBUG("destroy() callback\n");

    if (fspipe_data.fd_server != -1) {
        close(fspipe_data.fd_server);
    }

    if (fspipe_data.fd_skt != -1) {
        close(fspipe_data.fd_skt);
    }

    socket_destroy();
}

/** Get file attributes.
 *
 * Similar to stat().  The 'st_dev' and 'st_blksize' fields are
 * ignored. The 'st_ino' field is ignored except if the 'use_ino'
 * mount option is given. In that case it is passed to userspace,
 * but libfuse and the kernel will still assign a different
 * inode for internal use (called the "nodeid").
 *
 * `fi` will always be NULL if the file is not currently open, but
 * may also be NULL if the file is open.
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

/** Open a file
 *
 * Open flags are available in fi->flags. The following rules
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
    /*
     * Si blocca fino a quando il file non viene aperto sia in lettura che in scrittura. (man fifo 7)
     */
    int confirm, fd;
    size_t size;

    if ((fi->flags & O_ACCMODE) == O_RDONLY) {        // open the file for read access
        if (fspipe_data.fd_skt == -1) {
            MINUS1ERR(fspipe_data.fd_server = socket_listen(), perror("failed to use the socket"); return -ENOENT)
            MINUS1(fd = socket_accept(fspipe_data.fd_server, fspipe.timeout), perror("failed to accept"); return -ENOENT)
        } else {
            fd = fspipe_data.fd_skt;
        }

        // read what path is requested
        if (readn(fd, &size, sizeof(size_t)) <= 0) return -ENOENT;
        char *other_path = (char*) malloc(sizeof(char)*size);
        EQNULL(other_path, return -ENOENT)
        if (readn(fd, other_path, sizeof(char)*size) <= 0) {
            free(other_path);
            return -ENOENT;
        }
        // compare its path with mine and send confirmation
        confirm = strcmp(path, other_path) == 0 ? 1:0;
        free(other_path);
        if (writen(fd, &confirm, sizeof(int)) <= 0) return -ENOENT;
        if (!confirm) {
            return -ENOENT;
        }

        fi->direct_io = 1; // avoid kernel caching
        // seeking, or calling pread(2) or pwrite(2) with a nonzero position is not supported on sockets. (from man 7 socket)
        fi->nonseekable = 1;
    } else if ((fi->flags & O_ACCMODE) == O_WRONLY) {  // open the file for write access
        if (fspipe_data.fd_skt == -1) {
            MINUS1(fd = socket_connect(fspipe.timeout), perror("failed to connect"); return -ENOENT)
        } else {
            fd = fspipe_data.fd_skt;
        }

        // send the path requested by the client
        size = strlen(path);
        if (writen(fd, &size, sizeof(size_t)) <= 0) return -ENOENT;
        if (writen(fd, (void*) path, sizeof(char)*size) <= 0) return -ENOENT;

        // read the confirmation
        if (readn(fd, &confirm, sizeof(int)) <= 0) return -ENOENT;

        if (!confirm) {
            return -ENOENT;
        }
    } else if ((fi->flags & O_ACCMODE) == O_RDWR) {     // open the file for both reading and writing
        DEBUG("read and write access not permitted\n");
        return -EINVAL;
    } else {
        return -EINVAL;
    }

    fspipe_data.fd_skt = fd;
    DEBUG("established connection for %s\n", path);
    return 0;
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
    //TODO se tutti gli scrittori hanno chiuso il file, allora ritornare 0 (EOF)
    ssize_t bytes_read = readn(fspipe_data.fd_skt, buf, size);
    if (bytes_read == -1) {
        if (errno == EPIPE) {
            DEBUG("connection closed on the other side\n");
            return 0;
        }
        return -errno;
    }
    return bytes_read; // return >= 0
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
    //TODO se tutti i lettori hanno chiuso il file, allora ritornare errore EPIPE
    ssize_t bytes_wrote = writen(fspipe_data.fd_skt, (void*) buf, size);
    if (bytes_wrote == -1) {
        if (errno == EPIPE) {
            DEBUG("connection closed on the other side\n");
            return 0;
        }
        return -errno;
    }

    return bytes_wrote;
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
    return open_callback(path, fi);
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
    return 0;
}

/** Change the size of a file
 *
 * `fi` will always be NULL if the file is not currently open, but
 * may also be NULL if the file is open.
 *
 * Unless FUSE_CAP_HANDLE_KILLPRIV is disabled, this method is
 * expected to reset the setuid and setgid bits.
 *
 */
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

static const struct fuse_operations fspipe_oper = {
    .destroy = destroy_callback,
    .getattr = getattr_callback,
    .open = open_callback,
    .create = create_callback,
    .read = read_callback,
    .write = write_callback,
    .release = release_callback,
    .truncate = truncate_callback,
    .readdir = readdir_callback
};

static void show_help(const char *progname);

int main(int argc, char** argv) {
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

    /* Set defaults */
    fspipe.timeout = DEFAULT_TIMEOUT;
    fspipe.host = NULL;
    fspipe.port = DEFAULT_PORT;
    fspipe.remote_port = -1;

    /* Parse options */
    MINUS1ERR(fuse_opt_parse(&args, &fspipe, fspipe_opts, NULL), return 1)

    if (fspipe.debug) {
        MINUS1ERR(fuse_opt_add_arg(&args, "-d"), return 1)
    }
    /* When --help is specified, first print file-system specific help text,
       then signal fuse_main to show additional help (by adding `--help` to the options again)
       without usage: line (by setting argv[0] to the empty string) */
    if (fspipe.show_help) {
        show_help(argv[0]);
        return 0;
    } else if (fspipe.host == NULL) {
        fprintf(stderr, "missing host\nsee '%s -h' for usage\n", argv[0]);
        return 1;
    } else if (fspipe.remote_port == -1) {
        fprintf(stderr, "missing remote port\nsee '%s -h' for usage\n", argv[0]);
        return 1;
    }

    fspipe_data.fd_server   = -1;
    fspipe_data.fd_skt      = -1;
    DEBUG("fspipe running on local port %d and host %s:%d\n", fspipe.port, fspipe.host, fspipe.remote_port);

    int ret = fuse_main(args.argc, args.argv, &fspipe_oper, NULL);

    DEBUG("%s\n", "cleanup");
    fuse_opt_free_args(&args);
    return ret;
}

static void show_help(const char *progname) {
    printf("usage: %s [options] <mountpoint>\n"
           "\n", progname);
    printf("fspipe options:\n"
           "    --port=<d>             local port used for the socket connection (default: %d)\n"
           "    --host=<s>             remote host address to which connect to\n"
           "    --remote_port=<d>      remote port used for the socket connection\n"
           "\n", DEFAULT_PORT);
    printf("general options:\n"
           "    -o opt,[opt...]        mount options\n"
           "    -h   --help            print help\n"
           "    -V   --version         print version\n"
           "\n"
           "FUSE options:\n"
           "    -d   -o debug          enable debug output (implies -f)\n"
           "    -f                     foreground operation\n"
           "    -s                     disable multi-threaded operation\n"
           "\n"
           "    -o allow_other         allow access to other users\n"
           "    -o allow_root          allow access to root\n"
           "    -o auto_unmount        auto unmount on process termination\n"
           "    -o nonempty            allow mounts over non-empty file/dir\n"
           "    -o default_permissions enable permission checking by kernel\n"
           "    -o fsname=NAME         set filesystem name\n"
           "    -o subtype=NAME        set filesystem type\n"
           "    -o large_read          issue large read requests (2.4 only)\n"
           "    -o max_read=N          set maximum size of read requests\n"
           "\n"
           "    -o hard_remove         immediate removal (don't hide files)\n"
           "    -o use_ino             let filesystem set inode numbers\n"
           "    -o readdir_ino         try to fill in d_ino in readdir\n"
           "    -o direct_io           use direct I/O\n"
           "    -o kernel_cache        cache files in kernel\n"
           "    -o [no]auto_cache      enable caching based on modification times (off)\n"
           "    -o umask=M             set file permissions (octal)\n"
           "    -o uid=N               set file owner\n"
           "    -o gid=N               set file group\n"
           "    -o entry_timeout=T     cache timeout for names (1.0s)\n"
           "    -o negative_timeout=T  cache timeout for deleted names (0.0s)\n"
           "    -o attr_timeout=T      cache timeout for attributes (1.0s)\n"
           "    -o ac_attr_timeout=T   auto cache timeout for attributes (attr_timeout)\n"
           "    -o noforget            never forget cached inodes\n"
           "    -o remember=T          remember cached inodes for T seconds (0s)\n"
           "    -o nopath              don't supply path if not necessary\n"
           "    -o intr                allow requests to be interrupted\n"
           "    -o intr_signal=NUM     signal to send on interrupt (10)\n"
           "    -o modules=M1[:M2...]  names of modules to push onto filesystem stack\n"
           "\n"
           "    -o max_write=N         set maximum size of write requests\n"
           "    -o max_readahead=N     set maximum readahead\n"
           "    -o max_background=N    set number of maximum background requests\n"
           "    -o congestion_threshold=N  set kernel's congestion threshold\n"
           "    -o async_read          perform reads asynchronously (default)\n"
           "    -o sync_read           perform reads synchronously\n"
           "    -o atomic_o_trunc      enable atomic open+truncate support\n"
           "    -o big_writes          enable larger than 4kB writes\n"
           "    -o no_remote_lock      disable remote file locking\n"
           "    -o no_remote_flock     disable remote file locking (BSD)\n"
           "    -o no_remote_posix_lock disable remove file locking (POSIX)\n"
           "    -o [no_]splice_write   use splice to write to the fuse device\n"
           "    -o [no_]splice_move    move data while splicing to the fuse device\n"
           "    -o [no_]splice_read    use splice to read from the fuse device\n"
           "\n"
           "Module options:\n"
           "\n"
           "[iconv]\n"
           "    -o from_code=CHARSET   original encoding of file names (default: UTF-8)\n"
           "    -o to_code=CHARSET      new encoding of the file names (default: UTF-8)\n"
           "\n"
           "[subdir]\n"
           "    -o subdir=DIR           prepend this directory to all paths (mandatory)\n"
           "    -o [no]rellinks         transform absolute symlinks to relative\n");
}