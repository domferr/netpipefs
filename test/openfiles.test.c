#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include "../include/openfiles.h"
#include "testutilities.h"
#include "../include/socketconn.h"

struct fspipe_options fspipe_options;
struct fspipe_socket fspipe_socket;

static void test_unitialized_table(void);
static void test_file_operations(void);

int main(int argc, char** argv) {

    test_unitialized_table();
    testpassed("Operations on file with unitialized open files table will fail and set errno to EPERM");

    test_file_operations();
    testpassed("OPEN, CLOSE, READ and WRITE on files");

    return 0;
}

/* All the operations with unitialized table should fail and should set errno to EPERM */
static void test_unitialized_table(void) {
    const char *path = "./filename.txt";

    /* Open the file locally for reading */
    test(fspipe_file_open_local(path, O_RDONLY) == NULL)
    test(errno == EPERM)

    /* Open the file locally for writing */
    test(fspipe_file_open_local(path, O_WRONLY) == NULL)
    test(errno == EPERM)

    /* Open the file remotely for reading */
    test(fspipe_file_open_remote(path, O_RDONLY) == NULL)
    test(errno == EPERM)

    /* Open the file remotely for writing */
    test(fspipe_file_open_remote(path, O_WRONLY) == NULL)
    test(errno == EPERM)

    /* Write locally */
    test(fspipe_file_write_local(path, NULL, 0) == -1)
    test(errno == EPERM)

    /* Read remotely */
    test(fspipe_file_read_remote(path, 0) == -1)
    test(errno == EPERM)

    /* Close the file remotely for reading */
    test(fspipe_file_close_remote(path, O_RDONLY) == -1)
    test(errno == EPERM)

    /* Close the file remotely for writing */
    test(fspipe_file_close_remote(path, O_WRONLY) == -1)
    test(errno == EPERM)

    /* Reset errno */
    errno = 0;
}

/* Test all the operations on files */
static void test_file_operations(void) {
    const char *path = "./filename.txt";
    struct fspipe_file *file_read_remotely;
    struct fspipe_file *file_write_locally;

    // fake socket with a pipe
    int pipefd[2];
    pipe(pipefd);
    fspipe_socket.fd_skt = pipefd[1];

    /* Init open files table */
    test(fspipe_open_files_table_init() == 0)
    test(errno == 0)

    /* Open the file remotely for reading */
    test((file_read_remotely = fspipe_file_open_remote(path, O_RDONLY)) != NULL)
    test(errno == 0)

    /* Open the file locally for writing */
    test((file_write_locally = fspipe_file_open_local(path, O_WRONLY)) != NULL)
    test(errno == 0)

    /* Both structures should be the same */
    test(file_read_remotely == file_write_locally)
    test(errno == 0)

    /* Close the file remotely for reading */
    test(fspipe_file_close_remote(path, O_RDONLY) == 0)
    test(errno == 0)

    /* Close the file locally for writing */
    test(fspipe_file_close_local(file_write_locally, O_WRONLY) > 0)
    test(errno == 0)

    /* Destroy open files table */
    test(fspipe_open_files_table_destroy() == 0)
    test(errno == 0)

    // close fake pipe
    close(pipefd[0]);
    close(pipefd[1]);
}