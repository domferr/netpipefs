/*
 * Example with one producer and one consumer
 *
 * gcc -Wall examples/simpleprodcons.c src/scfiles.c -o bin/simpleprodcons
 */

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <wait.h>
#include "../include/utils.h"
#include "../include/scfiles.h"

#define PRODUCER_FILEPATH "./tmp/prod/mypipe.txt"
#define CONSUMER_FILEPATH "./tmp/cons/mypipe.txt"
#define MAXNUMBERS 10

/**
 * Function executed by the producer process
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
    next = -1;
    ISNEGATIVEERR(writen(fd, &next, sizeof(int)), return EXIT_FAILURE)
    printf("-1 into %s\n", PRODUCER_FILEPATH);

    // Close
    MINUS1ERR(close(fd), return EXIT_FAILURE);
    return 0;
}

/**
 * Function executed by the consumer process
 */
static int consumer() {
    int fd, read, maxnumbers;

    // Open file
    MINUS1ERR(fd = open(CONSUMER_FILEPATH, O_RDONLY), return EXIT_FAILURE);

    // Read from it
    ISNEGATIVEERR(read = readn(fd, &maxnumbers, sizeof(int)), return EXIT_FAILURE)
    if (read == 0) {
        printf("End of file\n");
        return 0;
    }
    int *numbers = (int*) malloc(sizeof(int)*maxnumbers);
    EQNULL(numbers, perror("malloc"); return EXIT_FAILURE)

    ISNEGATIVEERR(read = readn(fd, numbers, sizeof(int)*maxnumbers), return EXIT_FAILURE)
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