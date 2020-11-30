#include <signal.h>
#include <stdlib.h>
#include <pthread.h>

#include "../include/quickstream/app.h" // public interfaces

#include "debug.h"
#include "Dictionary.h"
#include "block.h"
#include "graph.h"
#include "threadPool.h"
#include "builder.h"
#include "run.h"
#include "trigger.h"



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
    struct QsBlock *bWithT = graph->firstBlock;
    for(; bWithT; bWithT = bWithT->next) {
        if(bWithT->isSuperBlock) continue;
        if(((struct QsSimpleBlock *) bWithT)->triggers)
            // We have at least one trigger.
            break;
    }
    if(!bWithT) {
        NOTICE("No stream triggers found");
        // There is nothing to run.
        return;
    }

    // First queue up the triggered blocks in the into their thread pool.





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
