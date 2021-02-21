#include <string.h>
#include <stdio.h>
#include "../include/utils.h"

#define FILEPATH "./tmp/prod/mypipe"
#define MAXNUMBERS 10

int main(int argc, char** argv) {
    //Open file
    FILE *fp = fopen(FILEPATH, "w");
    EQNULL(fp, perror("fopen()"); return 1)

    //Write into it
    int prec1 = 0, prec2 = 1, next = 1, counter = 2;
    ISNEGATIVEERR(fprintf(fp, "%d\n", prec1), return 1)
    ISNEGATIVEERR(fprintf(fp, "%d\n", prec2), return 1)

    while (counter < MAXNUMBERS) {
        ISNEGATIVEERR(fprintf(fp, "%d\n", next), return 1)

        prec1 = prec2;
        prec2 = next;
        next = prec1 + prec2;
        counter++;
    }
    ISNEGATIVEERR(fprintf(fp, "%d\n", -1), return 1)

    //END
    fclose(fp);

    return 0;
}