#include <stdio.h>
#include "../include/utils.h"
#include "testutilities.h"

static void test_ipv4_address_to_array(void);

int main(int argc, char** argv) {

    test_ipv4_address_to_array();
    testpassed("Convert ipv4 address into integer array");

    return 0;
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