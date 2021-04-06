#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/select.h>
#include <string.h>
#include "../include/options.h"
#include "../include/dispatcher.h"
#include "../include/utils.h"
#include "../include/scfiles.h"
#include "../include/openfiles.h"
#include "../include/netpipefs_socket.h"

struct dispatcher {
    pthread_t tid;  // dispatcher's thread id
    int pipefd[2];  // used to communicate with main thread
};

static struct dispatcher dispatcher = {0, {-1,-1} };

extern struct netpipefs_socket netpipefs_socket;

static int on_open(char *path) {
    int bytes, mode, just_created = 0;

    bytes = readn(netpipefs_socket.fd, &mode, sizeof(int));
    if (bytes <= 0) return bytes;

    /* Get the file struct or create it */
    struct netpipe *file = netpipefs_get_or_create_open_file(path, &just_created);
    if (file == NULL) return -1;

    DEBUG("remote: OPEN %s %d\n", path, mode);
    bytes = netpipe_open_update(file, mode);
    if (bytes == -1) {
        if (just_created) {
            netpipefs_remove_open_file(path);
            netpipe_free(file, NULL); // for sure there is no poll handle
        }
        return -1;
    }

    return 1; // > 0
}

static int on_close(char *path) {
    int bytes, mode;
    bytes = readn(netpipefs_socket.fd, &mode, sizeof(int));
    if (bytes <= 0) return bytes;

    struct netpipe *file = netpipefs_get_open_file(path);
    if (file == NULL) return -1;

    DEBUG("remote: CLOSE %s %d\n", path, mode);
    MINUS1(netpipe_close_update(file, mode, &netpipefs_remove_open_file, &netpipefs_poll_notify), return -1)

    return bytes; // > 0
}

static int on_write(char *path) {
    int bytes;
    size_t size;

    struct netpipe *file = netpipefs_get_open_file(path);
    if (file == NULL) {
        errno = ENOENT;
        return -1;
    }

    /* Read how much data can be read from socket */
    bytes = readn(netpipefs_socket.fd, &size, sizeof(size_t));
    if (bytes <= 0) {
        DEBUG("bytes <= 0\n");
        return bytes;
    }
    if (size <= 0) {
        errno = EINVAL;
        return -1;
    }

    DEBUG("remote: WRITE %s %ld bytes DATA\n", path, size);
    bytes = netpipe_recv(file, size, &netpipefs_poll_notify);
    if (bytes <= 0) {
        if (errno == EPIPE) {
            DEBUG("on write broken pipe\n");
            return 1;
        }
        return -1;
    }

    return bytes;
}

static int on_read(char *path) {
    int err, bytes;
    size_t size;

    bytes = readn(netpipefs_socket.fd, &size, sizeof(size_t));
    if (bytes <= 0) return bytes;
    if (size <= 0) {
        EINVAL;
        return -1;
    }

    struct netpipe *file = netpipefs_get_open_file(path);
    if (file == NULL) return -1;

    DEBUG("remote: READ %s %ld bytes\n", path, size);
    err = netpipe_read_update(file, size, &netpipefs_poll_notify);
    if (err == -1) return -1;

    return bytes;
}

static int on_read_request(char *path) {
    int err, bytes;
    size_t size;

    bytes = readn(netpipefs_socket.fd, &size, sizeof(size_t));
    if (bytes <= 0) return bytes;
    if (size <= 0) {
        EINVAL;
        return -1;
    }

    struct netpipe *file = netpipefs_get_open_file(path);
    if (file == NULL) return -1;

    DEBUG("remote: READ_REQUEST %s %ld bytes\n", path, size);
    err = netpipe_read_request(file, size, &netpipefs_poll_notify);
    if (err == -1) return -1;

    return bytes;
}

static void *netpipefs_dispatcher_fun(void *unused) {
    int bytes = 1, err, run = 1, nfds;

    fd_set set, rd_set;
    FD_ZERO(&set);
    FD_SET(netpipefs_socket.fd, &set);
    FD_SET(dispatcher.pipefd[0], &set);
    nfds = netpipefs_socket.fd > dispatcher.pipefd[0] ? netpipefs_socket.fd : dispatcher.pipefd[0];

    while(run) {
        rd_set = set;
        err = select(nfds+1, &rd_set, NULL, NULL, NULL);
        if (err == -1) { // an error occurred then stop running
            perror("dispatcher. select() failed");
            run = 0;
        } else if (FD_ISSET(dispatcher.pipefd[0], &rd_set)) {  // pipe can be read then stop running;
            run = 0;
        } else {    // can read from socket
            enum netpipefs_header header;
            char *path = NULL;
            if ((bytes = read_socket_header(&netpipefs_socket, &header, &path)) == -1) {
                perror("dispatcher - failed to read socket message");
            } else if (bytes > 0) {
                switch (header) {
                    case OPEN:
                        bytes = on_open(path);
                        if (bytes == -1) perror("on_open");
                        break;
                    case CLOSE:
                        bytes = on_close(path);
                        if (bytes == -1) perror("on_close");
                        break;
                    case WRITE:
                        bytes = on_write(path);
                        if (bytes == -1) perror("on_write");
                        break;
                    case READ:
                        bytes = on_read(path);
                        if (bytes == -1) perror("on_read");
                        break;
                    case READ_REQUEST:
                        bytes = on_read_request(path);
                        if (bytes == -1) perror("on_read_request");
                    default:
                        break;
                }
                free(path);
            }

            run = bytes > 0;
        }
    }
    if (bytes == 0)
        DEBUG("dispatcher has lost socket connection\n");

    return 0;
}

int netpipefs_dispatcher_run(void) {
    int err;
    MINUS1(pipe(dispatcher.pipefd), return -1)

    PTH(err, pthread_create(&(dispatcher.tid), NULL, &netpipefs_dispatcher_fun, NULL), return -1)

    return 0;
}

int netpipefs_dispatcher_stop(void) {
    int err;
    if (dispatcher.pipefd[1] == -1) return 0; // already stopped

    /* Close write end. Dispatcher will wake up and stop running */
    MINUS1(close(dispatcher.pipefd[1]), return -1)
    dispatcher.pipefd[1] = -1;

    PTH(err, pthread_join(dispatcher.tid, NULL), return -1)
    DEBUG("dispatcher stopped\n");

    /* Close the read end of the pipe */
    close(dispatcher.pipefd[0]);
    dispatcher.pipefd[0] = -1;

    return 0;
}