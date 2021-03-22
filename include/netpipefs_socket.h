#ifndef NETPIPEFS_SOCKET_H
#define NETPIPEFS_SOCKET_H

#include <pthread.h>

struct netpipefs_socket {
    int fd_skt;     // socket file descriptor
    int port;       // port used by socket
    pthread_mutex_t writemtx;
    size_t remotepipecapacity;
};

enum netpipefs_header {
    OPEN = 100,
    CLOSE,
    READ,
    WRITE
};

int establish_socket_connection(struct netpipefs_socket *netpipefs_socket, long timeout);

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
 * Send READ message
 *
 * @param skt netpipefs socket structure
 * @param path file path
 * @param size how much data was read
 *
 * @return > 0 on success, 0 if the socket was closed, -1 on error
 */
int send_read_message(struct netpipefs_socket *skt, const char *path, size_t size);

#endif //NETPIPEFS_SOCKET_H
