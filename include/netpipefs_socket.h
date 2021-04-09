#ifndef NETPIPEFS_SOCKET_H
#define NETPIPEFS_SOCKET_H

#include <pthread.h>
#include "netpipe.h"

#define AF_UNIX_LABEL "AF_UNIX"
#define AF_INET_LABEL "AF_INET"
#define DEFAULT_PORT 7000
#define DEFAULT_TIMEOUT 8000    // Massimo tempo, espresso in millisecondi, per avviare una connessione socket
#define CONNECT_INTERVAL 500    // Ogni quanti millisecondi riprovare la connect se fallisce

struct netpipefs_socket {
    int fd;     // socket file descriptor
    pthread_mutex_t wr_mtx; // protect write
    size_t remote_readahead;
};

/** Header sent before each message */
enum netpipefs_header {
    OPEN = 100,
    CLOSE,
    READ,
    READ_REQUEST,
    WRITE
};



/**
 * Establish a socket connection with a maximum time expressed my the given timeout value.
 *
 * @param netpipefs_socket socket structure
 * @param timeout maximum time allowed to establish the connection. Expressed in milliseconds.
 *
 * @return 0 on success, -1 on error and sets errno. On timeout it returns -1 and sets errno to ETIMEDOUT
 */
int establish_socket_connection(struct netpipefs_socket *netpipefs_socket, long timeout);

/**
 * Closes socket connection.
 *
 * @param netpipefs_socket socket structure
 *
 * @return 0 on success, -1 on error and sets errno
 */
int end_socket_connection(struct netpipefs_socket *netpipefs_socket);

/**
 * Read from socket the header and sets the header pointer and the path pointer
 *
 * @param skt netpipefs socket structure
 * @param header pointer to a header structure
 * @param path file path read
 * @return > 0 on success, 0 if the socket was closed, -1 on error
 */
int read_socket_header(struct netpipefs_socket *skt, enum netpipefs_header *header, char **path);

/**
 * Send OPEN message
 *
 * @param skt netpipefs socket structure
 * @param path file path
 * @param mode open mode
 *
 * @return > 0 on success, 0 if the socket was closed, -1 on error
 */
int send_open_message(struct netpipefs_socket *skt, const char *path, int mode);

/**
 * Send CLOSE message
 *
 * @param skt netpipefs socket structure
 * @param path file path
 * @param mode close mode
 *
 * @return > 0 on success, 0 if the socket was closed, -1 on error
 */
int send_close_message(struct netpipefs_socket *skt, const char *path, int mode);

/**
 * Send WRITE message and data
 *
 * @param skt netpipefs socket structure
 * @param path file path
 * @param buf data
 * @param size how much data should be sent
 *
 * @return > 0 on success, 0 if the socket was closed, -1 on error
 */
int send_write_message(struct netpipefs_socket *skt, const char *path, const char *buf, size_t size);

/**
 * Send WRITE message like the function send_write_message() but get data from file buffer
 *
 * @param skt netpipefs socket structure
 * @param file the file
 * @param size how much data should be sent
 *
 * @return > 0 on success, 0 if the socket was closed, -1 on error
 */
int send_flush_message(struct netpipefs_socket *skt, struct netpipe *file, size_t size);

/**
 * Send READ message
 *
 * @param skt netpipefs socket structure
 * @param path file path
 * @param size how much data was read
 *
 * @return > 0 on success, 0 if the socket was closed, -1 on error
 */
int send_read_message(struct netpipefs_socket *skt, const char *path, size_t size);

/**
 * Send READ_REQUEST message
 *
 * @param skt netpipefs socket structure
 * @param path file path
 * @param size how much data can be read
 *
 * @return > 0 on success, 0 if the socket was closed, -1 on error
 */
int send_read_request_message(struct netpipefs_socket *skt, const char *path, size_t size);

#endif //NETPIPEFS_SOCKET_H
