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
#include "../include/utils.h"
#include "../include/scfiles.h"

static size_t startbs = 1024;
static size_t maxbs = 4096;
static size_t writers = 1;
static size_t readers = 1;
static int can_send = 1;

static pthread_mutex_t writersmtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t writerwait = PTHREAD_COND_INITIALIZER;

static pthread_mutex_t readersmtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t readerwait = PTHREAD_COND_INITIALIZER;
static size_t readers_ready = 0;
static size_t readers_done = 0;
static size_t can_read = 0;

/* Converts a timespec to a fractional number of seconds.
 * From: https://github.com/solemnwarning/timespec/blob/master/timespec.c
 */
#define TIMESPEC_TO_DOUBLE(ts) ((double)((ts).tv_sec) + ((double)((ts).tv_nsec) / 1000000000))

/** From string to integer. Returns -1 on error */
static int str_to_int(char *str) {
    char *endptr;
    int val = (int) strtol(str, &endptr, 10);
    return endptr == str ? -1:val;
}

typedef struct arg_s {
    pthread_t tid;
    int error;
    int (*fun)(char *, size_t); // function to be executed and measured
    char *path;
} arg_t;

static int send_data(int fd, size_t size) {
    char *buf = (char*) malloc(sizeof(char) * size);
    if (buf == NULL) return -1;
    int err = writen(fd, buf, size);
    free(buf);

    return err;
}

static int recv_data(int fd, size_t size) {
    char *buf = (char*) malloc(sizeof(char) * size);
    if (buf == NULL) return -1;
    int err = readn(fd, buf, size);
    free(buf);

    return err;
}

static void log_bench(int isread, size_t size, double seconds) {
    static const char *SIZES[] = { "B", "KB", "MB", "GB" };
    size_t div = 0;
    size_t rem = 0;
    size_t tmpsize = size;
    float megabytes = (float) size / (1024*1024), convsize;

    while (tmpsize >= 1024 && div < (sizeof SIZES / sizeof *SIZES)) {
        rem = (tmpsize % 1024);
        div++;
        tmpsize /= 1024;
    }
    convsize = (float)tmpsize + (float)rem / 1024.0;

    printf("%ld bytes %s (%.1f %s), %.5f s, %.2f MB/s\n", size, (isread ? "read":"written"), convsize, SIZES[div], seconds, megabytes/seconds);
}

int writefun(char *path, size_t blocksize) {
    int writefd, err, datasent;
    struct timespec start, elapsed;
    double time; //in seconds
    blocksize /= writers;

    //PTH(err, pthread_mutex_lock(&writersmtx), return -1)
    /*writers_ready++;
    if (writers_ready == writers) {
        PTH(err, pthread_cond_broadcast(&writerwait), return -1)
        writers_done = 0;
    }

    while(writers_ready != writers || !can_send) {
        PTH(err, pthread_cond_wait(&writerwait, &writersmtx), return -1)
    }*/
    /*
    PTH(err, pthread_mutex_lock(&writersmtx), return -1)
    while(can_send == 0) {
        PTH(err, pthread_cond_wait(&writerwait, &writersmtx), return -1)
    }
    can_send--;
    PTH(err, pthread_mutex_unlock(&writersmtx), return -1)*/

    // Open
    writefd = open(path, O_WRONLY);
    if (writefd == -1) return -1;

    // Get start time
    err = clock_gettime(CLOCK_MONOTONIC, &start);
    if (err == -1) return -1;

    datasent = send_data(writefd, blocksize);
    if (datasent == -1) return -1;

    // Get elapsed time
    elapsed = elapsed_time(&start);
    time = TIMESPEC_TO_DOUBLE(elapsed);

    // Wait other writers
    /*PTH(err, pthread_mutex_lock(&writersmtx), return -1)
    writers_done++;
    if (writers_done == writers) {
        can_send = 0;
        writers_ready = 0;
        PTH(err, pthread_cond_broadcast(&writerwait), return -1)
    }

    while(writers_done != writers) {
        PTH(err, pthread_cond_wait(&writerwait, &writersmtx), return -1)
    }*/

    // Wait other readers
    PTH(err, pthread_mutex_lock(&readersmtx), return -1)
    while(readers_done != readers) {
        PTH(err, pthread_cond_wait(&readerwait, &readersmtx), return -1)
    }
    PTH(err, pthread_mutex_unlock(&readersmtx), return -1)

    // Log benchmark
    PTH(err, pthread_mutex_lock(&writersmtx), return -1)
    log_bench(0, datasent, time);
    PTH(err, pthread_mutex_unlock(&writersmtx), return -1)

    close(writefd);
    return err;
}

