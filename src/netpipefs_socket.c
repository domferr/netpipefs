#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include "../include/options.h"
#include "../include/netpipefs_socket.h"
#include "../include/scfiles.h"
#include "../include/sock.h"
#include "../include/utils.h"

#define UNIX_PATH_MAX 108
#define BASESOCKNAME "/tmp/sockfile"

/**
 * Set up a AF_INET address with the given ip and port
 *
 * @param sin structure that will be set
 * @param port the port
 * @param ip the ip. If NULL then INADDR_ANY will be used
 * @return 0 on success, -1 on error
 */
static int afinet_address(struct sockaddr_in *sin, int port, const char *ip) {
    memset(sin, 0, sizeof(*sin));

    sin->sin_family = AF_INET;
    sin->sin_port = htons(port);
    if (ip == NULL) { // is server
        sin->sin_addr.s_addr = htonl(INADDR_ANY);
    } else if (inet_pton(AF_INET, ip, &(sin->sin_addr)) < 0) { // is client
        return -1;
    }

    return 0;
}

/**
 * Set up a AF_UNIX address. The socket file will have a name
 * like [BASESOCKNAME][port].sock.
 *
 * @param sin structure that will be set
 * @param port the port
 * @return 0 on success, -1 on error
 */
static void afunix_address(struct sockaddr_un *sun, int port) {
    char sockname[UNIX_PATH_MAX];

    memset(sun, 0, sizeof(*sun));

    sun->sun_family = AF_UNIX;
    sprintf(sockname, "%s%d.sock", BASESOCKNAME, port);
    strncpy(sun->sun_path, sockname, UNIX_PATH_MAX);
}

static int unlink_afunix_socket(int port) {
    char sockname[UNIX_PATH_MAX];
    sprintf(sockname, "%s%d.sock", BASESOCKNAME, port);
    return unlink(sockname);
}
/**
 * Compares the given ip addresses and returns 1, 0 or -1 if the first is greater or equal or less than the second.
 * If the ip addresses are not valid or "localhost" or are the same then it returns 1, 0 or -1 if the first port
 * is greater or equal or less than the second port.
 */
static int hostcmp(char *firsthost, int firstport, char *secondhost, int secondport) {
    int firstaddr[4], secondaddr[4];

    MINUS1(ipv4_address_to_array(firsthost, firstaddr), goto ports_diff)
    MINUS1(ipv4_address_to_array(secondhost, secondaddr), goto ports_diff)

    for (int i = 0; i < 4; i++) {
        if (firstaddr[i] != secondaddr[i]) return firstaddr[i] - secondaddr[i];
    }

ports_diff:
    // if it is localhost or the ip addresses are the same
    return firstport - secondport;
}

int establish_socket_connection(struct netpipefs_socket *netpipefs_socket, long timeout) {
    int err, fdlisten, fdaccepted, fdconnect, comparison, localhost;
    char *host_received = NULL;
    struct sockaddr *conn_sa;

    size_t host_len = strlen(netpipefs_options.hostip);
    if (host_len == 0) return -1;

    /* Set the sock addresses used for connect() and accept() */
    localhost = strcmp(netpipefs_options.hostip, "localhost") == 0;
    if (localhost) { // af_unix
        struct sockaddr_un acc_sa_un;
        struct sockaddr_un conn_sa_un;
        afunix_address(&conn_sa_un, netpipefs_options.hostport);
        conn_sa = (struct sockaddr *) &conn_sa_un;
        afunix_address(&acc_sa_un, netpipefs_options.port);

        /* Create accept() socket */
        MINUS1(fdlisten = socket(acc_sa_un.sun_family, SOCK_STREAM, 0), return -1)
        /* Bind */
        MINUS1(bind(fdlisten, (const struct sockaddr *) &acc_sa_un, sizeof(acc_sa_un)), close(fdlisten); return -1)
    } else { // af_inet
        struct sockaddr_in acc_sa_in;
        struct sockaddr_in conn_sa_in;
        err = afinet_address(&conn_sa_in, netpipefs_options.hostport, netpipefs_options.hostip);
        if (err == -1) return -1;
        conn_sa = (struct sockaddr *) &conn_sa_in;

        err = afinet_address(&acc_sa_in, netpipefs_options.port, NULL);
        if (err == -1) return -1;

        /* Create accept() socket */
        MINUS1(fdlisten = socket(acc_sa_in.sin_family, SOCK_STREAM, 0), return -1)
        /* Bind */
        MINUS1(bind(fdlisten, (const struct sockaddr *) &acc_sa_in, sizeof(acc_sa_in)), close(fdlisten); return -1)
    }

    /* Create connect() socket */
    MINUS1(fdconnect = socket(conn_sa->sa_family, SOCK_STREAM, 0), return -1)
    /* Listen */
    MINUS1(listen(fdlisten, SOMAXCONN), close(fdlisten); close(fdconnect); return -1)

    fdaccepted = sock_connect_while_accept(fdconnect, fdlisten, conn_sa, timeout, CONNECT_INTERVAL);
    // do not listen for other connections
    close(fdlisten);
    if (localhost)
        MINUS1(unlink_afunix_socket(netpipefs_options.port), goto error)

    if (fdaccepted == -1) { // double connect failed
        close(fdconnect);
        return -1;
    }

    /* send host */
    err = sock_write_h(fdconnect, (void *) netpipefs_options.hostip, sizeof(char) * (1 + host_len));
    if (err <= 0) goto error;

    /* read other host */
    err = sock_read_h(fdaccepted, (void **) &host_received);
    if (err <= 0) goto error;

    /* compare the hosts */
    comparison = hostcmp(netpipefs_options.hostip, netpipefs_options.hostport, host_received, netpipefs_options.port);

    if (comparison > 0) { // use fdaccepted (acc_sa)
        MINUS1(close(fdconnect), goto error)
        fdconnect = -1;

        netpipefs_socket->fd = fdaccepted;
    } else if (comparison < 0) { // use fdconnect (conn_sa)
        MINUS1(close(fdaccepted), goto error)
        fdaccepted = -1;

        netpipefs_socket->fd = fdconnect;
    } else {
        errno = EINVAL;
        goto error;
    }

    /* send local pipe capacity */
    err = writen(netpipefs_socket->fd, &netpipefs_options.pipecapacity, sizeof(size_t));
    if (err <= 0) goto error;

    /* read remote pipe capacity */
    err = readn(netpipefs_socket->fd, &netpipefs_socket->remotepipecapacity, sizeof(size_t));
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
    return close(netpipefs_socket->fd);
}

