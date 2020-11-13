
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "../include/quickstream/app.h" // public interfaces

#include "debug.h"
#include "Dictionary.h"
#include "app.h"
#include "builder.h"


struct QsGraph *qsGraphCreate(void) {

    struct QsGraph *graph = calloc(1, sizeof(*graph));
    ASSERT(graph, "calloc(1,%zu) failed", sizeof(*graph));
    graph->blocks = qsDictionaryCreate();

    graph->mainThread = pthread_self();

    return graph;
}


void qsGraphDestroy(struct QsGraph *graph) {

    DASSERT(graph);
    DASSERT(graph->blocks);

    // This will destroy all the blocks too, via the
    // qsDictionarySetFreeValueOnDestroy() thingy.
    qsDictionaryDestroy(graph->blocks);

#ifdef DEBUG
    memset(graph, 0, sizeof(*graph));
#endif


    free(graph);
}
