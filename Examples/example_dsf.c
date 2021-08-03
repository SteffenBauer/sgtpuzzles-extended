#include <stdio.h>
#include "puzzles.h"

void fatal(const char *fmt, ...) {
    printf(fmt);
    exit(0);
}

int main(int argc, char *argv[]) {
    int w = 4;
    int h = 4;
    int i;

    int *dsf;

    dsf = snewn(w*h,int);
    dsf_init(dsf, w*h);

/*
    +---+---+---+---+
    | o===o | o | o |
    +-I-+-I-+-I-+-I-+
    | o===o | o===o |
    +---+---+---+---+
    | o===o | o===o |
    +-I-+---+-I-+-I-+
    | o |   | o===o |
    +---+---+---+---+
*/

    dsf_merge(dsf, 0, 1);
    dsf_merge(dsf, 0, 4);
    dsf_merge(dsf, 1, 0);
    dsf_merge(dsf, 1, 5);
    dsf_merge(dsf, 2, 6);
    dsf_merge(dsf, 3, 7);

    dsf_merge(dsf, 4, 0);
    dsf_merge(dsf, 4, 5);
    dsf_merge(dsf, 5, 1);
    dsf_merge(dsf, 5, 4);
    dsf_merge(dsf, 6, 2);
    dsf_merge(dsf, 6, 7);
    dsf_merge(dsf, 7, 3);
    dsf_merge(dsf, 7, 6);
    
    dsf_merge(dsf, 8, 9);
    dsf_merge(dsf, 8, 12);
    dsf_merge(dsf, 9, 8);
    dsf_merge(dsf, 10, 11);
    dsf_merge(dsf, 10, 14);
    dsf_merge(dsf, 11, 10);
    dsf_merge(dsf, 11, 15);
    
    dsf_merge(dsf, 12, 8);
    dsf_merge(dsf, 14, 10);
    dsf_merge(dsf, 14, 15);
    dsf_merge(dsf, 15, 11);
    dsf_merge(dsf, 15, 14);

    for (i=0;i<16;i++)
        printf("Cell %i is member of dsf %i (size %i)\n", i, dsf_canonify(dsf, i), dsf_size(dsf, i));

    sfree(dsf);

    return 0;
}
