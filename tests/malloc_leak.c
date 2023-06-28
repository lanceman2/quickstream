// Testing that valrind can find the memory lead by running this
// with valgrind.
//
// Try running:
//
//      ./valgrid_run_tests malloc_leak


#include <stdlib.h>

int main(void) {

    void *p = malloc(1);

    if(!p)
        return 1;

    // We needed a test to test that valgrind is working.

    // No free(ptr) should cause valgrind to fail.

    return 0;
}
