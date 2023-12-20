#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>


int main(void) {

    uint8_t value[2];

    while(fread(&value, 2, 1, stdin)) {
        float x[2];
        x[0] = value[0];
        x[1] = value[1];

        x[0] -= 127.5;
        x[1] -= 127.5;

        x[0] /= 127.5;
        x[1] /= 127.5;

        if(!fwrite(x, sizeof(x), 1, stdout))
            return 0;
    }

    return 0;
}
