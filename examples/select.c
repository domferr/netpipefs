/*
 * Example with one producer and one consumer. The producer writes MAXNUMBERS numbers
 * into PRODUCER_FILEPATH. The consumer reads from CONSUMER_FILEPATH an array of
 * MAXNUMBERS numbers. Both producer and consumer write/read to a file with the same
 * name but the directory is different so the files are not exactly the same.
 *
 * Run the following command to build this example
 * gcc -Wall examples/select.c src/scfiles.c -o bin/select
 *
 */

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <wait.h>
#include <errno.h>
#include "../include/utils.h"
#include "../include/scfiles.h"

#define PRODUCER_FILEPATH "./tmp/prod/mypipe.txt"
#define CONSUMER_FILEPATH "./tmp/cons/mypipe.txt"
#define MAXNUMBERS 20

/**
 * Function executed by the send_data process
 */
static int producer() {
    int fd, prec1 = 0, prec2 = 1, next = 1, counter = 2;

    // Open file
    MINUS1ERR(fd = open(PRODUCER_FILEPATH, O_WRONLY), return EXIT_FAILURE)

    printf("Writing %d %d ", prec1, prec2);
    ISNEGATIVEERR(writen(fd, &prec1, sizeof(int)), return EXIT_FAILURE)
    ISNEGATIVEERR(writen(fd, &prec2, sizeof(int)), return EXIT_FAILURE)

    while (counter < MAXNUMBERS) {
        ISNEGATIVEERR(writen(fd, &next, sizeof(int)), return EXIT_FAILURE)
        printf("%d ", next);

        prec1 = prec2;
        prec2 = next;
        next = prec1 + prec2;
        counter++;
    }
    printf("\n");

    sleep(2);
    // Close
    MINUS1ERR(close(fd), return EXIT_FAILURE);
    return 0;
}

/**
 * Function executed by the recv_data process
 */
static int consumer() {
    int fd, read = 1, number;
    fd_set set, rset;

    // Open file
    MINUS1ERR(fd = open(CONSUMER_FILEPATH, O_RDONLY), return EXIT_FAILURE)

    FD_ZERO(&set);
    FD_SET(fd, &set);

    while (read > 0) {
        rset = set;
        printf("select\n");
        read = select(fd+1, &rset, NULL, NULL, NULL);
        if (read == 0)
            errno = ETIMEDOUT;
        else if (read > 0) {
            // Read from netpipe
            read = readn(fd, &number, sizeof(int));
            if (read > 0) printf("num is %d ", number);
        }
    }
    if (read == 0) {
        printf("\n");
    } else if (read == -1) {
        perror("consumer read");
        return EXIT_FAILURE;
    }
    // close netpipe
    MINUS1ERR(close(fd), return EXIT_FAILURE)

    return EXIT_SUCCESS;
}

int main(int argc, char** argv) {
    int pid_cons;

    // Fork a process to run the consumer
    MINUS1ERR(pid_cons = fork(), return EXIT_FAILURE)

    if (pid_cons == 0) {
        return consumer();
    }

    int ret = producer();
    MINUS1(waitpid(pid_cons, NULL, 0), fprintf(stderr, "failure to wait pid %d: ", pid_cons); perror(""); return EXIT_FAILURE)
    return ret;
}