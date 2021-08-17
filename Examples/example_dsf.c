#include <stdio.h>
#include "puzzles.h"

void fatal(const char *fmt, ...) {
    printf(fmt);
    exit(0);
}

void dsf_print_groups(int *dsf, int size) {
    int i,j,n;
    bool found;
    int processed_count = 0, group_count;
    int *processed_cells = snewn(size, int);
    int *group_cells = snewn(size, int);
    for (i=0;i<size;i++) processed_cells[i] = -1;
    while(true) {
        group_count = 0;
        for (i=0;i<size;i++) {
            found = false;
            for (j=0;j<size;j++) {
                if (processed_cells[j] == i) {
                    found = true;
                    break;
                }
            }
            if (!found) break;
        }
        if (i==size) break;
        printf("Group ID %i, size %i: ", dsf_canonify(dsf, i), dsf_size(dsf, i));
        i = dsf_canonify(dsf, i);
        for (n = 0; n < size; n++) {
            if (dsf_canonify(dsf, n) == i) {
                processed_cells[processed_count++] = n;
                group_cells[group_count++] = n;
            }
        }
        for (n=0;n<group_count;n++)
            printf("%i ", group_cells[n]);
        printf("\n");
    }
    sfree(processed_cells);
    sfree(group_cells);
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

    dsf_print(dsf, w*h);
    dsf_print_groups(dsf, w*h);

    sfree(dsf);

    return 0;
}
