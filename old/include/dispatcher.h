#ifndef DISPATCHER_H
#define DISPATCHER_H

#include "./netpipe.h"

/**
 * Run dispatcher thread
 * @return 0 on success, -1 on error
 */
int netpipefs_dispatcher_run(void);

/**
 * Stop dispatcher thread
 * @return 0 on success, -1 on error
 */
int netpipefs_dispatcher_stop(void);

#endif //DISPATCHER_H
