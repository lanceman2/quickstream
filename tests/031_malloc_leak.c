#include <stdlib.h>
#include <stdio.h>

int main(void) {

    char *ptr = malloc(1);
    printf("ptr=%p\n", ptr);

    // We needed a test to test that valgrind is working.

    // No free(ptr) should cause valgrind to fail.

    return 0;
}
