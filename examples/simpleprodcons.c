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
#define READ_END 0
#define WRITE_END 1

/**
 * Function executed by the producer process
 */
static int producer(int fd) {
    int prec1 = 0, prec2 = 1, next = 1, counter = 2, maxnumbers = MAXNUMBERS;

    // Open file
    MINUS1ERR(fd = open(PRODUCER_FILEPATH, O_WRONLY), return 1);

    // Send the first MAXNUMBERS
    ISNEGATIVEERR(writen(fd, &maxnumbers, sizeof(int)), return 1)
    printf("Writing %d %d ", prec1, prec2);
    ISNEGATIVEERR(writen(fd, &prec1, sizeof(int)), return 1)
    ISNEGATIVEERR(writen(fd, &prec2, sizeof(int)), return 1)

    while (counter < MAXNUMBERS) {
        ISNEGATIVEERR(writen(fd, &next, sizeof(int)), return 1)
        printf("%d ", next);

        prec1 = prec2;
        prec2 = next;
        next = prec1 + prec2;
        counter++;
    }
    next = -1;
    ISNEGATIVEERR(writen(fd, &next, sizeof(int)), return 1)
    printf("-1 into %s\n", PRODUCER_FILEPATH);

    // Close
    return close(fd);
}

/**
 * Function executed by the consumer process
 */
static int consumer(int fd) {
    int read, maxnumbers;

    // Open file
    MINUS1ERR(fd = open(CONSUMER_FILEPATH, O_RDONLY), return 1);

    // Read from it
    ISNEGATIVEERR(read = readn(fd, &maxnumbers, sizeof(int)), return 1)
    if (read == 0) {
        printf("End of file\n");
        return 0;
    }
    int *numbers = (int*) malloc(sizeof(int)*maxnumbers);
    EQNULL(numbers, perror("malloc"); return 1);
    ISNEGATIVEERR(read = readn(fd, numbers, sizeof(int)*maxnumbers), return 1)
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
    return close(fd);
}

int main(int argc, char** argv) {
    int pid_cons;
    int pipefd[2] = { 0,0 };

    // Create an unnamed pipe
    //MINUS1ERR(pipe(pipefd), return 1)

    // Fork a process to run the consumer
    MINUS1ERR(pid_cons = fork(), return 1)

    if (pid_cons > 0) {
        return consumer(pipefd[READ_END]);
    }

    NOTZERO(producer(pipefd[WRITE_END]), perror("consumer"))

    // Close the unnamed pipe
    //MINUS1(close(pipefd[READ_END]), perror("close pipe's read end"))
    //MINUS1(close(pipefd[WRITE_END]), perror("close pipe's write end"))

    return waitpid(pid_cons, NULL, 0);
}