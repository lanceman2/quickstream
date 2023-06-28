#include <signal.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <pthread.h>

#include "../lib/debug.h"


static pthread_once_t key_once = PTHREAD_ONCE_INIT;


// Will this get called twice?
static void make_key(void) {

    ERROR();
}



int main(void) {

    CHECK(pthread_once(&key_once, make_key));
    CHECK(pthread_once(&key_once, make_key));
    CHECK(pthread_once(&key_once, make_key));
    CHECK(pthread_once(&key_once, make_key));
    CHECK(pthread_once(&key_once, make_key));
    CHECK(pthread_once(&key_once, make_key));
    CHECK(pthread_once(&key_once, make_key));


    key_once = PTHREAD_ONCE_INIT;
    CHECK(pthread_once(&key_once, make_key));
    CHECK(pthread_once(&key_once, make_key));
    CHECK(pthread_once(&key_once, make_key));
    CHECK(pthread_once(&key_once, make_key));

    return 0;
}
