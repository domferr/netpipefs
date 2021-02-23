#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include "../include/utils.h"
#include "../include/scfiles.h"

#define FILEPATH "./tmp/cons/mypipe"

int main(int argc, char** argv) {
    //Open file
    int fd = open(FILEPATH, O_RDONLY);
    MINUS1(fd, perror("open()"); return 1)

    //Read from it
    int number, err;
    while ((err = readn(fd, &number, sizeof(int))) > 0 && number != -1) {
        printf("%d ", number);
    }
    ISNEGATIVE(err, perror("readn"); return 1)
    printf("\n");
    
    //END
    MINUS1ERR(close(fd), return 1)

    return 0;
}