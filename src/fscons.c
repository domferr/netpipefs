#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include "../include/utils.h"
#include "../include/scfiles.h"

#define FILEPATH "./tmp/cons/mypipe.txt"

int main(int argc, char** argv) {
    //Open file
    int fd, maxnumbers;
    MINUS1ERR(fd = open(FILEPATH, O_RDONLY), return 1)

    //Read from it
    ISNEGATIVE(readn(fd, &maxnumbers, sizeof(int)), return 1)
    int *numbers = (int*) malloc(sizeof(int)*maxnumbers);
    EQNULL(numbers, perror("malloc"); return 1);
    ISNEGATIVE(readn(fd, numbers, sizeof(int)*maxnumbers), return 1)
    for (int i = 0; i < maxnumbers; ++i) {
        printf("%d ", numbers[i]);
    }
    printf("\n");

    //END
    MINUS1ERR(close(fd), return 1)

    return 0;
}