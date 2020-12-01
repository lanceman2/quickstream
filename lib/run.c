#include <signal.h>
#include <stdlib.h>
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


// This is the blocking call for the worker threads when there is only a
// system signal trigger, and no other triggers.  So there is only
// one simple block with a trigger, this signal trigger, sig.
static
void WaitForWork_signal(void) {

    DASSERT(sig);

}


static
void (*waitForWork)(void);


// Each worker thread will call this:
//
//static
void *runWorker(struct QsGraph *g, struct QsThreadPool *tp) {

    // Check for work:


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
}
