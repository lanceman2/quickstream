
#include <stdlib.h>
#include <string.h>
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



#if 0
// Each worker thread will call this:
//
void *runWorker(struct QsGraph *g) {


    return 0;
}



void InitializeWorkQueue(struct QsGraph *g, struct QsThreadPool *p) {


    for(struct QsSimpleBlock *b = 

}
#endif



void run(struct QsGraph *graph) {

    DASSERT(mainThread == pthread_self(), "Not graph main thread");
    DASSERT(graph->threadPools);
    DASSERT(graph->flowState == QsGraphFlowing);


    if(graph->threadPools->maxThreads == 0) {
        // The main thread will run the stream.
    }


}
