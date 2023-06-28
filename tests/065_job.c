//
// This test tests building blocks and jobs, but not necessarily running
// them.  It likely cleans-up the graph before it gets to run worker
// thread long enough to do anything.  The jobs are just incrementing and
// decrementing shared counters.
//
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


// We use this single mutex for all jobs.
//pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;


// Counters that count as the work that this program does.
static int counter = 0;
static atomic_uint acounter = 0;


// The sum all the number of times Work() is called for all
// jobs.
static atomic_uint workCount = 0;

// Large number
static const uint32_t workMax = 1000000;



struct Block {
    struct QsJobsBlock jobsBlock;

    // It has this one job:
    struct QsJob job;
};



static
bool Work(struct QsJob *j) {

    DSPEW();

    const uint32_t workCountIn = ++workCount;

    if(workCountIn < workMax) {
        ++acounter;
        ++counter;
    } else if(workCountIn < workMax * 2) {
        --acounter;
        --counter;
    }


    for(struct QsJob **job = j->peers; *job; ++job)
        qsJob_queueJob(*job);


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

    signal(SIGSEGV, Catcher);
    signal(SIGABRT, Catcher);

    struct QsGraph *g = qsGraph_create(0, 10, 0, 0, 0);

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


    qsGraph_threadPoolHaltUnlock(g);

    for(uint32_t i = 0; i < NumBlocks; ++i) {
        struct QsJob *j = &blocks[i].job;
        qsJob_lock(j);
        qsJob_queueJob(j);
        qsJob_unlock(j);
    }

    

    // Note: we never give the worker threads a chance to do any work.
    // Testing the cleanup of the library before the graph runs to
    // completion.
    //
    // Note: NOT calling qsGraph_destroy(g) will leave it to the
    // quickstream library destructor to do that, but that will crash the
    // program because the stack memory that we are using in the graph
    // (blocks and jobs) will be invalid after main() goes off the
    // function call stack when the library destructor is called.
    //
    qsGraph_destroy(g);

    printf("\n  Results:  atomic counter=%u   regular counter=%u\n\n",
                acounter, counter);

    return 0;
}
