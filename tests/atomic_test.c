#include <signal.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <pthread.h>

#include "../lib/debug.h"


pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;


//https://www.gnu.org/software/libc/manual/html_node/Atomic-Types.html
// Not a good multi-threaded counter.
//volatile sig_atomic_t iCount = 0;
int iCount = 0;


int mCount = 0;

// This one works.
atomic_uint goodCount = 0;



const int maxCount = 1000000;


void *CountUp(void *data) {

    int count = 0;
    for(;count<maxCount; ++count) {

        CHECK(pthread_mutex_lock(&mutex));
        // This is atomic:
        ++mCount;
        CHECK(pthread_mutex_unlock(&mutex));

        // This will not be atomic:
        ++iCount;

        // this will be atomic:
        ++goodCount;
    }

    return 0;
}


void *CountDown(void *data) {

    int count = 0;
    for(;count<maxCount; ++count) {

        CHECK(pthread_mutex_lock(&mutex));
        // This is atomic:
        --mCount;
        CHECK(pthread_mutex_unlock(&mutex));

        // This will not be atomic:
        --iCount;

        // this will be atomic:
        --goodCount;
    }

    return 0;
}





int main(void) {

    const int numThreads = 10;
    pthread_t threads[numThreads];

    for(int i = 0; i<numThreads; ++i) {
        void *(*run)(void *);
        if(i%2) run = CountUp;
        else run = CountDown;
        CHECK(pthread_create(threads + i, 0, run, 0));
    }

    for(int i = 0; i<numThreads; ++i)
        CHECK(pthread_join(threads[i], 0));

    printf("with mutex mCount=%d\n", mCount);

    printf("no mutex: int iCount=%d\n",iCount);

    printf("no mutex atomic_int goodCount=%d\n", goodCount);

    return 0;
}
