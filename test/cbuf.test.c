#include <unistd.h>
#include "testutilities.h"
#include "../include/cbuf.h"

static void test_operations(void);
static void test_zero_capacity(void);
static void test_from_file_descriptor(void);

int main(int argc, char** argv) {
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
    size_t capacity = 10;

    /* Alloc buffer */
    cbuf_t *buffer = cbuf_alloc(capacity);
    test(buffer != NULL)

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