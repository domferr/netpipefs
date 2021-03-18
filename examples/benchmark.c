/*
 * Example with one producer and one consumer but you can specify the number of bytes to be sent and read
 * and how many iterations should be done.
 *
 * Run the following command to build this example
 * gcc -Wall examples/benchmark.c src/scfiles.c src/utils.c -o bin/benchmark -lpthread
 *
 * Example usage. Send 10Mb with 65Kb blocks and read them with 32 Kb blocks:
 * ./bin/benchmark 65536 160 32768 320
 */

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <sys/stat.h>
#include "../include/utils.h"
#include "../include/scfiles.h"
#include <sys/socket.h>
#include <sys/un.h>

#define PRODUCER_FILEPATH "./tmp/prod/mypipe.txt"
#define CONSUMER_FILEPATH "./tmp/cons/mypipe.txt"
#define NAMEDPIPE_FILEPATH "/tmp/namedpipe.txt"
#define SOCKET_FILEPATH "/tmp/socketbench.sock"

typedef struct arg {
    int fd;
    const char *path;
    size_t data_block;
    size_t iterations;
    struct timespec bench;
    int error;
} arg_t;

#define ARG_INIZIALIZER_FD(fd, datablock, iterations) { fd, NULL, datablock, iterations, {-1,-1}, 0 }
#define ARG_INIZIALIZER(path, datablock, iterations) { -1, path, datablock, iterations, {-1,-1}, 0 }

/* Converts a timespec to a fractional number of seconds.
 * From: https://github.com/solemnwarning/timespec/blob/master/timespec.c
 */
#define TIMESPEC_TO_DOUBLE(ts) ((double)((ts).tv_sec) + ((double)((ts).tv_nsec) / 1000000000))

#define LOGBENCH(name, write, read) \
    fprintf(stdout, "[%s] write: %fs, read: %fs\n", \
        name, TIMESPEC_TO_DOUBLE(write), TIMESPEC_TO_DOUBLE(read))

static arg_t *arg_alloc(int fd, const char *path, size_t datablock, size_t iter) {
    arg_t *arg = (arg_t *) malloc(sizeof(arg_t));
    if (arg == NULL) return NULL;

    arg->fd = fd;
    arg->path = path;
    arg->data_block = datablock;
    arg->iterations = iter;
    arg->bench.tv_sec = -1;
    arg->bench.tv_nsec = -1;
    arg->error = 0;

    return arg;
}

/**
 * Function executed by the send_data process
 */
static int send_data(arg_t *arg) {
    struct timespec start;
    int wrote = 1, doclose = 0;
    arg->bench.tv_sec = -1;
    arg->bench.tv_nsec = -1;
    arg->error = 0;

    if (arg->path != NULL) {
        doclose = 1;
        arg->fd = open(arg->path, O_WRONLY);
    }
    if (doclose && arg->fd == -1) {
        arg->error = errno;
        return 0;
    }

    size_t datalen = arg->data_block / sizeof(int);
    int dummydata[datalen];
    for (int i = 0; i < datalen; ++i) {
        dummydata[i] = i;
    }

    /* Get current time */
    if (clock_gettime(CLOCK_MONOTONIC, &start) == -1) {
        arg->error = errno;
        if (doclose) close(arg->fd);
        return 0;
    }

    size_t i = 0;
    while(wrote > 0 && i < arg->iterations) {
        wrote = writen(arg->fd, dummydata, datalen);
        i++;
    }

    if (wrote > 0) {
        /* Get elapsed time from start to now */
        arg->bench = elapsed_time(&start);
        if (arg->bench.tv_sec == -1) {
            arg->error = errno;
            if (doclose) close(arg->fd);
            return 0;
        }
    } else {
        arg->bench.tv_sec = -1;
        arg->bench.tv_nsec = -1;
        arg->error = errno;
    }

    if (doclose) close(arg->fd);

    return wrote;
}

/**
 * Function executed by the consumer thread
 */
