#ifndef DISPATCHER_H
#define DISPATCHER_H

struct fspipe_data {
    int fd_server;  // used to accept socket connections
    int fd_skt;     // used to communicate via sockets
};

struct dispatcher {
    pthread_t tid;
    int *pipefd;
};

void *fspipe_dispatcher_fun(void *args);

struct dispatcher *fspipe_dispatcher_run(void);

int fspipe_dispatcher_stop(struct dispatcher *dispatcher);

int fspipe_dispatcher_join(struct dispatcher *dispatcher, void *ret);

void fspipe_dispatcher_free(struct dispatcher *dispatcher);

#endif //DISPATCHER_H
