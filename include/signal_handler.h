#ifndef NETPIPEFS_SIGNAL_HANDLER_H
#define NETPIPEFS_SIGNAL_HANDLER_H

#include "options.h" // needed for fuse version
#include <signal.h>

int netpipefs_set_signal_handlers(sigset_t *set, struct fuse_chan *ch, struct fuse *fu);

int netpipefs_remove_signal_handlers(void);

#endif //NETPIPEFS_SIGNAL_HANDLER_H
