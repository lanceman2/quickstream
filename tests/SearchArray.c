#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#include "../lib/SearchArray.h"


int main(void) {


#define NX 25
#define NY 22

    int x[NX] =
    { -23, -7, 0, 0, 2,
        2, 6, 6, 7, 8,
        8, 9, 9, 12, 12,
        12, 12, 23, 34, 40,
        41, 42, 43, 50, 56
    };

    int y[NY] = {
        -283, -24, -23, -22, -7,
        -6, 0, 6, 7, 8,
        8, 9, 9, 12, 33,
        34, 50, 54, 55, 56,
        57, 1000
    };

    printf(" Searching in:");
    for(int i = 0; i < NX; ++i)
        printf(" %d", x[i]);
    printf("\n");

    printf("Searching for:");
    for(int i = 0; i < NY; ++i)
        printf(" %d", y[i]);
    printf("\n");

    printf("\n");
    for(int i = 0; i < NY; ++i)
        printf(" %d => %d\n", y[i],  FindNearestInArray(y[i], x, NX));
    printf("\n");

    return 0;
}

