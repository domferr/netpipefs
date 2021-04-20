/*
 * This program will run like dd on multiple files in parallel by using
 * the select() system call.
 *
 * Run the following command to build this example
 * gcc -Wall examples/ddsel.c src/scfiles.c -o bin/ddsel -lpthread -lm
 *
 * For example, with the following parameters ddsel will read and
 * write 8192 bytes 50 times into 100 netpipes:
 *     ddsel np=100 bs=8192 count=50
 */

#include <math.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include "../include/utils.h"
#include "../include/scfiles.h"

#define DEFAULT_PROD_NETPIPE "./tmp/prod/netpipe"
#define DEFAULT_CONS_NETPIPE "./tmp/cons/netpipe"
#define DEFAULT_NUM_NETPIPES 10
#define DEFAULT_BLOCKSIZE 512
#define DEFAULT_COUNT 100

/* Converts a timespec to a fractional number of seconds.
 * From: https://github.com/solemnwarning/timespec/blob/master/timespec.c
 */
#define TIMESPEC_TO_DOUBLE(ts) ((double)((ts).tv_sec) + ((double)((ts).tv_nsec) / 1000000000))

static size_t num_netpipes = DEFAULT_NUM_NETPIPES;
static size_t out_blocksize = DEFAULT_BLOCKSIZE;
static size_t in_blocksize = DEFAULT_BLOCKSIZE;
static size_t count = DEFAULT_COUNT;

static int open_k_files(char *base, int *destfd, int k, int oflag, struct timespec *start, ...) {
    va_list va_args;
    va_start(va_args, start);
    size_t max_path_len = (size_t) ceil(log10(num_netpipes)) + 1 + strlen(base);

    // alloc path
    char *path = (char *) malloc(sizeof(char) * max_path_len);
    if (path == NULL) return -1;

    // copy the base path into the path
    strcpy(path, base);

    // get the point from where to concat
    char *path_concat_point = path + strlen(path);

    for (int i = 0; i < k; ++i) {
        sprintf(path_concat_point, "%d", i);
        /* Get current time */
        if (clock_gettime(CLOCK_MONOTONIC, &start[i]) == -1) {
            free(path);
            return -1;
        }
        /* Open the file */
        destfd[i] = open(path, oflag, va_args);
        if (destfd[i] == -1) {
            free(path);
            return -1;
        }
    }

    va_end(va_args);
    free(path);
    return 0;
}

static void printsize(size_t size) {
    static const char *SIZES[] = { "B", "KB", "MB", "GB" };
    size_t div = 0;
    size_t rem = 0;

    while (size >= 1024 && div < (sizeof SIZES / sizeof *SIZES)) {
        rem = (size % 1024);
        div++;
        size /= 1024;
    }

    printf("%.1f %s", (float)size + (float)rem / 1024.0, SIZES[div]);
}

static void usage(char *progname) {
    printf("usage: %s [OPTIONS]\n", progname);
    printf("    -h           print usage\n"
                  "    np=N         write to N netpipes (default: %d)\n"
                  "    ibs=BYTES    read up to BYTES bytes at a time (default: %d)\n"
                  "    obs=BYTES    write BYTES bytes at a time (default: %d)\n"
                  "    bs=BYTES     read and write up to BYTES bytes at a time (default: %d)\n"
                  "                 overrides ibs and obs\n"
                  "    count=N      copy only N input blocks (default: %d)\n",
                  DEFAULT_NUM_NETPIPES, DEFAULT_BLOCKSIZE, DEFAULT_BLOCKSIZE, DEFAULT_BLOCKSIZE, DEFAULT_COUNT);
}

static int parse_size_flag(char *given_opt, const char *format, size_t *result) {
    int i = 0;
    char *endptr;

    while(given_opt[i] != '\0' && format[i] != '\0' && given_opt[i] == format[i]) {
        i++;
    }

    long val = strtol(given_opt+i, &endptr, 10);
    if (endptr == (given_opt+i) || val < 0)
        return 0;

    *result = val;
    return 1;
}