/**
 * Write to the socket the message header
 *
 * @param fd_skt socket file descriptor
 * @param message message header
 * @param path path relative to the message
 * @return 0 if the connection is lost, more than zero on success, -1 on error
 */
static int send_socket_header(int fd_skt, enum netpipefs_header message, const char *path) {
    int bytes = writen(fd_skt, &message, sizeof(enum netpipefs_header));
    if (bytes <= 0) return bytes;

    bytes = sock_write_h(fd_skt, (void *) path, sizeof(char) * (strlen(path) + 1));
    return bytes;
}

int read_socket_header(struct netpipefs_socket *skt, enum netpipefs_header *header, char **path) {
    int bytes = readn(skt->fd, header, sizeof(enum netpipefs_header));
    if (bytes > 0)
        return sock_read_h(skt->fd, (void **) path);

    return bytes; // <= 0
}

int send_open_message(struct netpipefs_socket *skt, const char *path, int mode) {
    int err, bytes;

    PTH(err, pthread_mutex_lock(&(skt->wr_mtx)), return -1)

    bytes = send_socket_header(skt->fd, OPEN, path);
    if (bytes > 0) {
        bytes = writen(skt->fd, &mode, sizeof(int));
    }

    PTH(err, pthread_mutex_unlock(&(skt->wr_mtx)), return -1)
    if (bytes > 0) DEBUG("sent: OPEN %s %d\n", path, mode);

    return bytes;
}

int send_close_message(struct netpipefs_socket *skt, const char *path, int mode) {
    int err, bytes;

    PTH(err, pthread_mutex_lock(&(skt->wr_mtx)), return -1)

    bytes = send_socket_header(skt->fd, CLOSE, path);
    if (bytes > 0) {
        bytes = writen(skt->fd, &mode, sizeof(int));
    }

    PTH(err, pthread_mutex_unlock(&(skt->wr_mtx)), return -1)
    if (bytes > 0) DEBUG("sent: CLOSE %s %d\n", path, mode);

    return bytes;
}

int send_flush_message(struct netpipefs_socket *skt, struct netpipe *file, size_t size) {
    int err, bytes;

    PTH(err, pthread_mutex_lock(&(skt->wr_mtx)), return -1)

    bytes = send_socket_header(skt->fd, WRITE, file->path);
    if (bytes > 0)
        bytes = writen(skt->fd, &size, sizeof(size_t));
    if (bytes > 0 && size > 0) {
        bytes = cbuf_writen(skt->fd, file->buffer, size);
    }

    PTH(err, pthread_mutex_unlock(&(skt->wr_mtx)), return -1)
    if (bytes > 0) DEBUG("sent: WRITE %s %ld <DATA>\n", file->path, size);

    return bytes;
}

int send_write_message(struct netpipefs_socket *skt, const char *path, const char *buf, size_t size) {
    int err, bytes;

    PTH(err, pthread_mutex_lock(&(skt->wr_mtx)), return -1)

    bytes = send_socket_header(skt->fd, WRITE, path);
    if (bytes > 0) {
        bytes = sock_write_h(skt->fd, (void *) buf, size);
    }

    PTH(err, pthread_mutex_unlock(&(skt->wr_mtx)), return -1)
    if (bytes > 0) DEBUG("sent: WRITE %s %ld <DATA>\n", path, size);

    return bytes;
}

int send_read_message(struct netpipefs_socket *skt, const char *path, size_t size) {
    int err, bytes;

    PTH(err, pthread_mutex_lock(&(skt->wr_mtx)), return -1)

    bytes = send_socket_header(skt->fd, READ, path);
    if (bytes > 0) {
        bytes = writen(skt->fd, &size, sizeof(size_t));
    }

    PTH(err, pthread_mutex_unlock(&(skt->wr_mtx)), return -1)
    if (bytes > 0) DEBUG("sent: READ %s %ld\n", path, size);

    return bytes;
}

int send_read_request_message(struct netpipefs_socket *skt, const char *path, size_t size) {
    int err, bytes;

    PTH(err, pthread_mutex_lock(&(skt->wr_mtx)), return -1)

    bytes = send_socket_header(skt->fd, READ_REQUEST, path);
    if (bytes > 0) {
        bytes = writen(skt->fd, &size, sizeof(size_t));
    }

    PTH(err, pthread_mutex_unlock(&(skt->wr_mtx)), return -1)
    if (bytes > 0) DEBUG("sent: READ_REQUEST %s %ld\n", path, size);

    return bytes;
}