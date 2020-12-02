#include <signal.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>

#include "../include/quickstream/app.h" // public interfaces

#include "debug.h"
#include "Dictionary.h"
#include "block.h"
#include "trigger.h"
#include "blockJobsLists.h"
#include "graph.h"
#include "threadPool.h"
#include "builder.h"
#include "run.h"


static inline
bool IfNeedMutex(struct QsThreadPool *tp) {
    return (tp->maxThreads > 1);
}


static inline
void IfMutexLock(struct QsThreadPool *tp) {

    if(IfNeedMutex(tp))
        CHECK(pthread_mutex_lock(&tp->mutex));
}


static inline
void IfMutexUnlock(struct QsThreadPool *tp) {

    if(IfNeedMutex(tp))
        CHECK(pthread_mutex_unlock(&tp->mutex));
}


// We need the threadPool mutex before calling this.
//
// This is the blocking call for the worker threads when there is only a
// system signal trigger, and no other triggers.  So there is only
// one simple block with a trigger, this signal trigger, sig.
static
bool WaitForWork_signal(struct QsThreadPool *tp) {

    DASSERT(sig);
    struct QsSimpleBlock *b = sig->trigger.block;
    DASSERT(b->triggers);
    DASSERT(b->threadPool == tp);

    IfMutexUnlock(tp);

    ASSERT(-1 == pause());
    // The signal catcher should have caught SIGALRM
    DASSERT(errno == EINTR);

    IfMutexLock(tp);

    if(sig->triggered) {

        // Reset flag.
        sig->triggered = 0;

        TriggersToLastJob(b, &sig->trigger);
        if(!b->blockInThreadPoolQueue)
            // We don't add it to the thread pool job queue unless it's
            // not in it already.
            AddBlockToThreadPoolQueue(b);
        
        return true;
    }

    // It was a different signal from SIGALRM
    return false; // No more working.
}


// Return true if we have work or false if the graph is done running.
static
bool (*waitForWork)(struct QsThreadPool *tp) = 0;


// Each worker thread will call this:
//
static
void *runWorker(struct QsGraph *g, struct QsThreadPool *tp) {

    DASSERT(waitForWork);

    IfMutexLock(tp);

    // Check for work:

    while(waitForWork(tp)) {

        struct QsSimpleBlock *b = PopBlockFromThreadPoolQueue(tp);

        DASSERT(b->firstJob);

        while(b->firstJob) {
            struct QsTrigger *t = PopJobBackToTriggers(b);

            t->callback(t->userData);
        }
    }

    IfMutexUnlock(tp);

    return 0;
}


void run(struct QsGraph *graph) {

    DASSERT(mainThread == pthread_self(), "Not graph main thread");
    DASSERT(graph->threadPools);
    DASSERT(graph->flowState == QsGraphFlowing);

    // Find a block with a trigger.
    struct QsBlock *b = graph->firstBlock;
    for(; b; b = b->next) {
        if(b->isSuperBlock) continue;
        // There should be no triggered jobs yet.
        DASSERT(((struct QsSimpleBlock *) b)->firstJob == 0);
        DASSERT(((struct QsSimpleBlock *) b)->lastJob == 0);
        if(((struct QsSimpleBlock *) b)->triggers)
            // We have at least one trigger.
            break;
    }
    if(!b) {
        NOTICE("No stream triggers found");
        // There is nothing to run.
        return;
    }

    // First queue up the triggered blocks in the into their thread pool.
    //
    b = graph->firstBlock;
    for(; b; b = b->next) {
        if(b->isSuperBlock) continue;
        struct QsTrigger *t = ((struct QsSimpleBlock *) b)->triggers;
        if(t) {
            struct QsSimpleBlock *smB = (struct QsSimpleBlock *) b;
            DASSERT(t->block == smB);
            while(t) {
                // We need the next trigger now since we may edit the list
                // in TriggersToFirstJob();
                struct QsTrigger *nextT = t->next;
                if(t->checkTrigger()) {
                    // Add this trigger thingy job to a listed list of
                    // jobs.
                    TriggersToFirstJob(smB, t);
                }
                t = nextT;
            }
            if(smB->firstJob)
                // This block, b (smB), has a trigger triggered.  We call
                // it a job now.  Now we queue it up for a worker thread.
                AddBlockToThreadPoolQueue(smB);
        }
    }

    // TODO: Add a will it run anything check here.


    for(struct QsThreadPool *tp = graph->threadPools; tp; tp = tp->next)
        if(IfNeedMutex(tp))
            CHECK(pthread_mutex_init(&tp->mutex, 0));



    if(graph->threadPools->maxThreads == 0) {
        // This is the only thread pool and it has no threads.  This is a
        // special case of running with the main thread and returning when
        // finished or signaled.
        DASSERT(graph->threadPools->next == 0);

        // TODO: How do we determine which waitForWork() function to use:
        waitForWork = WaitForWork_signal;

        // The main thread will run the stream.
        runWorker(graph, graph->threadPools);

    } else {

        // TODO: HERE initialize each threadPool mutexs and conditionals.


        // TODO: HERE We will have worker threads.


        ERROR("Write MORE CODE HERE");
    }

    for(struct QsThreadPool *tp = graph->threadPools; tp; tp = tp->next)
        if(IfNeedMutex(tp))
            CHECK(pthread_mutex_destroy(&tp->mutex));
}
