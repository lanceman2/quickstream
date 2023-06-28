#include <inttypes.h>
#include <signal.h>
#include <unistd.h>
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


// We use this single mutex for all jobs.
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;


// Counters that count as the work that this program does.
static uint64_t counter = 0;
static atomic_uint acounter = 0, wcount = 0;



struct Block {
    struct QsJobsBlock jobsBlock;

    // It has this one job:
    struct QsJob job;
};



static
bool Work(struct QsJob *j) {

    ++wcount;

    qsJob_unlock(j);

    // After qsJob_unlock(j) this can run while other worker threads run,
    // and the non-atomic counter "count" can get a different value than the
    // atomic counter "acounter".
    for(int i=0; i<20; ++i) {
        --acounter;
        --counter;
        --acounter;
        --counter;
    }

    ++acounter;
    ++counter;

    for(int i=0; i<20; ++i) {
        ++acounter;
        ++counter;
        ++acounter;
        ++counter;
    }


    qsJob_lock(j);


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

    struct QsGraph *g = qsGraph_create(0, 7, 0, 0, 0);

    static const uint32_t NumBlocks = 10;
    struct Block blocks[NumBlocks];
    struct QsJob *slave = 0;

    qsGraph_threadPoolHaltLock(g, 0);
    
    for(uint32_t i = 0; i < NumBlocks; ++i) {
        Block_init(g, blocks + i, slave);
        slave = &blocks[i].job;
    }
    // And now that the last slave is initialized we can add it:
    qsJob_addPeer(&blocks[0].job, slave);

    qsGraph_threadPoolHaltUnlock(g);


    for(uint32_t i = 0; i < NumBlocks; ++i) {
        struct QsJob *j = &blocks[i].job;
        qsJob_lock(j);
        qsJob_queueJob(j);
        qsJob_unlock(j);
    }

    uint32_t ycount = 0;
    // Try to let the Work function get called by worker threads a few
    // times, then exit.
    //
    // Note: since this is a part of the "quick" tests we need this to
    // run and finish quickly.  Runs in about 0.003 seconds on my machine
    // (Tue Jan 31 06:32:20 AM EST 2023).
    //
    while(wcount < 73) {
        // Work() was not called yet.
        ASSERT(sched_yield() == 0);
        ASSERT(usleep(1) == 0);
        ++ycount;
    }

    // Note:
    //
    // Results: wcount=103 atomic counter=103  regular counter=81  sched_yield() count=8
    //
    // The atomic and regular counter differ.  The regular counter should
    // get a "wrong" value, given the Work() functions runs in parallel.

    printf("\n  Results: wcount=%u atomic counter=%u  regular counter=%" PRIu64
            "  sched_yield() count=%" PRIu32 "\n\n",
                wcount, acounter, counter, ycount);

    qsGraph_destroy(g);

    printf("\n  Results: wcount=%u atomic counter=%u  regular counter=%" PRIu64
            "  sched_yield() count=%" PRIu32 "\n\n",
                wcount, acounter, counter, ycount);

    return (wcount == acounter)?0:1;
}
