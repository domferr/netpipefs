/*
 * Example with one producer and one consumer. The producer writes MAXNUMBERS numbers
 * into PRODUCER_FILEPATH. The consumer reads from CONSUMER_FILEPATH an array of
 * MAXNUMBERS numbers. Both producer and consumer write/read to a file with the same
 * name but the directory is different so the files are not exactly the same.
 * The consumer reads in non blocking mode. It sleeps for 1 second when a read returns
 * EAGAIN.
 *
 * Run the following command to build this example
 * gcc -Wall examples/nonblockingio.c src/scfiles.c -o bin/nonblockingio
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
#define MAXNUMBERS 10

/**
 * Function executed by the send_data process
 */
static int producer() {
    int fd, prec1 = 0, prec2 = 1, next = 1, counter = 2, maxnumbers = MAXNUMBERS;

    // Open file
    MINUS1ERR(fd = open(PRODUCER_FILEPATH, O_WRONLY), return EXIT_FAILURE)

    // Send the first MAXNUMBERS
    ISNEGATIVEERR(writen(fd, &maxnumbers, sizeof(int)), return EXIT_FAILURE)
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

    // Close
    MINUS1ERR(close(fd), return EXIT_FAILURE);
    return 0;
}

static int interval_open(const char *path, int mode) {
    int res;
    do {
        res = open(path, mode);
        if (res == -1 && errno == EAGAIN) {
            res = (int) sleep(1); // sleep for 1s
            if (res == -1) break; // sleep error
        } else {
            break;
        }
    } while(1);

    return res;
}

static ssize_t interval_read(int fd, void *ptr, size_t n) {
    ssize_t res;
    do {
        res = readn(fd, ptr, n);
        if (res == -1 && errno == EAGAIN) {
            res = (int) sleep(1); // sleep for 500 ms
            if (res == -1) break; // sleep error
        } else {
            break;
        }
    } while(1);

    return res;
}

/**
 * Function executed by the recv_data process
 */
static int consumer() {
    int fd, read, maxnumbers;

    // Open file
    MINUS1ERR(fd = interval_open(CONSUMER_FILEPATH, O_RDONLY | O_NONBLOCK), return EXIT_FAILURE)

    // Read from it
    ISNEGATIVEERR(read = interval_read(fd, &maxnumbers, sizeof(int)), return EXIT_FAILURE)
    if (read == 0) {
        printf("End of file\n");
        return 0;
    }
    printf("max numbers = %d\n", maxnumbers);
    int *numbers = (int*) malloc(sizeof(int)*maxnumbers);
    EQNULL(numbers, perror("malloc"); return EXIT_FAILURE)

    ISNEGATIVEERR(read = interval_read(fd, numbers, sizeof(int)*maxnumbers), return EXIT_FAILURE)
    if (read == 0) {
        printf("End of file\n");
        return 0;
    }
    printf("Read ");
    for (int i = 0; i < maxnumbers; ++i) {
        printf("%d ", numbers[i]);
    }
    printf("from %s\n", CONSUMER_FILEPATH);

    // Close
    MINUS1ERR(close(fd), return EXIT_FAILURE)
    return 0;
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