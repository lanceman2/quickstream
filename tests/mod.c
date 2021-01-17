#include <stdio.h>
#include <inttypes.h>

int main(void) {

    uint64_t x = 5;
    uint32_t y = 3;

    printf("%" PRIu64 "%c%" PRIu32 "=%" PRIu64 "\n",
        x, '%', y, x%y);

    return 0;
}