int readfun(char *path, size_t blocksize) {
    int readfd, err, dataread;
    struct timespec start, elapsed;
    double time; //in seconds
    blocksize /= readers;

    // Wait other readers to be ready
    /*PTH(err, pthread_mutex_lock(&readersmtx), return -1)
    readers_ready++;
    if (readers_ready == readers) {
        PTH(err, pthread_cond_broadcast(&readerwait), return -1)
        readers_done = 0;
        can_read = readers;

        // Wake up all the writers
        PTH(err, pthread_mutex_lock(&writersmtx), return -1)
        can_send = writers;
        PTH(err, pthread_cond_broadcast(&writerwait), return -1)
        PTH(err, pthread_mutex_unlock(&writersmtx), return -1)
    }

    while(can_read == 0) {
        PTH(err, pthread_cond_wait(&readerwait, &readersmtx), return -1)
    }
    can_read--;
    readers_ready--;

    PTH(err, pthread_mutex_unlock(&readersmtx), return -1)*/

    // open path
    readfd = open(path, O_RDONLY);
    if (readfd == -1) return -1;

    // Get start time
    err = clock_gettime(CLOCK_MONOTONIC, &start);
    if (err == -1) return -1;

    dataread = recv_data(readfd, blocksize);
    if (dataread == -1) return -1;

    // Get elapsed time
    elapsed = elapsed_time(&start);
    time = TIMESPEC_TO_DOUBLE(elapsed);

    // Wait other readers
    PTH(err, pthread_mutex_lock(&readersmtx), return -1)
    // Log benchmark
    log_bench(1, dataread, time);

    readers_done++;
    if (readers_done == readers) {
        PTH(err, pthread_cond_broadcast(&readerwait), return -1)
        readers_ready = 0;
    }

    while(readers_done != readers) {
        PTH(err, pthread_cond_wait(&readerwait, &readersmtx), return -1)
    }

    PTH(err, pthread_mutex_unlock(&readersmtx), return -1)

    // close
    close(readfd);

    return err;
}

void *worker(void *argument) {
    int err = 0;
    arg_t *arg = (arg_t*) argument;
    size_t blocksize = startbs;

    //while(err == 0 && blocksize <= maxbs) {
        err = arg->fun(arg->path, blocksize);
        //blocksize = blocksize * 2;
        //sleep(1);
    //}

    if (err != 0) arg->error = errno;

    return 0;
}

static void usage(char *progname) {
    fprintf(stderr, "usage: %s <write_file> <read_file> <max_block_size> <readers> <writers>\n", progname);
}

int main(int argc, char** argv) {
    int err;
    size_t i;
    if (argc < 4) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }
    MINUS1(err = str_to_int(argv[3]), usage(argv[0]); return EXIT_FAILURE);
    maxbs = err;
    startbs = maxbs;

    if (argc >= 5) {
        MINUS1(err = str_to_int(argv[4]), usage(argv[0]); return EXIT_FAILURE);
        readers = err;
    }

    if (argc >= 6) {
        MINUS1(err = str_to_int(argv[5]), usage(argv[0]); return EXIT_FAILURE);
        writers = err;
    }
    can_send = writers;

    arg_t **argwriters = (arg_t**) malloc(sizeof(arg_t*) * writers);
    if (argwriters == NULL) return -1;

    for (i=0; i<writers; i++) {
        argwriters[i] = (arg_t *) malloc(sizeof(arg_t));
        EQNULLERR(argwriters[i], return -1)
        argwriters[i]->error = 0;
        argwriters[i]->fun = writefun;
        argwriters[i]->path = argv[1];

        err = pthread_create(&(argwriters[i]->tid), NULL, worker, argwriters[i]);
        if (err != 0) {
            errno = err;
            perror("cannot create writer thread");
            free(argwriters);
            exit(1);
        }
    }
    //printf("writers are running\n");

    arg_t **argreaders = (arg_t**) malloc(sizeof(arg_t*) * readers);
    if (argreaders == NULL) return -1;

    for (i=0; i<readers; i++) {
        argreaders[i] = (arg_t *) malloc(sizeof(arg_t));
        EQNULLERR(argreaders[i], return -1)
        argreaders[i]->error = 0;
        argreaders[i]->fun = readfun;
        argreaders[i]->path = argv[2];

        err = pthread_create(&(argreaders[i]->tid), NULL, worker, argreaders[i]);
        if (err != 0) {
            errno = err;
            perror("cannot create reader thread");
            exit(1);
        }
    }
    //printf("readers are running\n");

    for (i=0; i<writers; i++) {
        err = pthread_join(argwriters[i]->tid, NULL);
        if (err != 0) {
            errno = err;
            perror("cannot join writer thread");
            exit(1);
        }
        if (argwriters[i]->error != 0) {
            printf("writer %ld: %s\n", i, strerror(argwriters[i]->error));
        }
        free(argwriters[i]);
    }
    //printf("writers ended\n");

    for (i=0; i<readers; i++) {
        err = pthread_join(argreaders[i]->tid, NULL);
        if (err != 0) {
            errno = err;
            perror("cannot join reader thread");
            exit(1);
        }
        if (argreaders[i]->error != 0) {
            printf("reader %ld: %s\n", i, strerror(argreaders[i]->error));
        }
        free(argreaders[i]);
    }
    //printf("readers ended\n");

    free(argwriters);
    free(argreaders);
    return 0;
}