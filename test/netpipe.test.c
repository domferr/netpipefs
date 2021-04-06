#include <unistd.h>
#include "testutilities.h"
#include "../include/netpipe.h"

static void test_zero_capacity(void);

int main(int argc, char** argv) {
    test_zero_capacity();
    testpassed("Netpipe");
    return 0;
}

static void test_zero_capacity(void) {
    size_t capacity = 10;

    /* Alloc buffer */
    cbuf_t *buffer = cbuf_alloc(capacity);
    test(buffer != NULL)

    /* Free buffer */
    cbuf_free(buffer);
}