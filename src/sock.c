#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/select.h>
#include <fcntl.h>
#include <stdlib.h>
#include "../include/utils.h"
#include "../include/sock.h"
#include "../include/scfiles.h"

/** Returns the size of the given sockaddr. Supports AF_UNIX and AF_INET */
static socklen_t get_socklen(struct sockaddr *sa) {
    switch(sa->sa_family) {
        case AF_UNIX:
            return sizeof(*((struct sockaddr_un *) sa));
        case AF_INET: // ipv4
            return sizeof(*((struct sockaddr_in *) sa));
    }

    return 0; // unsupported socket family
}

int sock_connect_while_accept(int fdconn, int fdacc, struct sockaddr *conn_sa, long timeout, long interval) {
    struct timeval select_timeout;
    long remaining, sleeptime;
    int connflags, res, nsel, accepted_fd = -1, connectdone = 0;
    fd_set rset, wset;

    /* Set socket for connect to nonblock */
    MINUS1(connflags = fcntl(fdconn, F_GETFL, 0), return -1)
    MINUS1(fcntl(fdconn, F_SETFL, connflags | O_NONBLOCK), return -1)

    /* Connect */
    res = connect(fdconn, conn_sa, get_socklen(conn_sa));
    if (res == -1) {
        if (errno != EINPROGRESS && errno != ENOENT) goto end;
    }
    if (res == 0) connectdone = 1; /* connect completed immediately */

    remaining = timeout;
    while(!connectdone || accepted_fd == -1) {
        nsel = -1;
        FD_ZERO(&rset);
        if (!connectdone) {
            FD_SET(fdconn, &rset); // rset += connect fd
            nsel = fdconn;
            wset = rset; // wset = connect fd
        }
        if (accepted_fd == -1) {
            FD_SET(fdacc, &rset); // rset += accept fd
            nsel = fdacc > nsel ? fdacc : nsel;
        }

        /* Set the remaining timeout */
        select_timeout.tv_sec = MS_TO_SEC(remaining);
        select_timeout.tv_usec = MS_TO_USEC(remaining);

        /* select() for connect and accept */
        res = select(nsel + 1, &rset, !connectdone ? &wset:NULL, NULL, &select_timeout);
        if (res == 0) {
            errno = ETIMEDOUT;
            res = -1;
            break;
        } else if (res == -1) {
            break;
        }

        if (!connectdone) {
            /* Check for connect */
            if (FD_ISSET(fdconn, &rset) || FD_ISSET(fdconn, &wset)) {
                res = connect(fdconn, conn_sa, get_socklen(conn_sa));
                if (res >= 0) connectdone = 1; /* connect completed immediately */
                if (res == -1) {
                    switch (errno) {
                        case EISCONN: // successful connect
                            connectdone = 1;
                            res = 1;
                            break;
                        case ECONNREFUSED:
                        case ENOENT:
                            remaining = (select_timeout.tv_sec * 1000L) + (select_timeout.tv_usec / 1000L);
                            if (remaining == 0) {
                                errno = ETIMEDOUT;
                            } else {
                                sleeptime = remaining < interval ? remaining : interval;
                                MINUS1(msleep(sleeptime), res = -1)
                                remaining -= sleeptime;
                                res = 1;
                            }
                            break;
                        case EALREADY:
                        case EINPROGRESS:
                            res = 1;
                            break;
                        default: // there is some error
                            break;
                    }
                }
                // if it is still -1 then an error occurred
                if (res == -1) break;
            }
        }

        /* Check for accept */
        if (accepted_fd == -1 && FD_ISSET(fdacc, &rset)) {
            res = accept(fdacc, NULL, 0);
            if (res == -1) break;
            accepted_fd = res;
        }
    }

end:
    MINUS1(fcntl(fdconn, F_SETFL, connflags), return -1) /* restore file status flags */

    if (res == -1) {
        if (accepted_fd != -1) close(accepted_fd);
        return -1;
    }

    return accepted_fd;
}

int sock_write_h(int fd_skt, void *data, size_t size) {
    int bytes = writen(fd_skt, &size, sizeof(size_t));
    if (bytes > 0)
        return writen(fd_skt, data, size);
    return bytes;
}

int sock_read_h(int fd_skt, void **ptr) {
    int bytes;
    size_t size = 0;
    bytes = readn(fd_skt, &size, sizeof(size_t));
    if (bytes <= 0) return bytes;

    if (size <= 0) {
        errno = EINVAL;
        return -1;
    }

    if (*ptr != NULL) free(*ptr);
    EQNULL(*ptr = (void*) malloc(size), return -1)

    bytes = readn(fd_skt, *ptr, size);
    if (bytes <= 0) free(*ptr);

    return bytes;
}