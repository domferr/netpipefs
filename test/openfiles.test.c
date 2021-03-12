#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include "../include/openfiles.h"
#include "testutilities.h"
#include "../include/socketconn.h"

struct fspipe_options fspipe_options;
struct fspipe_socket fspipe_socket;

static void test_uninitialized_table(void);
static void test_file_operations_locally(void);

int main(int argc, char** argv) {
    fspipe_options.debug = 0; // disable debug printings

    test_uninitialized_table();
    testpassed("Operations on file with uninitialized open files table");
    test_file_operations_locally();
    testpassed("OPEN, CLOSE, READ and WRITE on local files");

    return 0;
}

/* All the operations with uninitialized table should fail and should set errno to EPERM */
static void test_uninitialized_table(void) {
    const char *path = "./filename.txt";

    /* Open the file locally for reading */
    test(fspipe_file_open_local(path, O_RDONLY) == NULL)
    test(errno == EPERM)
    errno = 0;

    /* Open the file locally for writing */
    test(fspipe_file_open_local(path, O_WRONLY) == NULL)
    test(errno == EPERM)
    errno = 0;

    /* Open the file remotely for reading */
    test(fspipe_file_open_remote(path, O_RDONLY) == NULL)
    test(errno == EPERM)
    errno = 0;

    /* Open the file remotely for writing */
    test(fspipe_file_open_remote(path, O_WRONLY) == NULL)
    test(errno == EPERM)
    errno = 0;

    /* Write locally */
    test(fspipe_file_write_local(path, NULL, 0) == -1)
    test(errno == EPERM)
    errno = 0;

    /* Read remotely */
    test(fspipe_file_read_remote(path, 0) == -1)
    test(errno == EPERM)
    errno = 0;

    /* Close the file remotely for reading */
    test(fspipe_file_close_remote(path, O_RDONLY) == -1)
    test(errno == EPERM)
    errno = 0;

    /* Close the file remotely for writing */
    test(fspipe_file_close_remote(path, O_WRONLY) == -1)
    test(errno == EPERM)
    errno = 0;
}

/* Test all the operations on files */
static void test_file_operations_locally(void) {
    const char *path = "./filename.txt";
    struct fspipe_file *file_read_remotely;
    struct fspipe_file *file_write_locally;

    // fake socket with a pipe
    int pipefd[2];
    pipe(pipefd);
    fspipe_socket.fd_skt = pipefd[1];

    /* Init open files table */
    test(fspipe_open_files_table_init() == 0)

    /* Open the file remotely for reading */
    test((file_read_remotely = fspipe_file_open_remote(path, O_RDONLY)) != NULL)

    /* Open the file locally for writing */
    test((file_write_locally = fspipe_file_open_local(path, O_WRONLY)) != NULL)

    /* Both structures should be the same */
    test(file_read_remotely == file_write_locally)

    /* Open both in reading and writing mode shouldn't be allowed */
    test(fspipe_file_open_remote(path, O_RDWR) == NULL)
    test(errno == EINVAL)
    errno = 0;

    /* Open both in reading and writing mode shouldn't be allowed */
    test(fspipe_file_open_local(path, O_RDWR) == NULL)
    test(errno == EINVAL)
    errno = 0;

    /* Write dummy data on file */
    const char *dummydata = "Testing\0";
    int datasize = (int) strlen(dummydata)+1;
    test(fspipe_file_write_local(path, (void*) dummydata, datasize) == datasize)

    /* Read from file */
    char *dataread = (char*) malloc(sizeof(char)*datasize);
    test(fspipe_file_read_local(file_read_remotely, dataread, datasize) == datasize)
    free(dataread);

    /* Close the file remotely for reading */
    test(fspipe_file_close_remote(path, O_RDONLY) == 0)

    /* Close the file locally for writing */
    test(fspipe_file_close_local(file_write_locally, O_WRONLY) > 0)

    /* Destroy open files table */
    test(fspipe_open_files_table_destroy() == 0)

    // close fake pipe
    close(pipefd[0]);
    close(pipefd[1]);
}