#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include "../include/utils.h"
#include "testutilities.h"

static void test_ipv4_address_to_array(void);
static void test_macros(void);
static void test_msleep(void);
static void test_elapsedtime(void);

int main(int argc, char** argv) {

    test_macros();
    test_ipv4_address_to_array();
    test_msleep();
    test_elapsedtime();

    testpassed("Utilities");

    return 0;
}

static void test_macros(void) {
    int negative_one = -1, zero = 0, positive = 10, res;
    void *nullptr = NULL;

    res = 0;
    MINUS1(negative_one, res = 1)
    test(res == 1)

    res = 0;
    MINUS1(zero, res = 1)
    test(res == 0)

    res = 0;
    MINUS1(positive, res = 1)
    test(res == 0)

    res = 0;
    EQNULL(nullptr, res = 1)
    test(res == 1)

    res = 0;
    EQNULL(&res, res = 1)
    test(res == 0)

    res = 0;
    ISNEGATIVE(negative_one, res = 1)
    test(res == 1)

    res = 0;
    ISNEGATIVE(zero, res = 1)
    test(res == 0)

    res = 0;
    ISNEGATIVE(positive, res = 1)
    test(res == 0)

    res = 0;
    NOTZERO(positive, res = 1)
    test(res == 1)

    res = 0;
    NOTZERO(negative_one, res = 1)
    test(res == 1)

    res = 0;
    NOTZERO(zero, res = 1)
    test(res == 0)

    res = 0;
    PTH(res, positive, res = 1)
    test(res == 1)

    res = 0;
    PTH(res, negative_one, res = 1)
    test(res == 1)

    res = 0;
    PTH(res, zero, res = 1)
    test(res == 0)
}

static void test_ipv4_address_to_array(void) {
    int array[4];

    /* Simple case 192.168.1.8 */
    test(ipv4_address_to_array("192.168.1.8", array) == 0)
    test(array[0] == 192)
    test(array[1] == 168)
    test(array[2] == 1)
    test(array[3] == 8)

    /* 0.0.0.0 case */
    test(ipv4_address_to_array("0.0.0.0", array) == 0)
    test(array[0] == 0)
    test(array[1] == 0)
    test(array[2] == 0)
    test(array[3] == 0)

    /* 255.255.255.255 */
    test(ipv4_address_to_array("255.255.255.255", array) == 0)
    test(array[0] == 255)
    test(array[1] == 255)
    test(array[2] == 255)
    test(array[3] == 255)

    /* First number is negative */
    test(ipv4_address_to_array("-192.168.1.8", array) == -1)

    /* Second number is negative */
    test(ipv4_address_to_array("192.-168.1.8", array) == -1)

    /* Third number is negative */
    test(ipv4_address_to_array("192.168.-1.8", array) == -1)

    /* Last number is negative */
    test(ipv4_address_to_array("192.168.1.-8", array) == -1)

    /* Not an ip address */
    test(ipv4_address_to_array("first.second.third.last", array) == -1)

    /* Three values */
    test(ipv4_address_to_array("192.168.1", array) == -1)

    /* Two values */
    test(ipv4_address_to_array("192.168", array) == -1)

    /* One value */
    test(ipv4_address_to_array("192", array) == -1)

    /* Empty string */
    test(ipv4_address_to_array("", array) == -1)

    /* Too many dots */
    test(ipv4_address_to_array("192.168.1.5.", array) == -1)

    /* Too many numbers */
    test(ipv4_address_to_array("192.168.1.6.7", array) == -1)

    /* First number is more than 255 */
    test(ipv4_address_to_array("260.168.1.10", array) == -1)

    /* Second number is more than 255 */
    test(ipv4_address_to_array("25.2650.1.10", array) == -1)

    /* Third number is more than 255 */
    test(ipv4_address_to_array("2.168.600.6", array) == -1)

    /* Last number is more than 255 */
    test(ipv4_address_to_array("55.168.1.478", array) == -1)
}

static void test_msleep(void) {
    long waiting_ms = 100;
    double elapsed;
    struct timespec tw1, tw2;

    clock_gettime(CLOCK_MONOTONIC, &tw1);
    test(msleep(waiting_ms) == 0)
    clock_gettime(CLOCK_MONOTONIC, &tw2);
    elapsed = 1000.0*tw2.tv_sec + 1e-6*tw2.tv_nsec - (1000.0*tw1.tv_sec + 1e-6*tw1.tv_nsec);
    test(elapsed >= waiting_ms)

    /* Sleep for a negative amount of milliseconds. It shouldn't sleep at all */
    clock_gettime(CLOCK_MONOTONIC, &tw1);
    test(msleep(-1) == 0)
    clock_gettime(CLOCK_MONOTONIC, &tw2);
    elapsed = 1000.0*tw2.tv_sec + 1e-6*tw2.tv_nsec - (1000.0*tw1.tv_sec + 1e-6*tw1.tv_nsec);
    test(elapsed >= 0)
}

static void test_elapsedtime(void) {
    long waiting_ms = 100;
    struct timespec tw1, elapsed;

    clock_gettime(CLOCK_MONOTONIC, &tw1);
    test(msleep(waiting_ms) == 0)
    elapsed = elapsed_time(&tw1);
    test(elapsed.tv_sec >= 0 && elapsed.tv_nsec >= 0)
    test((elapsed.tv_sec * 1000L + elapsed.tv_nsec / 1000000L) >= 100)
    test((elapsed.tv_sec * 1000L + elapsed.tv_nsec / 1000000L) <= 200)
}