#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdatomic.h>

#include "../include/quickstream.h"

#include "debug.h"
#include "Dictionary.h"

#include "c-rbtree.h"
#include "name.h"
#include "threadPool.h"
#include "block.h"
#include "graph.h"
#include "job.h"
#include "port.h"
#include "parameter.h"
#include "stream.h"

#include "graphConnect.h"



int qsGraph_disconnect(struct QsPort *p) {

    NotWorkerThread();

    DASSERT(p);
    DASSERT(p->block);
    struct QsGraph *g = p->block->graph;
    DASSERT(g);

    int ret = -1;

    CHECK(pthread_mutex_lock(&g->mutex));

    if(IsParameter(p)) {
        ret = qsParameter_disconnect((void *) p);
        goto finish;
    }

    ASSERT(p->portType == QsPortType_input ||
                p->portType == QsPortType_output);
    char *end = 0;
    uint32_t num = strtoul(p->name, &end, 10);
    // We require/assume that this was a unsigned number as a string.
    ASSERT(end != p->name);
    ret = qsBlock_disconnectStreams((void *) p->block, p, num);

finish:

    CHECK(pthread_mutex_unlock(&g->mutex));
    return ret;
}


int qsGraph_disconnectByStrings(struct QsGraph *g,
        const char *blockName, const char *portType, const char *portName) {

    NotWorkerThread();

    DASSERT(g);
    DASSERT(blockName && blockName[0]);
    DASSERT(portType && portType[0]);
    DASSERT(portName && portName[0]);


    int ret = -1; // default return is -1 error.

    // g->mutex is a recursive mutex.
    CHECK(pthread_mutex_lock(&g->mutex));

    struct QsBlock *b = qsGraph_getBlock(g, blockName);
    if(!b) goto finish;

    struct QsPort *p = qsBlock_getPort(b, GetPortType(portType), portName);
    if(!p) goto finish;


    if(IsParameter(p))
        //
        // Note: struct QsParameter inherits struct QsPort, so we cast the
        // easy way with void *.
        //
        // There are additional error modes in qsParameter_disconnect().
        //
        ret = qsParameter_disconnect((void *) p);
    else {
        ASSERT(p->portType == QsPortType_input ||
                p->portType == QsPortType_output);
        // Get the port number:
        char *end = 0;
        uint32_t num = strtoul(p->name, &end, 10);
        // We require/assume that this was a unsigned number as a string.
        ASSERT(end != p->name);
        ret = qsBlock_disconnectStreams((void *) p->block, p, num);
    }


finish:

    CHECK(pthread_mutex_unlock(&g->mutex));
    return ret;
}
