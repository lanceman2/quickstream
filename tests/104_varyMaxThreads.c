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
static atomic_uint acounter = 0;
static atomic_uint wcount = 0;


#define NumJobsPerBlock  33
#define NumBlocks        20


struct Block {
    struct QsJobsBlock jobsBlock;

    // It has this one job:
    struct QsJob jobs[NumJobsPerBlock];
};



static
bool Work(struct QsJob *j) {

    ++wcount;

    qsJob_unlock(j);

    ++acounter;
    ++counter;

    for(int k=0; k<20; ++k) {

        // After qsJob_unlock(j) this can run while other worker threads
        // run, and the non-atomic counter "count" can get a different
        // value than the atomic counter "acounter".
        for(int i=0; i<10; ++i) {
            --acounter;
            --counter;
            --acounter;
            --counter;
        }

        for(int i=0; i<10; ++i) {
            ++acounter;
            ++counter;
            ++acounter;
            ++counter;
        }
    }

    qsJob_lock(j);

    for(struct QsJob **job = j->peers; *job; ++job)
        qsJob_queueJob(*job);


    return false; // Until next time we are queued.
}



static
void Block_init(struct QsGraph *g, struct Block *b,
        struct QsThreadPool *tp) {

    qsBlock_init(g, &b->jobsBlock.block,
            0/*parentBlock*/, tp,
            QsBlockType_jobs, 0, 0);

    for(uint32_t i=0; i<NumJobsPerBlock; ++i) {
        struct QsJob *j = b->jobs + i;
        qsJob_init(j, &b->jobsBlock, Work, 0, 0);
        qsJob_addMutex(j, &mutex);
    }
}


static
void Catcher(int sig) {
    ERROR("Caught signal %d", sig);
    ASSERT(0);
}


int main(void) {

#ifndef NO_QUEUE // Try to make a version of this test that will let
                 // us run it with valgrind.
    if(getenv("VaLGRIND_RuN"))
        // We don't run this test with valgrind.  We find that the
        // valgrind thread scheduler does not let the main thread run soon
        // enough.  The short sleep() loop turns into a very long time.
        //
        // But it works, just takes a very long time to run.  I think...
        return 123;
#endif

    signal(SIGSEGV, Catcher);
    signal(SIGABRT, Catcher);

    struct QsGraph *g = qsGraph_create(0, 6, 0, 0, 0);
    struct QsThreadPool *tp = qsGraph_createThreadPool(g, 4, 0/*name*/);

    struct Block blocks[NumBlocks];
    memset(blocks, 0, NumBlocks*sizeof(struct Block));

    ASSERT(NumBlocks > 2);
    ASSERT(NumJobsPerBlock >= 1);


    qsGraph_threadPoolHaltLock(g, 0);


    for(uint32_t i = 0; i < NumBlocks; ++i)
        Block_init(g, blocks + i, tp);


    for(uint32_t i = 0; i < NumBlocks; ++i) {

        struct Block *b0; // The block before making the blocks[] array
                          // into a cycle.
        if(i != 0)
            b0 = blocks + i - 1;
        else
            b0 = blocks + NumBlocks - 1;

        struct Block *b1; // The block after making the blocks[] array
                          // into a cycle.
        if(i < NumBlocks-1)
            b1 = blocks + i + 1;
        else
            b1 = blocks;


        for(uint32_t k = 0; k < NumJobsPerBlock; ++k) {
            // Make some connections between different blocks.
            qsJob_addPeer(&blocks[i].jobs[k], &b0->jobs[k]);
            qsJob_addPeer(&blocks[i].jobs[k], &b1->jobs[k]);
#ifndef NO_QUEUE
            // No job locks needed because we have a thread pool halt
            qsJob_queueJob(&b0->jobs[k]);
            qsJob_queueJob(&b1->jobs[k]);
#endif
        }
    }

    qsGraph_threadPoolHaltUnlock(g);

#define WorkCount  70
#define PlusCount  21

// Use these and run "htop" and see CPUs in action with longer run.
//#define WorkCount  200000
//#define PlusCount  300000

    uint32_t ycount = 0;
#ifndef NO_QUEUE
    // Try to let the Work function get called by worker threads a few
    // times, then exit.
    //
    while(wcount < WorkCount) {
        ASSERT(sched_yield() == 0);
        ASSERT(usleep(1) == 0);
        ++ycount;
    }


    printf("\n  Results: wcount=%u atomic counter=%u  regular counter=%" PRIu64
            "  sched_yield() count=%" PRIu32 "\n\n",
                wcount, acounter, counter, ycount);
#endif


    // We destroy some thread pools while it is running.  The blocks that
    // are left thread-pool-less will get put into the newest existing
    // thread pool.

    for(uint32_t i=0; i<10; ++i) {

        if(i%2)
            qsThreadPool_setMaxThreads(tp, 1);
        else
            qsThreadPool_setMaxThreads(tp, 8);

#ifndef NO_QUEUE
        uint32_t workCount = wcount + PlusCount;
ERROR("wcount=%u workCount=%" PRIu32, wcount, workCount);
        // wait some more.
        while(wcount < workCount) {
            //ASSERT(sched_yield() == 0);
            ASSERT(usleep(1) == 0);
            ++ycount;
        }

    printf("\n - Results: wcount=%u atomic counter=%u  regular counter=%" PRIu64
            "  sched_yield() count=%" PRIu32 "\n\n",
                wcount, acounter, counter, ycount);
#endif
    }

    qsGraph_destroy(g);

    printf("\n  Results: wcount=%u atomic counter=%u  regular counter=%" PRIu64
            "  sched_yield() count=%" PRIu32 "\n\n",
                wcount, acounter, counter, ycount);

    return (wcount == acounter)?0:1;
}
