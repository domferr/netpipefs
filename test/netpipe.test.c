#include <unistd.h>
#include "testutilities.h"
#include "../include/netpipe.h"
#include "../include/dispatcher.h"
#include "../include/netpipefs_socket.h"
#include "../include/openfiles.h"

struct netpipefs_socket netpipefs_socket;

static void test_nonblock_operations(void);

int main(int argc, char** argv) {
    netpipefs_options.debug = 0;
    test(netpipefs_open_files_table_init() == 0)

    test_nonblock_operations();
    test(netpipefs_dispatcher_run() == 0)
    test(netpipefs_dispatcher_stop() == 0)

    test(netpipefs_open_files_table_destroy() == 0)

    testpassed("Netpipe");
    return 0;
}

static void test_nonblock_operations(void) {
    /*struct netpipe *netpipe;
    size_t local_writeahead = 4096;
    size_t old_writeahead = netpipefs_options.pipecapacity;
    size_t old_readahead = netpipefs_socket.remotepipecapacity;
    ssize_t bytes;
    netpipefs_options.pipecapacity = local_writeahead;
    netpipefs_socket.remotepipecapacity = 0;*/

    /* Open netpipe */
    /*netpipe = netpipe_open("./netpipe", O_WRONLY, 1);
    test(netpipe != NULL)*/

    /* Send data. It will writeahead */
    /*char dummydata[local_writeahead];
    for(size_t i=0; i<local_writeahead; i++) dummydata[i] = (char)(i);
    bytes = 100;
    test(netpipe_send(netpipe, dummydata, bytes, 1) == bytes)*/

    /* Close netpipe */
    /*test(netpipe_close(netpipe, O_WRONLY) == 0)
    netpipefs_options.pipecapacity = old_writeahead;
    netpipefs_socket.remotepipecapacity = old_readahead;*/
}