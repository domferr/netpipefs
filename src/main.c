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
#include "../include/scfiles.h"
#include "../include/dispatcher.h"
#include "../include/options.h"
#include "../include/fspipe_file.h"

/* Command line options */
struct fspipe_options fspipe_options;

struct fspipe_socket fspipe_socket;

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
    int bytes, err, mode = fi->flags & O_ACCMODE;

    if (mode == O_RDWR) {     // open the file for both reading and writing
        DEBUG("both read and write access is not allowed\n");
        return -EINVAL;
    }

    PTH(err, pthread_mutex_lock(&(fspipe_socket.writesktmtx)), return -errno)
    bytes = write_socket_message(fspipe_socket.fd_skt, OPEN, path, mode);
    PTH(err, pthread_mutex_unlock(&(fspipe_socket.writesktmtx)), return -errno)
    if (bytes <= 0) return -errno;

    DEBUG("sent: OPEN %s %d\n", path, mode);

    EQNULL(fspipe_file_open_local(path, mode), return -errno)

    fi->direct_io = 1; // avoid kernel caching
    // seeking, or calling pread(2) or pwrite(2) with a nonzero position is not supported on sockets. (from man 7 socket)
    fi->nonseekable = 1;

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
    //TODO se tutti gli scrittori hanno chiuso il file, allora ritornare 0 (EOF)
    /*
     * 0. fspipe_file = fi->fh;
     *
     *    //da cambiare in fspipe_file_read(fspipe_file, buf, size);
     * 1. bytes = readn(fspipe_file->fd[READ_END], size);
     *
     * 2. lock(fspipe_file->mtx);
     * 2. fspipe_file->size -= bytes;
     * 2. unlock(fspipe_file->mtx);
     * 2. if (bytes == 0) return -errno;
     *
     * 3. lock(socket_write_mtx);
     * 3. sent = write(socket, "header_size READ path bytes", header_size);
     * 3. unlock(socket_write_mtx);
     *
     * 4. return bytes;
     */
    ssize_t bytes_read = readn(fspipe_socket.fd_skt, buf, size);
    if (bytes_read == -1) {
        if (errno == EPIPE) {
            DEBUG("connection closed on the other side\n");
            return 0;   // return EOF
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
    /*
     * 0. fspipe_file = fi->fh;
     * 0. lock(fspipe_file->mtx);
     *    // se tutti i lettori hanno chiuso il file, allora ritornare errore EPIPE
     * 0. if (fspipe_file->readers == 0) unlock(fspipe_file->mtx); return -EPIPE;
     *
     *    // aspetta che l'altro abbia abbastanza spazio
     * 1. while(fspipe_file->size + size > fspipe_file->capacity) {
     *      wait(fspipe_file->nospace);
     *    }
     *
     *    // invia i dati tramite socket
     * 2. lock(socket_write_mtx);
     * 2. bytes = write(socket, "header_size WRITE path size", header_size);
     * 2. if (bytes > 0) bytes = write(socket, buf, size);
     * 2. unlock(socket_write_mtx);
     *
     *    // questo dovrebbe succedere solo se l'altro ha risposto "scritto con successo"
     * 3. if (bytes > 0) {
     *      fspipe_file->size += bytes;
     *    } else {
     *      unlock(fspipe_file->mtx);
     *      return -errno;
     *    }
     *
     * 3. unlock(fspipe_file->mtx);
     * 3. return bytes;
     */
    ssize_t bytes_wrote = writen(fspipe_socket.fd_skt, (void*) buf, size);
    if (bytes_wrote == -1) {
        if (errno == EPIPE) {
            DEBUG("connection closed on the other side\n");
            return 0;
        }
        return -errno;
    }

    return bytes_wrote;
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
    int bytes, err, mode = fi->flags & O_ACCMODE;

    MINUS1(fspipe_file_close(path, mode), return -errno)

    // send: CLOSE strlen(path) path mode
    PTH(err, pthread_mutex_lock(&(fspipe_socket.writesktmtx)), return -errno)
    bytes = write_socket_message(fspipe_socket.fd_skt, CLOSE, path, mode);
    PTH(err, pthread_mutex_unlock(&(fspipe_socket.writesktmtx)), return -errno)
    if (bytes <= 0) return -errno;

    DEBUG("sent: CLOSE %s %d\n", path, mode);

    return 0;

    /*
     * 0. fspipe_file = fi->fh;
     *
     * 1. lock(fspipe_file->mtx);
     *
     *    // cambiare in fspipe_file_close(fspipe_file, mode);
     * 1. if (read mode) {
     *      fspipe_file->readers--;
     *      if (fspipe_file->readers == 0) close(fspipe_file->fd[READ_END]);
     *    } else if (write mode) {
     *      fspipe_file->writers--;
     *      if (fspipe_file->writers == 0) close(fspipe_file->fd[WRITE_END]);
     *    }
     *
     * 1. unlock(fspipe_file->mtx);
     *
     * 2. lock(socket_write_mtx);
     * 2. sent = write(socket, "header_size CLOSE path mode", header_size);
     * 2. unlock(socket_write_mtx);
     */

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

static int hostcmp(char *firsthost, int firstport, char *secondhost, int secondport) {
    int firstaddr[4], secondaddr[4];

    ipv4_address_to_array(firsthost, firstaddr);
    ipv4_address_to_array(secondhost, secondaddr);

    for (int i = 0;  i<4; i++) {
        if (firstaddr[i] != secondaddr[i]) return firstaddr[i] - secondaddr[i];
    }

    return firstport - secondport;
}

static int establish_socket_connection(int local_port, int remote_port, char *host, long timeout) {
    int err, fd_server, fd_accepted, fd_skt;
    char *host_received = NULL;
    size_t host_len = strlen(host);
    if (host_len == 0) return -1;

    MINUS1(fd_server = socket_listen(local_port), return -1)
    MINUS1(fd_skt = socket(AF_UNIX, SOCK_STREAM, 0), socket_destroy(fd_server, local_port); return -1)

    // try to connect
    MINUS1(socket_connect_interval(fd_skt, remote_port, timeout), close(fd_skt); socket_destroy(fd_server, local_port); return -1)

    // try to accept
    MINUS1(fd_accepted = socket_accept(fd_server, timeout), goto error)

    // send host
    err = socket_write_h(fd_skt, (void*) host, sizeof(char)*(1+host_len));
    if (err <= 0) {
        goto error;
    }

    // read other host
    err = socket_read_h(fd_accepted, (void**) &host_received);
    if (err <= 0) {
        goto error;
    }

    // compare the hosts
    int comparison = hostcmp(host, local_port, host_received, remote_port);
    if (comparison > 0) {
        MINUS1(close(fd_skt), goto error)
        // not needed to accept other connections anymore
        MINUS1(close(fd_server), goto error)
        fspipe_socket.fd_skt = fd_accepted;
        fspipe_socket.port  = local_port;
    } else if (comparison < 0) {
        // not needed to accept other connections anymore
        MINUS1(close(fd_accepted), goto error)
        MINUS1(socket_destroy(fd_server, local_port), goto error)
        fspipe_socket.fd_skt = fd_skt;
        fspipe_socket.port  = -1;
    } else {
        errno = EINVAL;
        goto error;
    }

    free(host_received);
    return 0;

error:
    if (fd_accepted != -1) close(fd_accepted);
    socket_destroy(fd_server, local_port);
    socket_destroy(fd_skt, remote_port);
    if (host_received) free(host_received);
    return -1;
}

int main(int argc, char** argv) {
    int ret, err;
    struct dispatcher *dispatcher;
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

    /* Parse options */
    ret = fspipe_opt_parse(argv[0], &args);
    if (ret == -1) {
        perror("failed to parse command line options");
        return EXIT_FAILURE;
    } else if (ret == 1) {
        fspipe_opt_free(&args);
        return EXIT_SUCCESS;
    }

    PTHERR(err, pthread_mutex_init(&(fspipe_socket.writesktmtx), NULL), return EXIT_FAILURE)
    MINUS1(establish_socket_connection(fspipe_options.port, fspipe_options.remote_port, fspipe_options.host, fspipe_options.timeout), perror("unable to establish socket communication"); fspipe_opt_free(&args); return EXIT_FAILURE)

    MINUS1(fspipe_open_files_table_init(), perror("failed to create file table"); fspipe_opt_free(&args); return EXIT_FAILURE)

    /* Run dispatcher */
    EQNULL(dispatcher = fspipe_dispatcher_run(&fspipe_socket), perror("failed to run dispatcher"); fspipe_opt_free(&args); return EXIT_FAILURE)

    /* Run fuse loop. Block until CTRL+C or fusermount -u */
    ret = fuse_main(args.argc, args.argv, &fspipe_oper, NULL);
    if (ret == EXIT_FAILURE) perror("fuse_main()");

    DEBUG("%s\n", "cleanup");
    fspipe_opt_free(&args);
    MINUS1(fspipe_dispatcher_stop(dispatcher), perror("failed to stop dispatcher thread"))
    MINUS1(fspipe_dispatcher_join(dispatcher, NULL), perror("failed to join dispatcher thread"))
    fspipe_dispatcher_free(dispatcher);
    MINUS1(fspipe_open_files_table_destroy(), perror("failed to destroy file table"))
    if (fspipe_socket.port != -1)
    MINUS1(socket_destroy(fspipe_socket.fd_skt, fspipe_socket.port), perror("failed to close socket connection"))

    PTH(err, pthread_mutex_destroy(&(fspipe_socket.writesktmtx)), perror("failed to destroy socket's mutex"); return EXIT_FAILURE)
    return ret;
}