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
#include "../include/fspipe_file.h"

static int read_socket_message(struct fspipe_socket *fspipe_socket, enum socket_message *message, char **path) {
    int bytes = readn(fspipe_socket->fd_skt, message, sizeof(enum socket_message));
    if (bytes > 0)
        return socket_read_h(fspipe_socket->fd_skt, (void**) path);

    return bytes;
}

static void *fspipe_dispatcher_fun(void *args) {
    int bytes = 1, err, run = 1;
    struct dispatcher *dispatcher = (struct dispatcher *) args;
    struct fspipe_socket *fspipe_socket = dispatcher->fspipe_socket;
    DEBUG("%s\n", "dispatcher - running");

    fd_set set, rd_set;
    FD_ZERO(&set);
    FD_SET(fspipe_socket->fd_skt, &set);
    FD_SET(dispatcher->pipefd[0], &set);

    while(run) {
        rd_set = set;
        err = select(dispatcher->pipefd[0]+1, &rd_set, NULL, NULL, NULL);
        if (err == -1) { // an error occurred then stop running
            perror("dispatcher - select() failed");
            run = 0;
        } else if (FD_ISSET(dispatcher->pipefd[0], &rd_set)) {  // pipe can be read then stop running
            run = 0;
        } else {    // can read from socket
            enum socket_message message;
            char *path = NULL;
            int mode, remote_error = 0;
            if ((bytes = read_socket_message(fspipe_socket, &message, &path)) == -1) {
                perror("dispatcher - failed to read socket message");
            } else if (bytes > 0) {
                switch (message) {
                    case OPEN:
                        bytes = readn(fspipe_socket->fd_skt, &mode, sizeof(int));
                        if (bytes > 0) {
                            DEBUG("remote: OPEN %s %d\n", path, mode);
                            EQNULL(fspipe_file_open_remote(path, mode, 0), remote_error = errno; run = 0; break)

                            // send: OPEN_CONFIRM path mode remote_error
                            PTH(err, pthread_mutex_lock(&(fspipe_socket->writesktmtx)), run = 0; break)
                            bytes = write_socket_message(fspipe_socket->fd_skt, OPEN_CONFIRM, path, mode);
                            if (bytes > 0) {
                                bytes = writen(fspipe_socket->fd_skt, &remote_error, sizeof(int));
                            }
                            PTH(err, pthread_mutex_unlock(&(fspipe_socket->writesktmtx)), run = 0)
                            if (bytes > 0) DEBUG("sent: OPEN_CONFIRM %s %d %d\n", path, mode, remote_error);
                        }
                        break;
                    case OPEN_CONFIRM:
                        bytes = readn(fspipe_socket->fd_skt, &mode, sizeof(int));
                        if (bytes <= 0) break;

                        bytes = readn(fspipe_socket->fd_skt, &remote_error, sizeof(int));
                        if (bytes > 0) {
                            DEBUG("remote: OPEN_CONFIRM %s %d %d\n", path, mode, remote_error);
                            EQNULLERR(fspipe_file_open_remote(path, mode, remote_error), run = 0)
                        }
                        break;
                    case CLOSE:
                        bytes = readn(fspipe_socket->fd_skt, &mode, sizeof(int));
                        if (bytes > 0) {
                            DEBUG("remote: CLOSE %s %d\n", path, mode);
                            MINUS1ERR(fspipe_file_close(path, mode), run = 0)
                        }
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

struct dispatcher *fspipe_dispatcher_run(struct fspipe_socket *fspipe_data) {
    struct dispatcher *dispatcher = (struct dispatcher*) malloc(sizeof(struct dispatcher));
    EQNULL(dispatcher, return NULL)

    MINUS1(pipe(dispatcher->pipefd), return NULL)

    if (pthread_create(&(dispatcher->tid), NULL, &fspipe_dispatcher_fun, dispatcher) != 0) {
        free(dispatcher);
        return NULL;
    }

    dispatcher->fspipe_socket = fspipe_data;
    return dispatcher;
}

int fspipe_dispatcher_stop(struct dispatcher *dispatcher) {
    return close(dispatcher->pipefd[1]);
}

int fspipe_dispatcher_join(struct dispatcher *dispatcher, void *ret) {
    int r;
    PTH(r, pthread_join(dispatcher->tid, ret), return -1)
    return close(dispatcher->pipefd[0]);
}

void fspipe_dispatcher_free(struct dispatcher *dispatcher) {
    free(dispatcher);
}