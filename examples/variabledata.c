/*
 * Example with one producer and one consumer but you can specify the number of bytes to be sent and read
 * and how many iterations should be done.
 *
 * Run the following command to build this example
 * gcc -Wall examples/variabledata.c src/scfiles.c -o bin/variabledata
 *
 * Example usage. Send 10Mb with 65Kb blocks and read them with 32 Kb blocks:
 * ./bin/variabledata 65536 160 32768 320
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

/**
 * Function executed by the producer process
 */
static int producer(char *arg1, char *arg2) {
    int wrote = 1, fd;
    char *endptr;
    size_t data_block, iterations;
    data_block = (int) strtol(arg1, &endptr, 10);
    if (endptr == arg1) return EXIT_FAILURE;
    iterations = (int) strtol(arg2, &endptr, 10);
    if (endptr == arg2) return EXIT_FAILURE;

    size_t datalen = data_block / sizeof(int);
    int dummydata[datalen];
    for (int i = 0; i < datalen; ++i) {
        dummydata[i] = i;
    }

    // Open file
    MINUS1ERR(fd = open(PRODUCER_FILEPATH, O_WRONLY), return EXIT_FAILURE)

    size_t i = 0;
    while(wrote > 0 && i < iterations) {
        wrote = writen(fd, dummydata, datalen);
        i++;
    }
    printf("producer -> wrote %ld bytes\n", sizeof(int) * datalen * i);
    if (wrote <= 0) perror("producer -> writen");

    /* Wait 1 second to leave some time to consumer to read all the data */
    sleep(1);

    // Close
    MINUS1ERR(close(fd), return EXIT_FAILURE);
    return 0;
}

/**
 * Function executed by the consumer process
 */
static int consumer(char *arg1, char *arg2) {
    int read = 1, fd;
    char *endptr;
    size_t data_block, iterations;
    data_block = (int) strtol(arg1, &endptr, 10);
    if (endptr == arg1) return EXIT_FAILURE;
    iterations = (int) strtol(arg2, &endptr, 10);
    if (endptr == arg2) return EXIT_FAILURE;

    size_t datalen = data_block / sizeof(int);
    int data[datalen];

    // Open file
    MINUS1ERR(fd = open(CONSUMER_FILEPATH, O_RDONLY), return EXIT_FAILURE)

    size_t i = 0;
    while(read > 0 && i < iterations) {
        read = readn(fd, data, datalen);
        i++;
    }
    printf("consumer -> read %ld bytes\n", sizeof(int) * datalen * i);
    if (read <= 0) perror("consumer -> readn");

    // Close
    MINUS1ERR(close(fd), return EXIT_FAILURE)
    return 0;
}

int main(int argc, char** argv) {
    int pid_cons;
    if (argc != 5) {
        fprintf(stderr, "usage: %s <prod_data_block> <prod_iterations> <cons_data_block> <cons_iterations>\n", argv[0]);
        return EXIT_SUCCESS;
    }

    // Fork a process to run the consumer
    MINUS1ERR(pid_cons = fork(), return EXIT_FAILURE)

    if (pid_cons == 0) {
        return consumer(argv[3], argv[4]);
    }

    int ret = producer(argv[1], argv[2]);

    MINUS1(waitpid(pid_cons, NULL, 0), fprintf(stderr, "failure to wait pid %d: ", pid_cons); perror(""); return EXIT_FAILURE)
    return ret;
}