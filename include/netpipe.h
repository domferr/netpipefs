#ifndef NETPIPE_H
#define NETPIPE_H

#include <pthread.h>
#include "options.h"
#include "cbuf.h"

/** Print debug info about the given file */
#define DEBUGFILE(file) \
    do { if ((file)->open_mode == O_RDONLY) { \
            DEBUG("[%s] %d readers, %d writers, %ld/%ld local buffer\n", \
            (file)->path, (file)->readers, (file)->writers, cbuf_size((file)->buffer), cbuf_capacity((file)->buffer)); \
        } else {        \
            DEBUG("[%s] %d readers, %d writers, %ld/%ld local buffer, %ld/%ld remote bytes\n", \
            (file)->path, (file)->readers, (file)->writers, cbuf_size((file)->buffer), cbuf_capacity((file)->buffer), (file)->remotesize, (file)->remotemax); \
        }               \
    } while(0)

#define DEFAULT_PIPE_CAPACITY 4096

/** Structure for a file in netpipefs */
struct netpipe {
    const char *path;
    int open_mode;  // netpipe was open locally with this mode
    int force_exit; // operations on the netpipe should immediately end
    int writers;    // number of writers
    int readers;    // number of readers
    cbuf_t *buffer; // circular buffer
    size_t remotemax;  // max number of bytes that can be sent
    size_t remotesize; // number of bytes sent
    pthread_cond_t canopen; // wait for at least one reader and one writer
    pthread_cond_t close;   // wait that the buffer is flushed before close
    pthread_mutex_t mtx;    // netpipe lock
    struct netpipe_req_l *req_l; // FIFO list of read or write requests
    struct poll_handle *poll_handles;
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
 * @param poll_destroy function used to free each poll handle
 * @return 0 on success, -1 on error and it sets errno
 */
int netpipe_free(struct netpipe *file, void (*poll_destroy)(void *));

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
 * Open the given netpipe. If nonblock is 0 then it waits until there is at least one reader and one writer.
 * If nonblock is 1 but there isn't at least one writer and on reader then this function returns NULL
 * and errno is set to EAGAIN.
 *
 * @param file the netpipe that should be open
 * @param mode open mode
 * @param nonblock 1 will mean that open shouldn't wait for at least one reader and one writer
 * @return 0 on success, -1 on error. It also returns -1 if nonblock is 1 but there isn't at least one reader and one writer
 */
int netpipe_open(struct netpipe *file, int mode, int nonblock);

/**
 * Updates the netpipe and notifies that it was open remotely with the specified mode.
 *
 * @param file the netpipe that was open remotely
 * @param mode open mode
 * @return 0 on success, -1 on error
 */
int netpipe_open_update(struct netpipe *file, int mode);

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
 * @param poll_notify pointer to a function that will be called to notify each registered poll handle
 * @return how much data was received or -1 on error
 */
int netpipe_recv(struct netpipe *file, size_t size, void (*poll_notify)(void *)) ;

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
 * Notify the netpipe that the remote host read "size" bytes.
 *
 * @param file pointer to netpipe structure
 * @param size how many bytes were read from the remote host
 * @param poll_notify pointer to a function that will be called to notify each registered poll handle
 * @return 0 on success, -1 on error
 */
int netpipe_read_update(struct netpipe *file, size_t size, void (*poll_notify)(void *));

/**
 * Notify the netpipe that there are some readers waiting for "size" bytes.
 *
 * @param file pointer to netpipe structure
 * @param size how many bytes the remote host is waiting for
 * @param poll_notify pointer to a function that will be called to notify each registered poll handle
 * @return 0 on success, -1 on error
 */
int netpipe_read_request(struct netpipe *file, size_t size, void (*poll_notify)(void *));

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
 * @param remove_open_file pointer to a function used to remove the open file safely from any data structure before it is freed
 * @param poll_notify pointer to a function that will be called to notify each registered poll handle
 * @return 0 on success, -1 on error
 */
int netpipe_close(struct netpipe *file, int mode, int (*remove_open_file)(const char *), void (*poll_notify)(void *));

/**
 * Update the netpipe because the remote host closed with the given mode.
 *
 * @param file pointer to netpipe structure
 * @param mode netpipe was open with this mode
 * @param remove_open_file pointer to a function used to remove the open file safely from any data structure before it is freed
 * @param poll_notify pointer to a function that will be called to notify each registered poll handle
 * @return 0 on success, -1 on error
 */
int netpipe_close_update(struct netpipe *file, int mode, int (*remove_open_file)(const char *), void (*poll_notify)(void *));

/**
 * Forces all the operations on this netpipe to stop and immediately end.
 * After calling this function, it will not possible to do any operation
 * on this netpipe.
 *
 * @param file pointer to netpipe structure
 * @param poll_notify pointer to a function that will be called to notify each registered poll handle
 * @return 0 on success, -1 on error
 */
int netpipe_force_exit(struct netpipe *file, void (*poll_notify)(void *));

#endif //NETPIPE_H
