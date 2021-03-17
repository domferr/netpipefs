#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "../include/netpipefs_socket.h"
#include "../include/scfiles.h"
#include "../include/socketconn.h"
#include "../include/utils.h"
#include "../include/options.h"

static int send_socket_header(int fd_skt, enum netpipefs_header message, const char *path) {
    int bytes = writen(fd_skt, &message, sizeof(enum netpipefs_header));
    if (bytes <= 0) return bytes;

    bytes = socket_write_h(fd_skt, (void*) path, sizeof(char)*(strlen(path)+1));
    return bytes;
}

int read_socket_header(struct netpipefs_socket *skt, enum netpipefs_header *header, char **path) {
    int bytes = readn(skt->fd_skt, header, sizeof(enum netpipefs_header));
    if (bytes > 0)
        return socket_read_h(skt->fd_skt, (void**) path);

    return bytes; // <= 0
}

int send_open_message(struct netpipefs_socket *skt, const char *path, int mode) {
    int err, bytes;

    PTH(err, pthread_mutex_lock(&(skt->writemtx)), return -1)

    bytes = send_socket_header(skt->fd_skt, OPEN, path);
    if (bytes > 0) {
        bytes = writen(skt->fd_skt, &mode, sizeof(int));
    }

    PTH(err, pthread_mutex_unlock(&(skt->writemtx)), return -1)
    if (bytes > 0) DEBUG("sent: OPEN %s %d\n", path, mode);

    return bytes;
}

int send_close_message(struct netpipefs_socket *skt, const char *path, int mode) {
    int err, bytes;

    PTH(err, pthread_mutex_lock(&(skt->writemtx)), return -1)

    bytes = send_socket_header(skt->fd_skt, CLOSE, path);
    if (bytes > 0) {
        bytes = writen(skt->fd_skt, &mode, sizeof(int));
    }

    PTH(err, pthread_mutex_unlock(&(skt->writemtx)), return -1)
    if (bytes > 0) DEBUG("sent: CLOSE %s %d\n", path, mode);

    return bytes;
}

int send_write_message(struct netpipefs_socket *skt, const char *path, const char *buf, size_t size) {
    int err, bytes;

    PTH(err, pthread_mutex_lock(&(skt->writemtx)), return -1)

    bytes = send_socket_header(skt->fd_skt, WRITE, path);
    if (bytes > 0) {
        bytes = socket_write_h(skt->fd_skt, (void*) buf, size);
    }

    PTH(err, pthread_mutex_unlock(&(skt->writemtx)), return -1)
    if (bytes > 0) DEBUG("sent: WRITE %s %ld <DATA>\n", path, size);

    return bytes;
}

int send_read_message(struct netpipefs_socket *skt, const char *path, size_t size) {
    int err, bytes;

    PTH(err, pthread_mutex_lock(&(skt->writemtx)), return -1)

    bytes = send_socket_header(skt->fd_skt, READ, path);
    if (bytes > 0) {
        bytes = writen(skt->fd_skt, &size, sizeof(size_t));
    }

    PTH(err, pthread_mutex_unlock(&(skt->writemtx)), return -1)
    if (bytes > 0) DEBUG("sent: READ %s %ld\n", path, size);

    return bytes;
}