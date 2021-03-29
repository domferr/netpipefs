#include "../include/signal_handler.h"
#include "../include/utils.h"
#include <pthread.h>
#include <errno.h>
#include <stdio.h>

/* Fuse channel. Used to unmount the filesystem when a signal arrives */
struct fuse_chan *chan;
/* Thread signal handler */
pthread_t sig_handler_tid;

/** Function executed by the signal handler. The argument is the set of signal that should be caught */
static void *signal_handler_thread(void *arg) {
    sigset_t *set = arg;
    int err, sig;

    // wait for SIGINT or SIGTERM
    PTHERR(err, sigwait(set, &sig), return NULL)

    // TODO close all files

    // unmount the filesystem
    if (netpipefs_options.mountpoint && chan)
        fuse_unmount(netpipefs_options.mountpoint, chan);

    return 0;
}

int netpipefs_set_signal_handlers(sigset_t *set, struct fuse_chan *ch) {
    int err;

    /* Set SIGINT, SIGTERM */
    MINUS1(sigemptyset(set), return -1)
    MINUS1(sigaddset(set, SIGINT), return -1)
    MINUS1(sigaddset(set, SIGTERM), return -1)
    MINUS1(sigaddset(set, SIGPIPE), return -1)

    /* Block SIGINT, SIGTERM and SIGPIPE for main thread */
    PTH(err, pthread_sigmask(SIG_BLOCK, set, NULL), return -1)

    /* Do not handle SIGPIPE */
    MINUS1(sigdelset(set, SIGPIPE), return -1)

    chan = ch;
    /* Run signal handler thread */
    PTH(err, pthread_create(&sig_handler_tid, NULL, &signal_handler_thread, set), if (chan) chan = NULL; return -1)

    //PTH(err, pthread_detach(sig_handler_tid), return -1)

    return 0;
}

int netpipefs_remove_signal_handlers(void) {
    int err;

    // Send SIGINT to stop the thread
    if ((err = pthread_kill(sig_handler_tid, SIGINT)) != 0) {
        if (err == ESRCH) return 0; // already ended
        errno = err;
        return -1;
    }

    PTH(err, pthread_join(sig_handler_tid, NULL), return -1)
    return 0;
}