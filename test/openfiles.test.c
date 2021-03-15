#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include "../include/openfiles.h"
#include "testutilities.h"
#include "../include/socketconn.h"

struct netpipefs_options netpipefs_options;
struct netpipefs_socket netpipefs_socket;

static void test_uninitialized_table(void);
static void test_openfiles_table(void);

int main(int argc, char** argv) {
    netpipefs_options.debug = 0; // disable debug printings
    netpipefs_options.pipecapacity = 1024;
    netpipefs_socket.remotepipecapacity = 512;

    test_uninitialized_table();
    testpassed("Operations on uninitialized open files table");

    test_openfiles_table();
    testpassed("Open files hash table");

    return 0;
}

/* All the operations with uninitialized table should fail and should set errno to EPERM */
static void test_uninitialized_table(void) {
    const char *path = "./filename.txt";

    /* Get open file */
    test(netpipefs_get_open_file(path) == NULL)
    test(errno == EPERM)
    errno = 0;

    /* Remove open file */
    test(netpipefs_remove_open_file(path) == -1)
    test(errno == EPERM)
    errno = 0;

    /* Get or create if missing */
    int just_created = 0;
    test(netpipefs_get_or_create_open_file(path, &just_created) == NULL)
    test(errno == EPERM)
    errno = 0;
}

/* All the operations on the open files hash table */
static void test_openfiles_table(void) {
    const char *path = "./filename.txt";
    struct netpipefs_file *file;

    // fake socket with a pipe
    int pipefd[2];
    pipe(pipefd);
    netpipefs_socket.fd_skt = pipefd[1];

    /* Init open files table */
    test(netpipefs_open_files_table_init() == 0)

    /* Get or create if missing */
    int just_created = 0;
    test((file = netpipefs_get_or_create_open_file(path, &just_created)) != NULL)
    test(just_created == 1)

    /* Get open file */
    test(netpipefs_get_open_file(path) == file)

    /* Remove open file */
    test(netpipefs_remove_open_file(path) == 0)

    /* Remove not open file */
    test(netpipefs_remove_open_file("badpath") == -1)

    /* Destroy open files table */
    test(netpipefs_open_files_table_destroy() == 0)

    close(pipefd[0]);
    close(pipefd[1]);
}
