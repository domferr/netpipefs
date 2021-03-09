#include <assert.h>
#include <stdio.h>
#include "../include/utils.h"

int main(int argc, char** argv) {
    int array[4];

    /* Simple case 192.168.1.8 */
    assert(ipv4_address_to_array("192.168.1.8", array) == 0);
    assert(array[0] == 192);
    assert(array[1] == 168);
    assert(array[2] == 1);
    assert(array[3] == 8);

    /* 0.0.0.0 case */
    assert(ipv4_address_to_array("0.0.0.0", array) == 0);
    assert(array[0] == 0);
    assert(array[1] == 0);
    assert(array[2] == 0);
    assert(array[3] == 0);

    /* 255.255.255.255 */
    assert(ipv4_address_to_array("255.255.255.255", array) == 0);
    assert(array[0] == 255);
    assert(array[1] == 255);
    assert(array[2] == 255);
    assert(array[3] == 255);

    /* First number is negative */
    assert(ipv4_address_to_array("-192.168.1.8", array) == -1);

    /* Second number is negative */
    assert(ipv4_address_to_array("192.-168.1.8", array) == -1);

    /* Third number is negative */
    assert(ipv4_address_to_array("192.168.-1.8", array) == -1);

    /* Last number is negative */
    assert(ipv4_address_to_array("192.168.1.-8", array) == -1);

    /* Not an ip address */
    assert(ipv4_address_to_array("first.second.third.last", array) == -1);

    /* Three values */
    assert(ipv4_address_to_array("192.168.1", array) == -1);

    /* Two values */
    assert(ipv4_address_to_array("192.168", array) == -1);

    /* One value */
    assert(ipv4_address_to_array("192", array) == -1);

    /* Empty string */
    assert(ipv4_address_to_array("", array) == -1);

    /* Too many dots */
    assert(ipv4_address_to_array("192.168.1.5.", array) == -1);

    /* Too many numbers */
    assert(ipv4_address_to_array("192.168.1.6.7", array) == -1);

    /* First number is more than 255 */
    assert(ipv4_address_to_array("260.168.1.10", array) == -1);

    /* Second number is more than 255 */
    assert(ipv4_address_to_array("25.2650.1.10", array) == -1);

    /* Third number is more than 255 */
    assert(ipv4_address_to_array("2.168.600.6", array) == -1);

    /* Last number is more than 255 */
    assert(ipv4_address_to_array("55.168.1.478", array) == -1);

    fprintf(stdout, "[ PASS ] ipv4_address_to_array()\n");
    return 0;
}