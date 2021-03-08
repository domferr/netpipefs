#ifndef DISPATCHER_H
#define DISPATCHER_H

struct dispatcher {
    struct fspipe_socket *fspipe_socket;
    pthread_t tid;  // dispatcher's thread id
    int pipefd[2];  // used to communicate with main thread
};

struct dispatcher *fspipe_dispatcher_run(struct fspipe_socket *fspipe_data);

int fspipe_dispatcher_stop(struct dispatcher *dispatcher);

int fspipe_dispatcher_join(struct dispatcher *dispatcher, void *ret);

void fspipe_dispatcher_free(struct dispatcher *dispatcher);

#endif //DISPATCHER_H
