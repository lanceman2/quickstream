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
#define NumBlocks        2



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

    if(getenv("VaLGRIND_RuN"))
        // We don't run this test with valgrind.  We find that the
        // valgrind thread scheduler does not let the main thread run soon
        // enough.  The short sleep() loop turns into a very long time.
        //
        // But it works, just takes a very long time to run.  I think...
        return 123;

    signal(SIGSEGV, Catcher);
    signal(SIGABRT, Catcher);

    struct QsGraph *g = qsGraph_create(0, 6, 0, 0, 0);

    struct Block blocks[NumBlocks];
    memset(blocks, 0, NumBlocks*sizeof(struct Block));

    // Array of pointers to thread pools:
    struct QsThreadPool *tps[NumBlocks];
    uint32_t numTps = 0;

    qsGraph_threadPoolHaltLock(g, 0);
    
    for(uint32_t i = 0; i < NumBlocks;) {

        tps[numTps] = qsGraph_createThreadPool(g, 4, 0/*name*/);

        Block_init(g, blocks + (i++), tps[numTps]);
        ++numTps;
     }


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

            qsJob_lock(&b0->jobs[k]);
            qsJob_queueJob(&b0->jobs[k]);
            qsJob_unlock(&b0->jobs[k]);

            qsJob_lock(&b1->jobs[k]);
            qsJob_queueJob(&b1->jobs[k]);
            qsJob_unlock(&b1->jobs[k]);
        }
    }

    qsGraph_threadPoolHaltUnlock(g);


    uint32_t ycount = 0;
    // Try to let the Work function get called by worker threads a few
    // times, then exit.
    //
    while(wcount < 100) {
        ASSERT(sched_yield() == 0);
        ASSERT(usleep(1) == 0);
        ++ycount;
    }


    fprintf(stderr, "\n  Results: wcount=%u atomic counter=%u  regular counter=%" PRIu64
            "  sched_yield() count=%" PRIu32 "\n\n",
                wcount, acounter, counter, ycount);

#ifdef REMOVE_TP
    // We destroy some thread pools while it is running.  The blocks that
    // are left thread-pool-less will get put into the newest existing
    // thread pool.
    qsThreadPool_destroy(tps[0]);

    // wait some more.
    while(wcount < 200) {
        ASSERT(sched_yield() == 0);
        ASSERT(usleep(1) == 0);
        ++ycount;
    }
#endif

    qsGraph_destroy(g);

    fprintf(stderr, "\n  Results: wcount=%u atomic counter=%u  regular counter=%" PRIu64
            "  sched_yield() count=%" PRIu32 "\n\n",
                wcount, acounter, counter, ycount);

    return (wcount == acounter)?0:1;
}
