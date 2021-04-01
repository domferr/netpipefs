#ifndef NETPIPE_H
#define NETPIPE_H

#include <pthread.h>
#include "options.h"
#include "cbuf.h"

/** Print debug info about the given file */
#define DEBUGFILE(file) DEBUG("   %s: %d readers, %d writers, %ld local bytes, %ld remote bytes\n", file->path, file->readers, file->writers, cbuf_size(file->buffer), file->remotesize)

#define DEFAULT_PIPE_CAPACITY 4096

/** Structure for a file in netpipefs */
struct netpipe {
    const char *path;
    int open_mode;  // netpipe was open locally with this mode
    int force_exit; // operations on the netpipe should immediately end
    int writers;    // number of writers
    int readers;    // number of readers
    cbuf_t *buffer;   // circular buffer
    size_t remotesize;  // how many bytes there are inside the remote buffer
    size_t remotecapacity;  // how much is the buffer capacity on the remote host
    struct poll_handle *poll_handles;
    pthread_cond_t canopen; // wait for at least one reader and one writer
    pthread_cond_t rd;  // wait if the buffer is empty
    pthread_cond_t wr;  // wait if the buffer is full
    pthread_mutex_t mtx; // netpipe lock
    struct netpipe_req *wr_req;
    struct netpipe_req *rd_req;
};

/**
 * Allocates new memory for a new file structure with the given path.
 *
 * @param path file's path
 * @return the created file structure or NULL on error and it sets errno
 */
struct netpipe *netpipe_alloc(const char *path);

/**
 * Frees the memory allocated for the given file.
 *
 * @param file the file structure
 * @param free_pollhandle function used to free each pollhandle
 * @return 0 on success, -1 on error and it sets errno
 */
int netpipe_free(struct netpipe *file);

/**
 * Lock the given file
 *
 * @param file file to be locked
 * @return 0 on success, -1 on error and sets errno
 */
int netpipe_lock(struct netpipe *file);

/**
 * Unlock the given file
 *
 * @param file file to be unlocked
 * @return 0 on success, -1 on error and sets errno
 */
int netpipe_unlock(struct netpipe *file);

/**
 * Open a netpipe. If nonblock is 0 then it waits until there is at least one reader and one writer.
 * If nonblock is 1 but there isn't at least one writer and on reader then this function returns NULL
 * and errno is set to EAGAIN.
 *
 * @param path netpipe's path
 * @param mode open mode
 * @param nonblock 1 will mean that open shouldn't wait for at least one reader and one writer
 * @return a pointer to the netpipe that was open. On error it returns NULL. It also returns NULL if
 * nonblock is 1 but there isn't at least one reader and one writer
 */
struct netpipe *netpipe_open(const char *path, int mode, int nonblock);

/**
 * Updates the netpipe and notifies that it was open remotely with the specified mode.
 *
 * @param path netpipe's path
 * @param mode open mode
 * @return a pointer to the netpipe that was open or NULL on error
 */
struct netpipe *netpipe_open_update(const char *path, int mode);

/**
 * Send "size" bytes to the remote host. This function will block (if nonblock is 0) when the remote netpipe
 * is full, otherwise if nonblock is 1 then it doesn't block and returns data that was sent without
 * blocking. If it's not possible to send data then it sets errno to EAGAIN it returns -1.
 * If there are no readers than it immediately return how much data was already sent or it returns -1
 * and sets errno to EPIPE.
 *
 * @param file pointer to the netpipe
 * @param buf data that should be sent
 * @param size how much data should be sent
 * @param nonblock if it is 1 then this function will send data that can be sent and will not block
 * @return how much data was sent, -1 on error
 */
ssize_t netpipe_send(struct netpipe *file, const char *buf, size_t size, int nonblock);

/**
 * Receive data from remote host by reading from socket.
 *
 * @param file pointer to netpipe structure
 * @param size how many bytes can be read from socket
 * @return how much data was received or -1 on error
 */
int netpipe_recv(struct netpipe *file, size_t size);

/**
 * Read "size" bytes from netpipe. Data read is put into the given buffer. If nonblock
 * is zero then this function will block until all the bytes are read from the netpipe.
 * When nonblock is 1 then this function will not block and will read all the available
 * data and will return immediately. If nonblock is 1 but the netpipe is empty then it
 * return -1 and errno is set to EAGAIN.
 *
 * @param file pointer to netpipe structure
 * @param buf where to put data read
 * @param size how many bytes should be moved from the netpipe to the buffer
 * @param nonblock if 1 then it will not block waiting for all the bytes required
 * @return how much data was read or -1 on error
 */
ssize_t netpipe_read(struct netpipe *file, char *buf, size_t size, int nonblock);

/**
 * Updates the netpipe after the remote host read.
 *
 * @param file pointer to netpipe structure
 * @param size how many bytes were read from the remote host
 * @return 0 on success, -1 on error
 */
int netpipe_read_update(struct netpipe *file, size_t size);

/**
 * Do polling by setting the available events and registering a poll handle.
 *
 * @param file the file to be polled
 * @param ph pointer to pollhandle
 * @param reventsp will be set with the available events
 * @return
 */
int netpipe_poll(struct netpipe *file, void *ph, unsigned int *reventsp);

/**
 * Flush data. When nonblock is 1 then it will flush only the bytes that can be flushed
 * without blocking. If nonblock is 0 then it will block until all the bytes in the
 * local buffer can be flushed. If nonblock is 1 but the netpipe is empty then it
 * return -1 and errno is set to EAGAIN.
 *
 * @param file the file to be flushed
 * @param nonblock if 1 then this function will not block
 * @return how many bytes were flushed or 0 if connection was lost or -1 on error
 */
ssize_t netpipe_flush(struct netpipe *file, int nonblock);

/**
 * Closes the netpipe.
 *
 * @param file pointer to netpipe structure
 * @param mode netpipe was open with this mode
 * @return 0 on success, -1 on error
 */
int netpipe_close(struct netpipe *file, int mode);

/**
 * Update the netpipe because the remote host closed with the given mode.
 *
 * @param file pointer to netpipe structure
 * @param mode netpipe was open with this mode
 * @return 0 on success, -1 on error
 */
int netpipe_close_update(struct netpipe *file, int mode);

/**
 * Forces all the operations on this netpipe to stop and immediately end.
 * After calling this function, it will not possible to do any operation
 * on this netpipe.
 *
 * @param file pointer to netpipe structure
 * @return 0 on success, -1 on error
 */
int netpipe_force_exit(struct netpipe *file);

#endif //NETPIPE_H
