#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <stdlib.h>
#include "../include/options.h"
#include "../include/dispatcher.h"
#include "../include/utils.h"

void *fspipe_dispatcher_fun(void *args) {
    //struct fspipe_data *fspipe_data = (struct fspipe_data*) args;
    DEBUG("%s\n", "dispatcher is running");

    /*
     * 1. stabilisci connessione via socket
     * 2. imposta il socket e sveglia tutti
     * 3. leggi dal socket
     * 4. se va tutto bene allora gestisci quello che hai letto, altrimenti esci e manda CTRL+C al processo main
     * 5. ritorna a 3
     */

    //sleep(20);

    DEBUG("%s\n", "dispatcher stopped running");
    return 0;
}

struct dispatcher *fspipe_dispatcher_run(void) {
    struct dispatcher *dispatcher = (struct dispatcher*) malloc(sizeof(struct dispatcher));
    EQNULL(dispatcher, return NULL)

    if (pthread_create(&(dispatcher->tid), NULL, &fspipe_dispatcher_fun, dispatcher) != 0) {
        free(dispatcher);
        return NULL;
    }

    return dispatcher;
}

int fspipe_dispatcher_stop(struct dispatcher *dispatcher) {
    return 0;
}

int fspipe_dispatcher_join(struct dispatcher *dispatcher, void *ret) {
    int r;
    PTH(r, pthread_join(dispatcher->tid, NULL), return -1)
    return 0;
}

void fspipe_dispatcher_free(struct dispatcher *dispatcher) {
    free(dispatcher);
}