static int parse_options(int argc, char **argv) {
    // example: ddsel np=100 bs=8192 count=50
    int i;
    int found;
    size_t size_val;

    for(i = 1; i < argc; i++) {
        // check for -h
        if (argv[i][0] == '-' && argv[i][1] == 'h') {
            return EXIT_FAILURE;
        }

        // check for nf=<d>
        found = parse_size_flag(argv[i], "np=", &size_val);
        if (found) {
            if (size_val == 0) return EXIT_FAILURE;
            num_netpipes = size_val;
            continue;
        }

        // check for obs=<d>
        found = parse_size_flag(argv[i], "obs=", &size_val);
        if (found) {
            if (size_val == 0) return EXIT_FAILURE;
            out_blocksize = size_val;
            continue;
        }

        // check for ibs=<d>
        found = parse_size_flag(argv[i], "ibs=", &size_val);
        if (found) {
            if (size_val == 0) return EXIT_FAILURE;
            in_blocksize = size_val;
            continue;
        }

        // check for bs=<d>
        found = parse_size_flag(argv[i], "bs=", &size_val);
        if (found) {
            if (size_val == 0) return EXIT_FAILURE;
            out_blocksize = size_val;
            in_blocksize = size_val;
            continue;
        }

        // check for count=<d>
        found = parse_size_flag(argv[i], "count=", &size_val);
        if (found) {
            if (size_val == 0) return EXIT_FAILURE;
            count = size_val;
            continue;
        }

        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

static void *consumer(void *arg) {
    int i, err, failure = 0, maxfd, *fd = NULL, bytes_read = 0;
    struct timespec *start = NULL, elapsed;
    double *time = NULL;
    fd_set rset;
    char *buf = NULL;

    EQNULLERR(buf = (char*) malloc(sizeof(char) * in_blocksize), failure = 1; goto cleanup)
    EQNULLERR(fd = (int *) malloc(sizeof(int) * num_netpipes), failure = 1; goto cleanup)
    EQNULLERR(time = (double *) malloc(sizeof(double) * num_netpipes), failure = 1; goto cleanup)
    EQNULLERR(start = (struct timespec *) malloc(sizeof(struct timespec) * num_netpipes), failure = 1; goto cleanup)

    // Open all the netpipes
    if (open_k_files(DEFAULT_CONS_NETPIPE, fd, num_netpipes, O_RDONLY, start) == -1) {
        failure = 1;
        goto cleanup;
    }

    err = 1;
    while (err > 0) {
        FD_ZERO(&rset);
        maxfd = -1;
        for(i=0; i < num_netpipes; i++) {
            if (fd[i] == -1) continue;
            FD_SET(fd[i], &rset);
            if (fd[i] > maxfd)
                maxfd = fd[i];
        }
        if (maxfd == -1) {
            err = 1;
            break;
        }

        err = select(maxfd+1, &rset, NULL, NULL, NULL);
        if (err == 0) {
            errno = ETIMEDOUT;
        } else if (err > 0) {
            printf("cons wokeup");
            // Read from readable netpipes
            for(i=0; i < num_netpipes; i++) {
                if (FD_ISSET(fd[i], &rset)) {
                    err = readn(fd[i], buf, in_blocksize);
                    if (i == 0 && err > 0) bytes_read += err;
                    printf(" %d, %d", fd[i], err);
                    if (err == 0) {
                        close(fd[i]);
                        fd[i] = -1;
                        if (clock_gettime(CLOCK_MONOTONIC, &elapsed) != -1) {
                            elapsed.tv_sec = elapsed.tv_sec - start[i].tv_sec;
                            elapsed.tv_nsec = elapsed.tv_nsec - start[i].tv_nsec;
                            if (elapsed.tv_nsec < 0) {
                                elapsed.tv_sec -= 1;
                                elapsed.tv_nsec += 1000000000L; //1e9
                            }
                            time[i] = TIMESPEC_TO_DOUBLE(elapsed);
                            err = 1;
                        } else {
                            time[i] = -1;
                            err = -1;
                        }
                    }
                }
            }
            printf("\n");
        }
    }

    // Close all the netpipes
    for(i=0; i < num_netpipes; i++) {
        if (fd[i] == -1) continue;
        if (close(fd[i]) == -1)
            fprintf(stderr, "consumer failed to close netpipe %d: %s\n", fd[i], strerror(errno));
    }

    if (err == -1) { // Print consumer error
        perror("consumer error");
    } else {
        printf("Read: ");
        printsize(bytes_read * sizeof(char));
        printf(" bytes\n      [");
        for (i = 0; i < num_netpipes; i++) {
            printf("%.2fs", time[i]);
            if (i < num_netpipes - 1) {
                if (i % 5 == 4) printf("]\n      [");
                else printf(", ");
            }
        }
        printf("]\n");
    }

    cleanup:
    if (buf) free(buf);
    if (fd) free(fd);
    if (time) free(time);
    if (start) free(start);

    if (failure)
        exit(EXIT_FAILURE);

    return 0;
}

int main(int argc, char** argv) {
    int err, i, maxfd, *fd = NULL, *sent = NULL, force_exit = 0;
    char *buf = NULL;
    struct timespec *start = NULL, elapsed;
    double *time = NULL;
    pthread_t pcons;
    fd_set wset;

    // Parse options
    err = parse_options(argc, argv);
    if (err == -1) { // Error occurred while parsing
        perror("failed to parse options");
        return EXIT_FAILURE;
    } else if (err == EXIT_FAILURE) { // Some option is not valid then print usage or was given -h
        usage(argv[0]);
        return 0;
    }
    printf("Writing blocks of %ld bytes %ld times into %ld netpipes\n", (out_blocksize), count, num_netpipes);
    printf("Reading blocks of %ld bytes from %ld netpipes\n", (in_blocksize), num_netpipes);

    EQNULLERR(buf = (char*) malloc(sizeof(char) * out_blocksize), goto cleanup)
    memset(buf, 0, out_blocksize);
    EQNULLERR(fd = (int *) malloc(sizeof(int) * num_netpipes), goto cleanup)
    EQNULLERR(sent = (int *) malloc(sizeof(int) * num_netpipes), goto cleanup)
    EQNULLERR(start = (struct timespec *) malloc(sizeof(struct timespec) * num_netpipes), goto cleanup)
    EQNULLERR(time = (double *) malloc(sizeof(double) * num_netpipes), goto cleanup)

    // Create thread for receiving data (consumer)
    PTHERR(err, pthread_create(&pcons, NULL, &consumer, NULL), goto cleanup)

    // Open all the netpipes
    if (open_k_files(DEFAULT_PROD_NETPIPE, fd, num_netpipes, O_WRONLY, start) == -1) {
        perror("producer can't open");
        force_exit = 1;
        goto cleanup;
    }

    for(i=0; i < num_netpipes; i++) {
        sent[i] = 0;
        start[i] = start[0];
    }

    err = 1;
    while (err > 0) {
        FD_ZERO(&wset);
        maxfd = -1;
        for(i=0; i < num_netpipes; i++) {
            if (fd[i] == -1) continue;
            if (sent[i] < count) { // write max count times
                FD_SET(fd[i], &wset);
                if (fd[i] > maxfd)
                    maxfd = fd[i];
            } else {
                close(fd[i]);
                fd[i] = -1;
                if (clock_gettime(CLOCK_MONOTONIC, &elapsed) != -1) {
                    elapsed.tv_sec = elapsed.tv_sec - start[i].tv_sec;
                    elapsed.tv_nsec = elapsed.tv_nsec - start[i].tv_nsec;
                    if (elapsed.tv_nsec < 0) {
                        elapsed.tv_sec -= 1;
                        elapsed.tv_nsec += 1000000000L; //1e9
                    }
                    time[i] = TIMESPEC_TO_DOUBLE(elapsed);
                } else {
                    time[i] = -1;
                }
            }
        }
        if (maxfd == -1) {
            break;
        }

        err = select(maxfd + 1, NULL, &wset, NULL, NULL);
        if (err == 0) {
            errno = ETIMEDOUT;
        } else if (err > 0) {
            //printf("prod wokeup");
            // Write to writable netpipes
            for(i=0; i < num_netpipes; i++) {
                if (FD_ISSET(fd[i], &wset)) {
                    err = writen(fd[i], buf, out_blocksize);
                    if (err > 0) sent[i]++;
                    //printf(" %d, %d", fd[i], sent[i]);
                }
            }
            //printf("\n");
        }
    }

    // Close all the netpipes
    for(i=0; i < num_netpipes; i++) {
        if (fd[i] == -1) continue;
        if (close(fd[i]) == -1)
            fprintf(stderr, "producer failed to close netpipe %d: %s\n", fd[i], strerror(errno));
    }

    // Join consumer thread
    PTHERR(err, pthread_join(pcons, NULL), force_exit = 1; goto cleanup)

    // Print producer recap
    if (err == -1) { // Print consumer error
        perror("producer error");
    } else {
        printf("Wrote:");
        printsize(count * sizeof(char) * out_blocksize);
        printf(" bytes\n      [");
        for (i = 0; i < num_netpipes; i++) {
            printf("%.2fs", time[i]);
            if (i < num_netpipes - 1) {
                if (i % 5 == 4) printf("]\n      [");
                else printf(", ");
            }
        }
        printf("]\n");
    }

    cleanup:
    if (buf) free(buf);
    if (fd) free(fd);
    if (sent) free(sent);
    if (time) free(time);
    if (start) free(start);
    if (force_exit)
        exit(EXIT_FAILURE);

    return 0;
}