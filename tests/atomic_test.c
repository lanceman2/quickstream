#include <signal.h>
#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>

#include "../lib/debug.h"


pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;


//https://www.gnu.org/software/libc/manual/html_node/Atomic-Types.html
volatile sig_atomic_t aCount = 0;

int mCount = 0;

const int maxCount = 1000000;


void *Run(void *data) {

    int count = 0;
    for(;count<maxCount; ++count) {

        CHECK(pthread_mutex_lock(&mutex));
        // This is atomic:
        ++mCount;
        CHECK(pthread_mutex_unlock(&mutex));
        // This will not be atomic:
        ++aCount;
    }

    return 0;
}






int main(void) {

    const int numThreads = 7;
    pthread_t threads[numThreads];

    for(int i = 0; i<numThreads; ++i)
        CHECK(pthread_create(threads + i, 0, Run, 0));

    for(int i = 0; i<numThreads; ++i)
        CHECK(pthread_join(threads[i], 0));

    printf("mCount=%d\n", mCount);

    if(numThreads * maxCount == aCount)
        printf("GOT count = %d x %d = %d\n", numThreads, maxCount, aCount);
    else
        printf("GOT BAD count = %d x %d = %d != %d\n", numThreads, maxCount,  numThreads * maxCount, aCount);

    return 0;
}