static void *recv_data(void *arguments) {
    struct timespec start;
    arg_t *arg = (arg_t*) arguments;
    arg->bench.tv_sec = -1;
    arg->bench.tv_nsec = -1;
    arg->error = 0;
    int read = 1, doclose = 0;

    if (arg->path != NULL) {
        doclose = 1;
        arg->fd = open(arg->path, O_RDONLY);
    }
    if (arg->fd == -1) {
        arg->error = errno;
        return 0;
    }

    size_t datalen = arg->data_block / sizeof(int);
    int data[datalen];

    /* Get current time */
    if (clock_gettime(CLOCK_MONOTONIC, &start) == -1) {
        arg->error = errno;
        if (doclose) close(arg->fd);
        return 0;
    }

    size_t i = 0;
    while(read > 0 && i < arg->iterations) {
        read = readn(arg->fd, data, datalen);
        i++;
    }

    if (read > 0) {
        /* Get elapsed time from start to now */
        arg->bench = elapsed_time(&start);
        if (arg->bench.tv_nsec == -1) {
            arg->error = errno;
            if (doclose) close(arg->fd);
            return 0;
        }
    } else {
        arg->bench.tv_sec = -1;
        arg->bench.tv_nsec = -1;
        arg->error = errno;
    }

    if (doclose) close(arg->fd);

    return 0;
}

static int with_netpipe(size_t prod_data_block, size_t prod_iterations, size_t cons_data_block, size_t cons_iterations) {
    int err, prod_ret;
    pthread_t pcons;

    arg_t *cons_arg = arg_alloc(-1, CONSUMER_FILEPATH, cons_data_block, cons_iterations);
    if (cons_arg == NULL) return -1;

    // Create thread for receiving data (consumer)
    PTHERR(err, pthread_create(&pcons, NULL, &recv_data, cons_arg), return -1)

    // Send data (producer)
    arg_t prod_arg = ARG_INIZIALIZER(PRODUCER_FILEPATH, prod_data_block, prod_iterations);
    prod_ret = send_data(&prod_arg);
    if (prod_ret <= 0) perror("[NETPIPE] send_data failure");

    // Join consumer thread
    PTHERR(err, pthread_join(pcons, NULL), return -1)

    // Print consumer error
    if (cons_arg->error != 0) fprintf(stderr, "[NETPIPE] recv_data failure: %s\n", strerror(cons_arg->error));

    if (cons_arg->error == 0 && prod_ret > 0)
        LOGBENCH("NETPIPE", prod_arg.bench, cons_arg->bench);

    free(cons_arg);
    return 0;
}

static int with_namedpipe(size_t prod_data_block, size_t prod_iterations, size_t cons_data_block, size_t cons_iterations) {
    int err, prod_ret;
    pthread_t pcons;

    // Create named pipe
    MINUS1ERR(mkfifo(NAMEDPIPE_FILEPATH, 0666), return -1)

    arg_t *cons_arg = arg_alloc(-1, NAMEDPIPE_FILEPATH, cons_data_block, cons_iterations);
    if (cons_arg == NULL) return -1;

    // Create thread for receiving data (consumer)
    PTHERR(err, pthread_create(&pcons, NULL, &recv_data, cons_arg), unlink(NAMEDPIPE_FILEPATH); return -1)

    // Send data (producer)
    arg_t prod_arg = ARG_INIZIALIZER(NAMEDPIPE_FILEPATH, prod_data_block, prod_iterations);
    prod_ret = send_data(&prod_arg);
    if (prod_ret <= 0) perror("[NAMED PIPE] send_data failure");

    // Join consumer thread
    PTHERR(err, pthread_join(pcons, NULL), unlink(NAMEDPIPE_FILEPATH); return -1)

    // Print consumer error
    if (cons_arg->error != 0) fprintf(stderr, "[NAMED PIPE] recv_data failure: %s\n", strerror(cons_arg->error));

    LOGBENCH("NAMED PIPE", prod_arg.bench, cons_arg->bench);

    // Unlink named pipe
    unlink(NAMEDPIPE_FILEPATH);

    free(cons_arg);
    return 0;
}

