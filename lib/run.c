
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "../include/quickstream/app.h" // public interfaces

#include "debug.h"
#include "Dictionary.h"
#include "block.h"
#include "graph.h"
#include "builder.h"
#include "run.h"















void run(struct QsGraph *graph) {

    DASSERT(mainThread == pthread_self(), "Not graph main thread");

    if(graph->threadPools->maxThreads == 0) {
        // The main thread will run the stream.
    }


}
