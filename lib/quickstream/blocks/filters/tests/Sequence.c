#include <inttypes.h>
#include <stdio.h>
#include <stdbool.h>
#include <signal.h>
#include <string.h>
#include "Sequence.h"

static bool running = true;

static void catch(int sig) {

    fprintf(stderr, "Caught signal %d ... exiting\n", sig);
    running = false;
    signal(SIGTERM, 0);
    signal(SIGINT, 0);
}


int main(void) {

    signal(SIGTERM, catch);
    signal(SIGINT, catch);

    struct RandomString r;
    randomString_init(&r, 0);

    const size_t LEN = 10;
    char str[LEN];

    while(running) {
        printf("%10.10s", randomString_get(&r, LEN, str));
    }

    fprintf(stderr, "done\n");

    return 0;
}
