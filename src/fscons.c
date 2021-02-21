#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "../include/utils.h"

#define FILEPATH "./tmp/cons/mypipe"

int main(int argc, char** argv) {
    //Open file
    FILE *fp;
    EQNULLERR(fp = fopen(FILEPATH, "r"), return 1)

    //Read from it
    int number, err;
    while ((err = fscanf(fp, "%d\n", &number)) > 0 && number != -1) {
        printf("%d ", number);
    }
    if (err < 0) {
        perror("fscanf");
        return 1;
    }

    //END
    fclose(fp);

    return 0;
}