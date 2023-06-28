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


#define NumJobsPerBlock  2
#define NumBlocks        11

// #define NumThreadPools  12  gives a *** stack smashing detected ***,
// due to a stack overflow, I suppose.  We do use some function recursion
// in the quickstream code.  Maybe we should not use so much function
// recursion in the quickstream code.  Maybe all the arrays declared on
// the stack does not help either.
//
// On System:
// Linux joe 5.15.0-56-generic #62-Ubuntu SMP Tue Nov 22 19:54:14 UTC 2022
// x86_64 x86_64 x86_64 GNU/Linux

#define NumThreadPools   7


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

static uint32_t ycount = 0;
static struct Block blocks[NumBlocks];


static inline
void Wait(uint32_t plus) {

    uint32_t workCount = wcount + plus;

        while(wcount < workCount) {
        ASSERT(sched_yield() == 0);
        ASSERT(usleep(1) == 0);
        ++ycount;
    }

    fprintf(stderr, "\n  Results: wcount=%u atomic counter=%u  regular counter=%" PRIu64
            "  sched_yield() count=%" PRIu32 "\n\n",
                wcount, acounter, counter, ycount);
}



int main(void) {

#ifndef NO_QUEUE
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

     memset(blocks, 0, NumBlocks*sizeof(struct Block));

    // Array of pointers to thread pools:
    struct QsThreadPool *tps[NumBlocks];
    uint32_t numTps = 0;


    for(uint32_t i = 0; i < NumThreadPools; ++i)
        tps[i] = qsGraph_createThreadPool(g, 1+(i%12), 0/*name*/);

    qsGraph_threadPoolHaltLock(g, 0);

    for(uint32_t i = 0; i < NumBlocks; ++i)
        Block_init(g, blocks + i, tps[numTps]);


    for(uint32_t i = 0; i < NumBlocks; ++i) {

        struct Block *b0;
        if(i != 0)
            b0 = blocks + i - 1;
        else
            b0 = blocks + NumBlocks - 1;

        struct Block *b1;
        if(i != NumBlocks-1)
            b1 = blocks + i + 1;
        else
            b1 = blocks;

        for(uint32_t k = 0; k < NumJobsPerBlock; ++k) {
            qsJob_addPeer(&blocks[i].jobs[k], &b0->jobs[k]);
            qsJob_addPeer(&blocks[i].jobs[k], &b1->jobs[k]);
#ifndef NO_QUEUE
            // No job locks needed bcause we have a thread pools halt.
            qsJob_queueJob(&b0->jobs[k]);
            qsJob_queueJob(&b1->jobs[k]);
#endif
        }
    }

    qsGraph_threadPoolHaltUnlock(g);

#ifndef NO_QUEUE
#  define WorkCountInit  30
#  define WorkCountPlus  20

// For a long running test so to look at with "htop":
//#  define WorkCountInit  300000
//#  define WorkCountPlus  200000

#else
#  define WorkCountInit  0
#  define WorkCountPlus  0
#endif

    // Try to let the Work function get called by worker threads a few
    // times:
    //
    Wait(WorkCountInit);


    for(uint32_t i = 0; i < 9; ++i) {

        struct QsThreadPool *tp = tps[i%NumThreadPools];

        for(uint32_t k = 0; k < NumBlocks - 1; ++k)
            qsThreadPool_addBlock(tp,
                &blocks[k].jobsBlock.block);

        Wait(WorkCountPlus);
    }

    qsGraph_destroy(g);

    Wait(0);

    return (wcount == acounter)?0:1;
}
