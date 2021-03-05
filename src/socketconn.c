#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/select.h>
#include <fcntl.h>
#include <stdlib.h>
#include "../include/utils.h"
#include "../include/socketconn.h"
#include "../include/scfiles.h"

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
    MINUS1(bind(fd_skt, (struct sockaddr *) &sa, sizeof(sa)), return -1)    //TODO should the socket be close on error?
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

    // aspetto di instaurare una connessione ma se scade il timeout termino
    MINUS1(err = select(fd_skt + 1, &set, NULL, NULL, &time_to_wait), return -1)
    if (err == 0) {
        errno = ETIMEDOUT;
        return -1;
    } else {
        MINUS1(fd_client = accept(fd_skt, NULL, 0), return -1)
    }

    return fd_client;
}

int socket_connect(long timeout) {
    ISNEGATIVE(timeout, timeout = DEFAULT_TIMEOUT)
    int fd_skt, flags, res;
    struct sockaddr_un sa = socket_get_address();

    MINUS1(fd_skt = socket(AF_UNIX, SOCK_STREAM, 0), return -1)

    // get socket flags
    ISNEGATIVE(flags = fcntl(fd_skt, F_GETFL, NULL), close(fd_skt); return -1)
    // set socket to non-blocking
    ISNEGATIVE(fcntl(fd_skt, F_SETFL, flags | O_NONBLOCK), close(fd_skt); return -1);

    // try to connect
    while ((res = connect(fd_skt, (struct sockaddr *) &sa, sizeof(sa))) < 0) {
        if (errno == ENOENT && timeout > 0) {
            long sleeptime = timeout < CONNECT_INTERVAL ? timeout:CONNECT_INTERVAL;
            MINUS1(msleep(sleeptime), close(fd_skt); return -1)
            timeout = timeout - sleeptime;
        } else {
            break;
        }
    }

    if (res < 0) {
        if (errno == EINPROGRESS) {
            fd_set wait_set;

            // make file descriptor set with socket
            FD_ZERO(&wait_set);
            FD_SET(fd_skt, &wait_set);

            // wait for socket to be writable; return after given timeout
            struct timeval time_to_wait = { MS_TO_SEC(timeout), MS_TO_USEC(timeout) };
            res = select(fd_skt + 1, NULL, &wait_set, NULL, &time_to_wait);
        } else if (timeout == 0) {
            res = 0;
        }
    } else {
        res = 1;
    }

    // reset socket flags
    ISNEGATIVE(fcntl(fd_skt, F_SETFL, flags), close(fd_skt); return -1)

    if (res < 0) {  // an error occurred in connect or select
        close(fd_skt);
        return -1;
    } else if (res == 0) {    // select timed out or attempted to connect many times without success
        errno = ETIMEDOUT;
        close(fd_skt);
        return -1;
    } else {
        socklen_t len = sizeof(flags);
        // check for errors in socket layer
        ISNEGATIVE(getsockopt(fd_skt, SOL_SOCKET, SO_ERROR, &flags, &len), return -1)
        // there was an error
        if (flags) {
            errno = flags;
            close(fd_skt);
            return -1;
        }
    }

    return fd_skt;
}

int socket_write_h(int fd_skt, void *data, size_t size) {
    MINUS1(writen(fd_skt, &size, sizeof(size_t)), return -1)
    return writen(fd_skt, data, size);
}

void *socket_read_h(int fd_skt) {
    size_t size = 0;
    MINUS1(readn(fd_skt, &size, sizeof(size_t)), return NULL)
    if (size <= 0) {
        errno = EINVAL;
        return NULL;
    }
    void *data = (void*) malloc(size);
    EQNULL(data, return NULL)
    MINUS1(readn(fd_skt, data, size), free(data); return NULL)
    return data;
}

int socket_destroy(void) {
    return unlink(SOCKNAME);
}

int socket_read_t(int fd, void *buf, size_t size, long timeout) {
    int err;
    ISNEGATIVE(timeout, timeout = DEFAULT_TIMEOUT)
    struct timeval time_to_wait = { MS_TO_SEC(timeout), MS_TO_USEC(timeout) };
    fd_set rd_set;

    FD_ZERO(&rd_set);
    FD_SET(fd, &rd_set);
    MINUS1(err = select(fd + 1, &rd_set, NULL, NULL, &time_to_wait), return -1);
    if (err == 0) {
        errno = ETIMEDOUT;
        return -1;
    }
    return readn(fd, buf, size);
}