static int with_socket(size_t prod_data_block, size_t prod_iterations, size_t cons_data_block, size_t cons_iterations) {
    int err, prod_ret, prodfd, consfd, srvfd;
    struct sockaddr_un sa;
    pthread_t pcons;

    // sock address
    strncpy(sa.sun_path, SOCKET_FILEPATH, 108);
    sa.sun_family = AF_UNIX;
    // bind and listen
    MINUS1ERR(srvfd = socket(AF_UNIX, SOCK_STREAM, 0), return -1)
    MINUS1ERR(bind(srvfd, (struct sockaddr *) &sa, sizeof(sa)), close(srvfd); return -1)
    MINUS1ERR(listen(srvfd, SOMAXCONN), close(srvfd); return -1)
    // connect
    MINUS1ERR(prodfd = socket(AF_UNIX, SOCK_STREAM, 0), close(srvfd); return -1)
    MINUS1ERR(connect(prodfd, (struct sockaddr *) &sa, sizeof(sa)), close(srvfd); close(prodfd); return -1)
    // accept
    MINUS1ERR(consfd = accept(srvfd, NULL, 0), close(srvfd); close(prodfd); return -1)
    close(srvfd); // not needed to accept anymore

    arg_t *cons_arg = arg_alloc(consfd, NULL, cons_data_block, cons_iterations);
    if (cons_arg == NULL) return -1;

    // Create thread for receiving data (consumer)
    PTHERR(err, pthread_create(&pcons, NULL, &recv_data, cons_arg), return -1)

    // Send data (producer)
    arg_t prod_arg = ARG_INIZIALIZER_FD(prodfd, prod_data_block, prod_iterations);
    prod_ret = send_data(&prod_arg);
    if (prod_ret <= 0) perror("[SOCKET] send_data failure");

    // Join consumer thread
    PTHERR(err, pthread_join(pcons, NULL), return -1)

    // Print consumer error
    if (cons_arg->error != 0) fprintf(stderr, "[SOCKET] recv_data failure: %s\n", strerror(cons_arg->error));

    if (cons_arg->error == 0 && prod_ret > 0)
        LOGBENCH("SOCKET", prod_arg.bench, cons_arg->bench);

    // Close sockets and unlink file
    close(prodfd);
    close(consfd);
    unlink(sa.sun_path);

    free(cons_arg);
    return 0;
}

/** From string to integer. Returns -1 on error */
static int str_to_int(char *str) {
    char *endptr;
    int val = (int) strtol(str, &endptr, 10);
    return endptr == str ? -1:val;
}

int main(int argc, char** argv) {
    int ret;
    if (argc != 5) {
        fprintf(stderr, "usage: %s <prod_data_block> <prod_iterations> <cons_data_block> <cons_iterations>\n", argv[0]);
        return EXIT_SUCCESS;
    }

    size_t prod_data_block, prod_iterations, cons_data_block, cons_iterations;

    if ((prod_data_block = (size_t) str_to_int(argv[1])) == -1) return EXIT_FAILURE;
    if ((prod_iterations = (size_t) str_to_int(argv[2])) == -1) return EXIT_FAILURE;
    if ((cons_data_block = (size_t) str_to_int(argv[3])) == -1) return EXIT_FAILURE;
    if ((cons_iterations = (size_t) str_to_int(argv[4])) == -1) return EXIT_FAILURE;

    ret = with_namedpipe(prod_data_block, prod_iterations, cons_data_block, cons_iterations);
    if (ret == -1) return EXIT_FAILURE;

    ret = with_netpipe(prod_data_block, prod_iterations, cons_data_block, cons_iterations);
    if (ret == -1) return EXIT_FAILURE;

    ret = with_socket(prod_data_block, prod_iterations, cons_data_block, cons_iterations);
    if (ret == -1) return EXIT_FAILURE;

    return 0;
}