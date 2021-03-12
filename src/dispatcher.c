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
#include "../include/socketconn.h"
#include "../include/netpipefs_file.h"
#include "../include/openfiles.h"

static int read_socket_message(struct netpipefs_socket *netpipefs_socket, enum netpipefs_message *message, char **path) {
    int bytes = readn(netpipefs_socket->fd_skt, message, sizeof(enum netpipefs_message));
    if (bytes > 0)
        return socket_read_h(netpipefs_socket->fd_skt, (void**) path);

    return bytes; // <= 0
}

static int on_open(struct netpipefs_socket *netpipefs_socket, char *path) {
    int bytes, mode;

    bytes = readn(netpipefs_socket->fd_skt, &mode, sizeof(int));
    if (bytes <= 0) return bytes;

    DEBUG("remote: OPEN %s %d\n", path, mode);
    EQNULL(netpipefs_file_open_remote(path, mode), return -1)

    return 1; // > 0
}

static int on_close(struct netpipefs_socket *netpipefs_socket, char *path) {
    int bytes, mode;
    bytes = readn(netpipefs_socket->fd_skt, &mode, sizeof(int));
    if (bytes <= 0) return bytes;

    DEBUG("remote: CLOSE %s %d\n", path, mode);
    MINUS1(netpipefs_file_close_remote(path, mode), return -1)

    return bytes; // > 0
}

static int on_write(struct netpipefs_socket *netpipefs_socket, char *path) {
    int bytes;
    char *buf = NULL;
    bytes = socket_read_h(netpipefs_socket->fd_skt, (void**) &buf);
    if (bytes <= 0) return bytes;
    DEBUG("remote: WRITE %s %d bytes DATA\n", path, bytes);

    bytes = netpipefs_file_write_local(path, buf, bytes);
    free(buf);
    if (bytes <= 0) {
        if (errno == EPIPE) {
            DEBUG("on write broken pipe\n");
            return 1;
        }
        return -1;
    }

    return bytes;
}

static int on_read(struct netpipefs_socket *netpipefs_socket, char *path) {
    int bytes, read;
    read = readn(netpipefs_socket->fd_skt, &bytes, sizeof(int));
    if (read <= 0) return read;

    DEBUG("remote: READ %s %d bytes\n", path, bytes);

    bytes = netpipefs_file_read_remote(path, bytes);
    if (bytes <= 0) return -1;

    return read;
}

static void *netpipefs_dispatcher_fun(void *args) {
    int bytes = 1, err, run = 1, nfds;
    struct dispatcher *dispatcher = (struct dispatcher *) args;
    struct netpipefs_socket *netpipefs_socket = dispatcher->netpipefs_socket;
    DEBUG("%s\n", "dispatcher - running");

    fd_set set, rd_set;
    FD_ZERO(&set);
    FD_SET(netpipefs_socket->fd_skt, &set);
    FD_SET(dispatcher->pipefd[0], &set);
    nfds = netpipefs_socket->fd_skt > dispatcher->pipefd[0] ? netpipefs_socket->fd_skt:dispatcher->pipefd[0];

    while(run) {
        rd_set = set;
        err = select(nfds+1, &rd_set, NULL, NULL, NULL);
        if (err == -1) { // an error occurred then stop running
            perror("dispatcher - select() failed");
            run = 0;
        } else if (FD_ISSET(dispatcher->pipefd[0], &rd_set)) {  // pipe can be read then stop running
            run = 0;
        } else {    // can read from socket
            enum netpipefs_message message;
            char *path = NULL;
            if ((bytes = read_socket_message(netpipefs_socket, &message, &path)) == -1) {
                perror("dispatcher - failed to read socket message");
            } else if (bytes > 0) {
                switch (message) {
                    case OPEN:
                        bytes = on_open(netpipefs_socket, path);
                        if (bytes == -1) perror("on_open");
                        break;
                    case CLOSE:
                        bytes = on_close(netpipefs_socket, path);
                        if (bytes == -1) perror("on_close");
                        break;
                    case WRITE:
                        bytes = on_write(netpipefs_socket, path);
                        if (bytes == -1) perror("on_write");
                        break;
                    case READ:
                        bytes = on_read(netpipefs_socket, path);
                        if (bytes == -1) perror("on_read");
                        break;
                    default:
                        break;
                }
                free(path);
            }

            run = bytes > 0;
        }
    }
    if (bytes == 0)
        DEBUG("%s\n", "dispatcher - socket connection lost");
    DEBUG("%s\n", "dispatcher - stopped running");
    return 0;
}

struct dispatcher *netpipefs_dispatcher_run(struct netpipefs_socket *netpipefs_data) {
    struct dispatcher *dispatcher = (struct dispatcher*) malloc(sizeof(struct dispatcher));
    EQNULL(dispatcher, return NULL)

    MINUS1(pipe(dispatcher->pipefd), return NULL)

    if (pthread_create(&(dispatcher->tid), NULL, &netpipefs_dispatcher_fun, dispatcher) != 0) {
        free(dispatcher);
        return NULL;
    }

    dispatcher->netpipefs_socket = netpipefs_data;
    return dispatcher;
}

int netpipefs_dispatcher_stop(struct dispatcher *dispatcher) {
    return close(dispatcher->pipefd[1]);
}

int netpipefs_dispatcher_join(struct dispatcher *dispatcher, void *ret) {
    int r;
    PTH(r, pthread_join(dispatcher->tid, ret), return -1)
    return close(dispatcher->pipefd[0]);
}

void netpipefs_dispatcher_free(struct dispatcher *dispatcher) {
    free(dispatcher);
}