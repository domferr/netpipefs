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

    /* Put data into a full buffer */
    test(cbuf_put(buffer, dummydata, capacity) == 0)
    test(cbuf_size(buffer) == capacity)

    /* Empty out the buffer */
    char datagot[capacity];
    test(cbuf_get(buffer, datagot, capacity) == capacity)
    test(cbuf_size(buffer) == 0)

    /* Compare data put with data got */
    for(size_t i = 0; i<capacity; i++) {
        test(datagot[i] == dummydata[i])
    }

    /* Free buffer */
    cbuf_free(buffer);

    testpassed("Circular buffer");

    return 0;
}