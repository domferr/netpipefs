#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include "../include/utils.h"

int msleep(int milliseconds) {
    struct timespec req = {MS_TO_SEC(milliseconds), MS_TO_NANOSEC(milliseconds)};
    struct timespec rem = {0, 0};
    int res;
    do {
        rem.tv_sec = 0;
        rem.tv_nsec = 0;
        res = nanosleep(&req, &rem);
        req = rem;
    } while (res == EINTR);

    return res;
}

int ipv4_address_to_array(const char *ipstr, int *res) {
    char *endptr;

    for(int i=0; i<4; i++) {
        res[i] = (int) strtol(ipstr, &endptr, 10);
        if (endptr == ipstr) return -1;   // there were no numbers
        if (i != 3 && *endptr == '\0') return -1;   // missing numbers
        if (res[i] < 0 || res[i] > 255) return -1;  // invalid number
        ipstr = endptr + 1; // avoid the dot
    }
    if (*endptr != '\0')
        return -1; // there is something after the ip address
    return 0;
}