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



// Each worker thread will call this:
//
//static
void *runWorker(struct QsGraph *g) {

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
                // it a job now.
                AddBlockToThreadPoolQueue(smB);
        }
    }




    if(graph->threadPools->maxThreads == 0) {
        // This is the only thread pool
        DASSERT(graph->threadPools->next == 0);
        // The main thread will run the stream.
        runWorker(graph);

    } else {

        // We will have worker threads.

        ERROR("Write MORE CODE HERE");
    }
}
