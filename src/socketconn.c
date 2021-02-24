#include "../include/socketconn.h"
#include "../include/utils.h"
#include "../include/scfiles.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/select.h>
#include <fcntl.h>
#include <stdio.h>

static struct sockaddr_un socket_get_address(void) {
    struct sockaddr_un sa;
    strncpy(sa.sun_path, SOCKNAME, UNIX_PATH_MAX);
    sa.sun_family = AF_UNIX;
    return sa;
}

int socket_listen(void) {
    struct sockaddr_un sa = socket_get_address();
    int fd_skt;
    MINUS1(fd_skt = socket(AF_UNIX, SOCK_STREAM, 0), return -1)
    MINUS1(bind(fd_skt, (struct sockaddr *) &sa, sizeof(sa)), return -1)
    MINUS1(listen(fd_skt, SOMAXCONN), return -1)

    return fd_skt;
}

int socket_accept(int fd_skt, long timeout) {
    int fd_client, err;
    ISNEGATIVE(timeout, timeout = DEFAULT_TIMEOUT)
    struct timeval time_to_wait = { MS_TO_SEC(timeout), MS_TO_USEC(timeout) };
    fd_set set;

    FD_ZERO(&set);
    FD_SET(fd_skt, &set);
    //aspetto di instaurare una connessione ma se scade il timeout termino
    MINUS1(err = select(fd_skt + 1, &set, NULL, NULL, &time_to_wait), return -1)
    if (err == 0) {
        errno = ETIMEDOUT;
        return -1;
    } else {
        MINUS1(fd_client = accept(fd_skt, NULL, 0), return -1)
    }

    return fd_client;
}

static int set_socket_blocking_mode(int fd, int blocking) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    flags = blocking ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK);
    return fcntl(fd, F_SETFL, flags);
}

int socket_connect(long timeout) {
    int fd_skt, so_error;
    socklen_t len = sizeof(so_error);
    struct sockaddr_un sa = socket_get_address();
    fd_set set;
    struct timeval time_to_wait = { MS_TO_SEC(timeout), MS_TO_USEC(timeout) };

    MINUS1(fd_skt = socket(AF_UNIX, SOCK_STREAM, 0), return -1)

    //Set the socket to non blocking mode
    MINUS1(set_socket_blocking_mode(fd_skt, 0), return -1)
    MINUS1(connect(fd_skt, (struct sockaddr *) &sa, sizeof(sa)), return -1)

    FD_ZERO(&set);
    FD_SET(fd_skt, &set);
    //Aspetto di instaurare una connessione ma se scade il timeout termino
    MINUS1(select(fd_skt + 1, &set, NULL, NULL, &time_to_wait), return -1)

    MINUS1(getsockopt(fd_skt, SOL_SOCKET, SO_ERROR, &so_error, &len), close(fd_skt); return -1)
    if (so_error == 0) {
        //Set the socket back to blocking mode
        MINUS1(set_socket_blocking_mode(fd_skt, 1), close(fd_skt); return -1)
        return fd_skt;
    } else if (time_to_wait.tv_usec == 0 && time_to_wait.tv_sec == 0) {
        errno = ETIMEDOUT;
        fprintf(stderr, "timeout\n");
    }

    close(fd_skt);
    return -1;
}

int socket_destroy(void) {
    return unlink(SOCKNAME);
}