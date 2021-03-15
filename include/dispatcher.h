#ifndef DISPATCHER_H
#define DISPATCHER_H

struct dispatcher {
    pthread_t tid;  // dispatcher's thread id
    int pipefd[2];  // used to communicate with main thread
};

struct dispatcher *netpipefs_dispatcher_run(void);

int netpipefs_dispatcher_stop(struct dispatcher *dispatcher);

int netpipefs_dispatcher_join(struct dispatcher *dispatcher, void *ret);

void netpipefs_dispatcher_free(struct dispatcher *dispatcher);

#endif //DISPATCHER_H
