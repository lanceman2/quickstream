#define _GNU_SOURCE // for PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP
#include <inttypes.h>
#include <signal.h>
#include <stdatomic.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

#include "../include/quickstream.h"

#include "../lib/debug.h"
#include "../lib/Dictionary.h"

#include "../lib/c-rbtree.h"
#include "../lib/name.h"
#include "../lib/threadPool.h"
#include "../lib/block.h"
#include "../lib/graph.h"
#include "../lib/job.h"

#ifndef MAXTHREADS
#  define MAXTHREADS 1
#endif

// We use this single mutex for all jobs.
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
//pthread_mutex_t mutex = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;


// Counters that count as the work that this program does.
static int counter = 0;
static atomic_uint acounter = 0;



struct Block {
    struct QsJobsBlock jobsBlock;

    // It has this one job:
    struct QsJob job;
};



static
bool Work(struct QsJob *j) {

    ++acounter;
    ++counter;

    // TESTING ...
    if(!(acounter % 100000))
        ERROR("acounter=%u", acounter);

    for(struct QsJob **job = j->peers; *job; ++job) {
        qsJob_queueJob(*job);
    }

    return false; // Until next time we are queued.
}



static
void Block_init(struct QsGraph *g, struct Block *b, struct QsJob *slave) {

    qsBlock_init(g, &b->jobsBlock.block,
            0/*parentBlock*/, 0/*thread pool*/,
            QsBlockType_jobs, 0, 0);

    struct QsJob *j = &b->job;

    qsJob_init(j, &b->jobsBlock, Work, 0, 0);

    // All the jobs will just use this mutex:
    qsJob_addMutex(j, &mutex);

    if(slave)
        qsJob_addPeer(j, slave);
}


static
void Catcher(int sig) {
    ERROR("Caught signal %d", sig);
    ASSERT(0);
}


int main(void) {


    if(getenv("VaLGRIND_RuN"))
        // We don't run this test with valgrind.  We find that the
        // valgrind thread scheduler does not let the main thread run soon
        // enough.  The short sleep() loop turns into a very long time.
        //
        // But it works, just takes a very long time to run.
        return 123;


    signal(SIGSEGV, Catcher);
    signal(SIGABRT, Catcher);

    DSPEW("MAXTHREADS=%" PRIu32, MAXTHREADS);
    struct QsGraph *g = qsGraph_create(0, MAXTHREADS, 0, 0, 0);

    qsGraph_threadPoolHaltLock(g, 0);

    static const uint32_t NumBlocks = 10;
    struct Block blocks[NumBlocks];
    struct QsJob *slave = 0;

    for(uint32_t i = 0; i < NumBlocks; ++i) {
        Block_init(g, blocks + i, slave);
        slave = &blocks[i].job;
    }
    // And now that the last slave is initialized we can add it:
    qsJob_addPeer(&blocks[0].job, slave);


    for(uint32_t i = 0; i < NumBlocks; ++i) {
        struct QsJob *j = &blocks[i].job;
        // No qsJob_lock() needed since we have a halt on the thread pool.
        qsJob_queueJob(j);
    }

    qsGraph_threadPoolHaltUnlock(g);

    uint32_t ycount = 0;
    // Try to let the Work function get called by worker threads and
    // then exit.
    //
    // Tests show that sometimes more than one call to sched_yield()
    // is required.  Looks like usleep() does a better job than
    // sched_yield().  Seeing ycount=114 without the usleep(1).
    //
    // I'm guessing my Linux kernel is tuned for optimizing a server
    // and not gaming.
    //
    // Note: since this is a part of the "quick" tests we need this to
    // run and finish quickly.
    //
    while(acounter < 3030) {
        // Work() was not called yet.
        ASSERT(sched_yield() == 0);
        ASSERT(usleep(1) == 0);
        ++ycount;
    }

    qsGraph_destroy(g);

    printf("\n  Results:  atomic counter=%u  regular counter=%u"
            "  sched_yield() count=%" PRIu32 "\n\n",
                acounter, counter, ycount);

    return 0;
}
