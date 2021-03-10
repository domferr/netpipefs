#ifndef TESTUTILITIES_H
#define TESTUTILITIES_H

#include <stdlib.h>

#define test(expr)       \
    if (!(expr)) {      \
        fprintf(stderr, "[ FAIL ] %s:%d: test '%s' failed\n", __FILE__, __LINE__, #expr);   \
        exit(EXIT_FAILURE); \
    }

#define testpassed(test) fprintf(stdout, "[ PASS ] %s\n", test)

#endif //TESTUTILITIES_H
