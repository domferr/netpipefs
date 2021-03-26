/*
 *
 * Run the following command to build this example
 * gcc -Wall examples/ddsel.c src/scfiles.c -o bin/ddsel -lpthread -lm
 *
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

static size_t num_netpipes = DEFAULT_NUM_NETPIPES;
static size_t blocksize = DEFAULT_BLOCKSIZE;
static size_t count = DEFAULT_COUNT;

static int open_k_files(char *base, int *destfd, int k, int oflag, ...) {
    va_list va_args;
    va_start(va_args, oflag);
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
    printf("    -h          print usage\n"
                  "    np=<d>      number of netpipes (default: %d)\n"
                  "    bs=<d>      block size (default: %d)\n"
                  "    count=<d>   number of times to send data (default: %d)\n",
                  DEFAULT_NUM_NETPIPES, DEFAULT_BLOCKSIZE, DEFAULT_COUNT);
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
    // example: ddsel nf=100 bs=8192 count=50
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

        // check for bs=<d>
        found = parse_size_flag(argv[i], "bs=", &size_val);
        if (found) {
            if (size_val == 0) return EXIT_FAILURE;
            blocksize = size_val;
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
    int i, err, maxfd, fd[num_netpipes], read[num_netpipes];
    fd_set rset;
    char *buf = (char *) malloc(sizeof(char) * blocksize);
    if (buf == NULL) {
        perror("consumer malloc");
        exit(EXIT_FAILURE);
    }

    // Open all the netpipes
    if (open_k_files(DEFAULT_CONS_NETPIPE, fd, num_netpipes, O_RDONLY) == -1) {
        perror("consumer can't open");
        free(buf);
        exit(EXIT_FAILURE);
    }

    for(i=0; i < num_netpipes; i++) {
        read[i] = 0;
    }
    err = 1;
    while (err > 0) {
        FD_ZERO(&rset);
        maxfd = -1;
        for(i=0; i < num_netpipes; i++) {
            if (read[i] < count) {
                FD_SET(fd[i], &rset);
                if (fd[i] > maxfd)
                    maxfd = fd[i];
            }
        }
        if (maxfd == -1) {
            err = 1;
            break;
        }

        err = select(maxfd+1, &rset, NULL, NULL, NULL);
        if (err == 0) {
            errno = ETIMEDOUT;
        } else if (err > 0) {
            // Read from readable netpipes
            for(i=0; i < num_netpipes; i++) {
                if (FD_ISSET(fd[i], &rset)) {
                    err = readn(fd[i], buf, blocksize);
                    if (err > 0) read[i]++;
                }
            }
        }
    }

    // Close all the netpipes
    for(i=0; i < num_netpipes; i++) {
        if (close(fd[i]) == -1)
            fprintf(stderr, "consumer failed to close netpipe %d: %s\n", fd[i], strerror(errno));
    }

    if (err == -1) { // Print consumer error
        perror("consumer failure");
    } else {
        printf("Read  [");
        for (i = 0; i < num_netpipes; i++) {
            printsize(read[i] * sizeof(char) * blocksize);
            if (i < num_netpipes - 1) printf(" ");
        }
        printf("] bytes\n");
    }

    free(buf);
    return 0;
}

int main(int argc, char** argv) {
    int err, i, maxfd, fd[num_netpipes], sent[num_netpipes];
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
    printf("Writing and reading %ld bytes %ld times into %ld netpipes\n", (blocksize), count, num_netpipes);

    char *buf = (char*) malloc(sizeof(char) * blocksize);
    if (buf == NULL) {
        perror("producer malloc");
        return EXIT_FAILURE;
    }
    memset(buf, 0, blocksize);

    // Create thread for receiving data (consumer)
    PTHERR(err, pthread_create(&pcons, NULL, &consumer, NULL), free(buf); return EXIT_FAILURE)

    // Open all the netpipes
    if (open_k_files(DEFAULT_PROD_NETPIPE, fd, num_netpipes, O_WRONLY) == -1) {
        perror("producer can't open");
        pthread_join(pcons, NULL);
        free(buf);
        exit(EXIT_FAILURE);
    }

    for(i=0; i < num_netpipes; i++) {
        sent[i] = 0;
    }

    err = 1;
    while (err > 0) {
        FD_ZERO(&wset);
        maxfd = -1;
        for(i=0; i < num_netpipes; i++) {
            if (sent[i] < count) { // write max count times
                FD_SET(fd[i], &wset);
                if (fd[i] > maxfd)
                    maxfd = fd[i];
            }
        }
        if (maxfd == -1) {
            break;
        }

        err = select(maxfd + 1, NULL, &wset, NULL, NULL);
        if (err == 0) {
            errno = ETIMEDOUT;
        } else if (err > 0) {
            // Write to writable netpipes
            for(i=0; i < num_netpipes; i++) {
                if (FD_ISSET(fd[i], &wset)) {
                    err = writen(fd[i], buf, blocksize);
                    if (err > 0) sent[i]++;
                }
            }
        }
    }

    // Close all the netpipes
    for(i=0; i < num_netpipes; i++) {
        if (close(fd[i]) == -1)
            fprintf(stderr, "producer failed to close netpipe %d: %s\n", fd[i], strerror(errno));
    }

    // Join consumer thread
    PTHERR(err, pthread_join(pcons, NULL), free(buf); return -1)

    // Print producer recap
    if (err == -1) { // Print consumer error
        perror("producer failure");
    } else {
        printf("Wrote [");
        for(i=0; i < num_netpipes; i++) {
            printsize(sent[i] * sizeof(char) * blocksize);
            if (i < num_netpipes - 1) printf(" ");
        }
        printf("] bytes\n");
    }

    free(buf);
    return 0;
}