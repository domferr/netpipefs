#ifndef TESTUTILITIES_H
#define TESTUTILITIES_H

#include <stdlib.h>
#include <errno.h>
#include <string.h>

#define PASS "[ PASS ]"
#define FAIL "[ FAIL ]"
#define SPCE "        "

#define test(expr)       \
    if (!(expr)) {       \
        fprintf(stderr, "%s %s:%d: test '%s' failed\n", FAIL, __FILE__, __LINE__, #expr); \
        if (errno != 0) fprintf(stderr, "%s %s\n", SPCE, strerror(errno));                \
        exit(EXIT_FAILURE); \
    }

#define testpassed(test) fprintf(stdout, "%s %s\n", PASS, test)

#endif //TESTUTILITIES_H
