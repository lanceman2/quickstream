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
static struct Block block;


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

    memset(&block, 0, sizeof(struct Block));

    qsGraph_threadPoolHaltLock(g, 0);

    Block_init(g, &block, 0);

    qsJob_addPeer(&block.jobs[1], &block.jobs[0]);
    qsJob_addPeer(&block.jobs[0], &block.jobs[1]);

#ifndef NO_QUEUE
    // No job locks needed because we have a thread pool halt.
    qsJob_queueJob(&block.jobs[0]);
    qsJob_queueJob(&block.jobs[1]);
#endif

    qsGraph_threadPoolHaltUnlock(g);

#ifndef NO_QUEUE
#  define WorkCount  300
#else
#  define WorkCount  0
#endif

    // Try to let the Work function get called by worker threads a few
    // times:
    //
    Wait(WorkCount);

    qsGraph_destroy(g);

    Wait(0);

    return (wcount == acounter)?0:1;
}
