#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include "../include/utils.h"
#include "../include/scfiles.h"

#define FILEPATH "./tmp/cons/mypipe.txt"
#define MAXNUMBERS 10

int main(int argc, char** argv) {
    //Open file
    int fd;
    MINUS1ERR(fd = open(FILEPATH, O_RDONLY), return 1)

    //Read from it
    int numbers[MAXNUMBERS];
    ISNEGATIVE(readn(fd, &numbers, sizeof(int)*MAXNUMBERS), return 1)
    for (int i = 0; i < MAXNUMBERS; ++i) {
        printf("%d ", numbers[i]);
    }
    printf("\n");

    //END
    MINUS1ERR(close(fd), return 1)

    return 0;
}