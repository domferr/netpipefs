#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/select.h>
#include <fcntl.h>
#include <stdlib.h>
#include "../include/utils.h"
#include "../include/socketconn.h"
#include "../include/scfiles.h"

int socket_connect_interval(int fd_skt, struct sockaddr_un sa, long *timeout, long interval) {
    int res;
    long sleeptime;
    while ((res = connect(fd_skt, (struct sockaddr *) &sa, sizeof(sa))) < 0) {
        if (errno == ENOENT && *timeout > 0) {
            sleeptime = *timeout < interval ? *timeout:interval;
            MINUS1(msleep(sleeptime), return -1)
            *timeout = *timeout - sleeptime;
        } else {
            break;
        }
    }

    if (*timeout == 0) {
        errno = ETIMEDOUT;
        return -1;
    }

    return res;
}

int socket_double_connect(int fdconnect, int fdaccept, struct sockaddr_un conn_sa, struct sockaddr_un acc_sa, long timeout, long interval) {
    struct timeval select_timeout;
    int error = 0, connflags, res, nsel, accepted_fd = -1, connectdone = 0;
    fd_set rset, wset;

    /* Bind and listen */
    MINUS1(bind(fdaccept, (struct sockaddr *) &acc_sa, sizeof(acc_sa)), return -1)
    MINUS1(listen(fdaccept, SOMAXCONN), return -1)

    /* Set socket for connect to nonblock */
    MINUS1(connflags = fcntl(fdconnect, F_GETFL, 0), if (acc_sa.sun_family == AF_UNIX) unlink(acc_sa.sun_path); return -1)
    MINUS1(fcntl(fdconnect, F_SETFL, connflags | O_NONBLOCK), if (acc_sa.sun_family == AF_UNIX) unlink(acc_sa.sun_path); return -1)

    /* Connect with intervals */
    res = socket_connect_interval(fdconnect, conn_sa, &timeout, interval);
    if (res == -1 && errno != EINPROGRESS) goto end;
    if (res == 0) connectdone = 1; /* connect completed immediately */

    /* Set the remaining timeout and select for connect and accept */
    select_timeout.tv_sec = MS_TO_SEC(timeout);
    select_timeout.tv_usec = MS_TO_USEC(timeout);
    while(!connectdone || accepted_fd == -1) {
        nsel = -1;
        FD_ZERO(&rset);
        if (!connectdone) {
            FD_SET(fdconnect, &rset); // rset += connect fd
            nsel = fdconnect;
            wset = rset; // wset = connect fd
        }
        if (accepted_fd == -1) {
            FD_SET(fdaccept, &rset); // rset += accept fd
            nsel = fdaccept > nsel ? fdaccept:nsel;
        }
        res = select(nsel + 1, &rset, !connectdone ? &wset:NULL, NULL, &select_timeout);
        if (res == 0) {
            errno = ETIMEDOUT;
            break;
        } else if (res == -1) {
            break;
        }

        if (!connectdone) {
            /* Check for connect */
            if (FD_ISSET(fdconnect, &rset) || FD_ISSET(fdconnect, &wset)) {
                socklen_t len = sizeof(error);
                if (getsockopt(fdconnect, SOL_SOCKET, SO_ERROR, &error, &len) < 0) { /* Solaris pending error */
                    res = -1;
                    break;
                }
                if (error) {
                    errno = error;
                    res = -1;
                    break;
                }
                connectdone = 1;
            }
        }

        /* Check for accept */
        if (accepted_fd == -1 && FD_ISSET(fdaccept, &rset)) {
            res = accept(fdaccept, NULL, 0);
            if (res == -1) break;
            accepted_fd = res;
        }
    }

end:
    MINUS1(fcntl(fdconnect, F_SETFL, connflags), return -1) /* restore file status flags */

    if (res <= 0) {
        if (accepted_fd != -1) close(accepted_fd);
        if (acc_sa.sun_family == AF_UNIX) unlink(acc_sa.sun_path);
        return -1;
    }

    return accepted_fd;
}

int socket_write_h(int fd_skt, void *data, size_t size) {
    int bytes = writen(fd_skt, &size, sizeof(size_t));
    if (bytes > 0)
        return writen(fd_skt, data, size);
    return bytes;
}

int socket_read_h(int fd_skt, void **ptr) {
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