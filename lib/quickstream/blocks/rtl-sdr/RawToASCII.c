#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>


int main(void) {

    //uint64_t count = 0;
    uint8_t value[2];

    while(fread(&value, 2, 1, stdin))
        printf("%" PRIu8 " %" PRIu8 "\n", value[0], value[1]);

    return 0;
}
