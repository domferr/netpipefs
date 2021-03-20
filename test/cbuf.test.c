#include <unistd.h>
#include "testutilities.h"
#include "../include/cbuf.h"

int main(int argc, char** argv) {
    size_t capacity = 10;

    /* Alloc buffer */
    cbuf_t *buffer = cbuf_alloc(capacity);
    test(buffer != NULL)

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

    int pipefd[2];
    if (pipe(pipefd) == -1) return EXIT_FAILURE;

    /* Alloc buffer */
    buffer = cbuf_alloc(capacity);
    test(buffer != NULL)

    if (write(pipefd[1], dummydata, capacity - capacity/4) < (ssize_t)(capacity - capacity/4)) return EXIT_FAILURE;

    /* Put into buffer from pipe */
    test(cbuf_readn(pipefd[0], buffer, capacity - capacity/4) == (ssize_t)(capacity - capacity/4))
    test(cbuf_size(buffer) == capacity - capacity/4)
    test(cbuf_get(buffer, datagot, capacity/3) == capacity/3)

    if (write(pipefd[1], dummydata, capacity/3 + capacity/4) < (ssize_t)(capacity/3 + capacity/4)) return EXIT_FAILURE;
    test(cbuf_readn(pipefd[0], buffer, capacity/3 + capacity/4) == (ssize_t)(capacity/3 + capacity/4))
    test(cbuf_size(buffer) == capacity)

    testpassed("Circular buffer");

    return 0;
}