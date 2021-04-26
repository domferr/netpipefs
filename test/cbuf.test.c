#include <unistd.h>
#include <time.h>
#include "testutilities.h"
#include "../include/cbuf.h"

static void test_operations(void);
static void test_zero_capacity(void);
static void test_from_file_descriptor(void);

int main(int argc, char** argv) {
    size_t capacity = 8192;
    char *bufptr = (char*) malloc(sizeof(char)*capacity);
    /* Alloc buffer */
    cbuf_t *buffer = cbuf_alloc(capacity);
    struct timespec tw1, tw2;
    double elapsed;

    cbuf_put(buffer, bufptr, capacity);
    clock_gettime(CLOCK_MONOTONIC, &tw1);
    size_t read = cbuf_get_memcpy(buffer, bufptr, capacity);
    clock_gettime(CLOCK_MONOTONIC, &tw2);

    elapsed = 1000.0*tw2.tv_sec + 1e-6*tw2.tv_nsec - (1000.0*tw1.tv_sec + 1e-6*tw1.tv_nsec);
    printf("%ld elapsed %.6fs\n", read, elapsed);

    cbuf_free(buffer);
    free(bufptr);

    capacity = 16384;
    bufptr = (char*) malloc(sizeof(char)*capacity);

    /* Alloc buffer */
    buffer = cbuf_alloc(capacity);
    cbuf_put(buffer, bufptr, capacity);

    clock_gettime(CLOCK_MONOTONIC, &tw1);
    read = cbuf_get_memcpy(buffer, bufptr, capacity);
    clock_gettime(CLOCK_MONOTONIC, &tw2);

    elapsed = 1000.0*tw2.tv_sec + 1e-6*tw2.tv_nsec - (1000.0*tw1.tv_sec + 1e-6*tw1.tv_nsec);
    printf("%ld elapsed %.6fs\n", read, elapsed);

    cbuf_free(buffer);
    free(bufptr);

    test_operations();
    test_zero_capacity();
    test_from_file_descriptor();
    testpassed("Circular buffer");
    return 0;
}

static void test_operations(void) {
    size_t capacity = 10;

    /* Alloc buffer */
    cbuf_t *buffer = cbuf_alloc(capacity);
    test(buffer != NULL)
    test(cbuf_empty(buffer) == 1)
    test(cbuf_size(buffer) == 0)

    /* Put zero bytes */
    test(cbuf_put(buffer, NULL, 0) == 0)
    test(cbuf_empty(buffer) == 1)

    char dummydata[capacity];
    for(size_t i=0; i<capacity; i++) dummydata[i] = (char)(97+i);

    /* Put data filling the buffer */
    test(cbuf_put(buffer, dummydata, capacity) == capacity)
    test(cbuf_size(buffer) == capacity)
    test(cbuf_full(buffer) == 1)

    /* Put data into a full buffer */
    test(cbuf_put(buffer, dummydata, capacity) == 0)
    test(cbuf_size(buffer) == capacity)
    test(cbuf_full(buffer) == 1)

    /* Empty out the buffer */
    char datagot[capacity];
    test(cbuf_get(buffer, datagot, capacity) == capacity)
    test(cbuf_size(buffer) == 0)
    test(cbuf_empty(buffer) == 1)

    /* Compare data put with data got */
    for(size_t i = 0; i<capacity; i++) {
        test(datagot[i] == dummydata[i])
    }

    /* Free buffer */
    cbuf_free(buffer);
}

static void test_zero_capacity(void) {
    /* Alloc buffer */
    cbuf_t *buffer = cbuf_alloc(0);
    test(buffer != NULL)
    test(cbuf_empty(buffer) == 1)
    test(cbuf_size(buffer) == 0)
    test(cbuf_full(buffer) == 0)

    char dummydata[10];
    for(size_t i=0; i<10; i++) dummydata[i] = (char)(97+i);

    /* Put data into the buffer */
    test(cbuf_put(buffer, dummydata, 5) == 0)
    test(cbuf_size(buffer) == 0)

    /* Get data from the buffer */
    char datagot[10];
    test(cbuf_get(buffer, datagot, 10) == 0)
    test(cbuf_size(buffer) == 0)

    /* Free buffer */
    cbuf_free(buffer);
}

static void test_from_file_descriptor(void) {
    size_t capacity = 10, bytes;
    int pipefd[2];
    if (pipe(pipefd) == -1) return;

    /* Alloc buffer */
    cbuf_t *buffer = cbuf_alloc(capacity);
    test(buffer != NULL)

    char dummydata[capacity];
    for(size_t i=0; i<capacity; i++) dummydata[i] = (char)(97+i);

    /* Write into the pipe */
    bytes = capacity - capacity/4;
    if (write(pipefd[1], dummydata, bytes) < (ssize_t)(bytes)) {
        perror("can't test from file descriptor");
        return;
    }

    /* Put into buffer from pipe */
    test(cbuf_readn(pipefd[0], buffer, bytes) == (ssize_t)(bytes))
    test(cbuf_size(buffer) == bytes)

    /* Writen from buffer to the pipe */
    bytes = capacity/3;
    test(cbuf_writen(pipefd[1], buffer, bytes) == (ssize_t)(bytes))

    /* Read from pipe */
    char datagot[bytes];
    if (read(pipefd[0], datagot, bytes) < (ssize_t)(bytes)) {
        perror("can't test from file descriptor");
        return;
    }

    /* Compare data put with data got */
    for(size_t i = 0; i<bytes; i++) {
        test(datagot[i] == dummydata[i])
    }

    close(pipefd[0]);
    close(pipefd[1]);

    /* Free buffer */
    cbuf_free(buffer);
}