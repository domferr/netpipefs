#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdlib.h>
#include "../include/netpipefs_socket.h"
#include "../include/scfiles.h"
#include "../include/socketconn.h"
#include "../include/utils.h"
#include "../include/options.h"

static struct sockaddr_un socket_get_address(int port) {
    struct sockaddr_un sa;
    char sockname[UNIX_PATH_MAX];

    sprintf(sockname, "%s%d.sock", BASESOCKNAME, port);
    strncpy(sa.sun_path, sockname, UNIX_PATH_MAX);
    sa.sun_family = AF_UNIX;

    return sa;
}

static int unlink_socket(int port) {
    char sockname[UNIX_PATH_MAX];
    sprintf(sockname, "%s%d.sock", BASESOCKNAME, port);
    return unlink(sockname);
}

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

int establish_socket_connection(struct netpipefs_socket *netpipefs_socket, long timeout) {
    int err, fdlisten, fdaccepted, fdconnect;
    char *host_received = NULL;
    struct sockaddr_un acc_sa = socket_get_address(netpipefs_options.port);
    struct sockaddr_un conn_sa = socket_get_address(netpipefs_options.hostport);
    size_t host_len = strlen(netpipefs_options.hostip);
    if (host_len == 0) return -1;

    MINUS1(fdlisten = socket(AF_UNIX, SOCK_STREAM, 0), return -1)
    MINUS1(fdconnect = socket(AF_UNIX, SOCK_STREAM, 0), close(fdlisten); return -1)

    netpipefs_socket->port = -1; // remember that you are not connected yet
    fdaccepted = socket_double_connect(fdconnect, fdlisten, conn_sa, acc_sa, timeout);
    close(fdlisten); // do not listen for other connections
    if (fdaccepted == -1) { // double connect failed
        close(fdconnect);
        return -1;
    }

    /* send host */
    err = socket_write_h(fdconnect, (void*) netpipefs_options.hostip, sizeof(char)*(1+host_len));
    if (err <= 0) goto error;

    /* read other host */
    err = socket_read_h(fdaccepted, (void**) &host_received);
    if (err <= 0) goto error;

    /* compare the hosts */
    int comparison = hostcmp(netpipefs_options.hostip, netpipefs_options.port, host_received, netpipefs_options.hostport);
    if (comparison > 0) {
        MINUS1(close(fdconnect), goto error)
        fdconnect = -1;
        netpipefs_socket->fd_skt = fdaccepted;
        netpipefs_socket->port  = netpipefs_options.port;
    } else if (comparison < 0) {
        MINUS1(close(fdaccepted), goto error)
        fdaccepted = -1;
        // not needed to accept other connections anymore
        MINUS1(unlink_socket(netpipefs_options.port), goto error)
        //MINUS1(socket_destroy(fdlisten, netpipefs_options.port), goto error)
        netpipefs_socket->fd_skt = fdconnect;
        netpipefs_socket->port  = -1;
    } else {
        errno = EINVAL;
        goto error;
    }

    /* send local pipe capacity */
    err = writen(netpipefs_socket->fd_skt, &netpipefs_options.pipecapacity, sizeof(size_t));
    if (err <= 0) goto error;

    /* read remote pipe capacity */
    err = readn(netpipefs_socket->fd_skt, &netpipefs_socket->remotepipecapacity, sizeof(size_t));
    if (err <= 0) goto error;

    free(host_received);
    return 0;

error:
    if (fdaccepted != -1) close(fdaccepted);
    if (fdconnect != -1) close(fdconnect);
    if (host_received) free(host_received);
    return -1;
}

int end_socket_connection(struct netpipefs_socket *netpipefs_socket) {
    if (netpipefs_socket->port == -1) return 0; // other end will close

    close(netpipefs_socket->fd_skt);
    return unlink_socket(netpipefs_options.port);
}

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