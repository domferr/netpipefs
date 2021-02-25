#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include "../include/utils.h"
#include "../include/scfiles.h"

#define FILEPATH "./tmp/prod/mypipe.txt"
#define MAXNUMBERS 10

int main(int argc, char** argv) {
    //Open file
    int fd = open(FILEPATH, O_WRONLY);
    MINUS1(fd, perror("open()"); return 1)

    //Write into it the first MAXNUMBERS
    int prec1 = 0, prec2 = 1, next = 1, counter = 2;
    printf("Sending: %d %d ", prec1, prec2);
    ISNEGATIVEERR(writen(fd, &prec1, sizeof(int)), return 1)
    ISNEGATIVEERR(writen(fd, &prec2, sizeof(int)), return 1)

    while (counter < MAXNUMBERS) {
        ISNEGATIVEERR(writen(fd, &next, sizeof(int)), return 1)
        printf("%d ", next);

        prec1 = prec2;
        prec2 = next;
        next = prec1 + prec2;
        counter++;
    }
    next = -1;
    ISNEGATIVEERR(writen(fd, &next, sizeof(int)), return 1)
    printf("-1\n");

    //END
    MINUS1ERR(close(fd), return 1)

    return 0;
}