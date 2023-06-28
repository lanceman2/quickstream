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



struct QsBlock *qsGraph_getBlock(struct QsGraph *g,
        const char *name) {

    NotWorkerThread();

    if(!g) {
        // We are calling this from a block module callback.
        struct QsBlock *b = GetBlock(CB_DECLARE|CB_CONFIG|CB_UNDECLARE,
                QS_TYPE_MODULE, 0);
        // if not assert...
        ASSERT(b);
        g = b->graph;
        DASSERT(g);

        char *bname = (char *) name;
        size_t len = strlen(b->name) + strlen(name) + 2;

        if(b->type == QsBlockType_super) {
            // Children of super blocks have block names with
            // the super block name plus ':' prepended.
            bname = malloc(len);
            ASSERT(bname, "malloc(%zu) failed", len);
            sprintf(bname, "%s:%s", b->name, name);
        }

        // The thread pool must be halted to run the blocks declare()
        // configure or undeclare() so we should already have a graph
        // mutex lock.
        struct QsBlock *ret = qsDictionaryFind(b->graph->blocks, bname);

        if(b->type == QsBlockType_super) {
            DZMEM(bname, len);
            free(bname);
        }

        return ret;
    }

    // This is thread-safe.  Looks like any thread can call this.

    // g->mutex is recursive.
    CHECK(pthread_mutex_lock(&g->mutex));
    struct QsBlock *b = qsDictionaryFind(g->blocks, name);
    CHECK(pthread_mutex_unlock(&g->mutex));

    return b;
}


struct QsPort *qsBlock_getPort(struct QsBlock *b,
        enum QsPortType pt, const char *pname) {

    NotWorkerThread();

    if(!b) {
        // We are calling this from a block module callback.
        struct QsBlock *b = GetBlock(CB_DECLARE|CB_CONFIG|CB_UNDECLARE,
                QS_TYPE_MODULE, 0);
        ASSERT(b);
        return _qsBlock_getPort(b, pt, pname);
    }
    return _qsBlock_getPort(b, pt, pname);
}


int qsGraph_connect(struct QsPort *pA, struct QsPort *pB) {

    NotWorkerThread();

    DASSERT(pA);
    DASSERT(pB);
    DASSERT(pA->block);
    DASSERT(pB->block);
    struct QsGraph *g = pA->block->graph;
    DASSERT(g);
    DASSERT(pB->block->graph == g);

    int ret = -1;

    CHECK(pthread_mutex_lock(&g->mutex));

    if(IsParameter(pA) && IsParameter(pB)) {
        ret = qsParameter_connect((void *) pA, (void *) pB);
        goto finish;
    }

    ASSERT(!IsParameter(pA) && !IsParameter(pB));

    if(pA->portType == QsPortType_output &&
            pB->portType == QsPortType_input) {
        // switch them
        struct QsPort *p = pA;
        pA = pB;
        pB = p;
    }
    ASSERT(pA->portType == QsPortType_input &&
            pB->portType == QsPortType_output);

    ret = qsBlock_connectStreams(
            (void *) pA->block,
            ((struct QsInput *) pA)->portNum,
            (void *) pB->block,
            ((struct QsInput *) pB)->portNum);

finish:

    CHECK(pthread_mutex_unlock(&g->mutex));
    return ret;
}


int qsGraph_connectByBlock(struct QsGraph *g,
        struct QsBlock *bA, const char *typeA, const char *nameA,
        struct QsBlock *bB, const char *typeB, const char *nameB) {

    NotWorkerThread();

    if(!g) {
        // We are calling this from a block module callback
        struct QsBlock *b = GetBlock(CB_DECLARE|CB_CONFIG,
                0, QsBlockType_super);
        ASSERT(b);
        g = b->graph;
        DASSERT(g);
    }

    DASSERT(bA);
    DASSERT(typeA && typeA[0]);
    DASSERT(nameA && nameA[0]);
    DASSERT(bB);
    DASSERT(typeB && typeB[0]);
    DASSERT(nameB && nameB[0]);


    int ret = -1; // default return is -1 error.

    // g->mutex is a recursive mutex.
    CHECK(pthread_mutex_lock(&g->mutex));

    struct QsPort *pA = qsBlock_getPort(bA, GetPortType(typeA), nameA);
    if(!pA) {
        ERROR("No such port: \"%s:%s:%s\"", bA->name, typeA, nameA);
        goto finish;
    }

    struct QsPort *pB = qsBlock_getPort(bB, GetPortType(typeB), nameB);
    if(!pB) {
        ERROR("No such port: \"%s:%s:%s\"", bB->name, typeB, nameB);
        goto finish;
    }

    if(IsParameter(pA) && IsParameter(pB))
        //
        // Note: struct QsParameter inherits struct QsPort, so we cast the
        // easy way with void *.
        //
        // There are additional error modes in qsParameter_connect().
        //
        ret = qsParameter_connect((void *) pA, (void *) pB);
    else {
        // This is connecting two stream ports.
        //
        // One port must be an input and one port must be an output.
        ASSERT(pA->portType == QsPortType_input ||
                pB->portType == QsPortType_input);
        ASSERT(pA->portType == QsPortType_output ||
                pB->portType == QsPortType_output);
        // The port numbers:
        char *end = 0;
        uint32_t numA, numB;
        numA = strtoul(pA->name, &end, 10);
        // We require/assume that this was a unsigned number as a string.
        ASSERT(end != pA->name);
        end = 0;
        numB = strtoul(pB->name, &end, 10);
        // We require/assume that this was a unsigned number as a string.
        ASSERT(end != pB->name);

        // bA and bB may be top block or super blocks with port aliases.
        // The block with the ports are in pA->block and pB->block.

        DASSERT(pA->block->type == QsBlockType_simple);
        DASSERT(pB->block->type == QsBlockType_simple);

        if(pA->portType == QsPortType_input)
            ret = qsBlock_connectStreams((void *) pA->block , numA,
                    (void *) pB->block, numB);
        else
            ret = qsBlock_connectStreams((void *) pB->block, numB,
                    (void *) pA->block, numA);
    }


finish:

    CHECK(pthread_mutex_unlock(&g->mutex));
    return ret;
}


int qsGraph_connectByStrings(struct QsGraph *g,
        const char *blockA, const char *typeA, const char *nameA,
        const char *blockB, const char *typeB, const char *nameB) {

    NotWorkerThread();

    DASSERT(!blockA || blockA[0]);
    DASSERT(typeA && typeA[0]);
    DASSERT(nameA && nameA[0]);
    DASSERT(!blockB || blockB[0]);
    DASSERT(typeB && typeB[0]);
    DASSERT(nameB && nameB[0]);

    // If the graph in, g, is not set this is that case that a
    // super block is making a connection.
    struct QsGraph *gg = g;

    // Let sb be the block (super block) that is calling this, and let it
    // be 0 if the block calling this is the graph (top block).
    struct QsBlock *sb = 0;

    if(!g) {
        // There must be a super block calling this.
        sb = GetBlock(CB_DECLARE|CB_CONFIG,
                   0, QsBlockType_super);
        ASSERT(sb);
        g = sb->graph;
        DASSERT(g);
    }


    int ret = -1; // default return is -1 error.

    // g->mutex is a recursive mutex.
    CHECK(pthread_mutex_lock(&g->mutex));

    // If this is a super block calling this we prepend the
    // super block name and ':'.

    struct QsBlock *bA;
    if(blockA)
        // This is the graph making a connection.
        bA = qsGraph_getBlock(gg, blockA);
    else
        // This is a super block using its own port alias to make a
        // connection.
        bA = sb;

    if(!bA) {
        ERROR("Block \"%s\" not found", blockA);
        goto finish;
    }


    struct QsBlock *bB;
    if(blockB)
        // This is the graph making a connection.
        bB = qsGraph_getBlock(gg, blockB);
    else
        // This is a super block using its own port alias to make a
        // connection.
        bB = sb;

    if(!bB) {
        ERROR("Block \"%s\" not found", blockB);
        goto finish;
    }

    ret = qsGraph_connectByBlock(g, bA, typeA, nameA, bB, typeB, nameB);

finish:

    CHECK(pthread_mutex_unlock(&g->mutex));
    return ret;
}


// Note: super blocks cannot remove port aliases.
//
void qsGraph_removePortAlias(struct QsGraph *g, enum QsPortType type,
        const char *name) {

    DASSERT(g);
    DASSERT(name);
    DASSERT(name[0]);

    struct QsDictionary *d;

    switch(type) {
        case QsPortType_setter:
            d = g->ports.setters;
            break;
        case QsPortType_getter:
            d = g->ports.getters;
            break;
        case QsPortType_input:
            d = g->ports.inputs;
            break;
        case QsPortType_output:
            d = g->ports.outputs;
            break;
        default:
            ASSERT(0);
            break;
    }

    if(d)
        qsDictionaryRemove(d, name);
}




int qsBlock_makePortAlias(
        struct QsGraph *g,
        const char *toBlockName,
        const char *portType,
        const char *toPortName,
        const char *aliasPortName) {

    NotWorkerThread();

    struct QsGraph *gg = g;

    DASSERT(portType && portType[0]);
    DASSERT(aliasPortName && aliasPortName[0]);
    DASSERT(toBlockName && toBlockName[0]);
    DASSERT(toPortName && toPortName[0]);

    struct QsParentBlock *p;
    struct QsPortDicts *ports;

    if(!g) {

        // There must be a super block calling this.
        p = GetBlock(CB_DECLARE|CB_CONFIG,
                   0, QsBlockType_super);
        ASSERT(p);
        g = p->block.graph;
        DASSERT(g);
        ports = &((struct QsSuperBlock *) p)->module.ports;

    } else {

        DASSERT(g->parentBlock.block.type == QsBlockType_top);
        // This is a graph calling this.
        p = &g->parentBlock;
        ports = &g->ports;
    }

    struct QsDictionary *d;
    enum QsPortType type;

    switch(portType[0]) {

        case 'i':
        case 'I':
            d = ports->inputs;
            type = QsPortType_input;
            break;

        case 'o':
        case 'O':
            d = ports->outputs;
            type = QsPortType_output;
            break;

        case 's':
        case 'S':
            d = ports->setters;
            type = QsPortType_setter;
            break;

        case 'g':
        case 'G':
            d = ports->getters;
            type = QsPortType_getter;
            break;

        default:

            ERROR("Bad portType \"%s\"", portType);
            DASSERT(0);
            return -1;
    }

    DASSERT(d);


    if(qsDictionaryFind(d, aliasPortName)) {
        ERROR("Block \"%s\" alias \"%c:%s\" already exists",
                p->block.name, portType[0],
                aliasPortName);
        return -2;
    }

    struct QsBlock *block = qsGraph_getBlock(gg, toBlockName);

    if(!block) {
        ERROR("Block \"%s\" not found in block \"%s\"",
                toBlockName, p->block.name);
        return -3;
    }

    struct QsPort *port = qsDictionaryFind(GetDictionary(block, type),
            toPortName);

    if(!port) {
        errno = 0;
        ERROR("Block \"%s\" %s port \"%s\" not found in parent block \"%s\"",
                toBlockName, portType, toPortName, p->block.name);
        return -4;
    }

    ASSERT(0 == qsDictionaryInsert(d, aliasPortName, port, 0));


    return 0; 
}


bool qsGraph_canConnect(const struct QsPort *p1, const struct QsPort *p2) {

    NotWorkerThread();
    DASSERT(p1);
    DASSERT(p2);
    DASSERT(p1->block);
    struct QsGraph *g = p1->block->graph;

    CHECK(pthread_mutex_lock(&g->mutex));

    bool ret = false;

    switch(p1->portType) {

        case QsPortType_input:
            if(((struct QsInput *) p1)->output)
                // It's already connected.
                break;
            switch(p2->portType) {
                case QsPortType_input:
                case QsPortType_setter:
                case QsPortType_getter:
                    break;
                case QsPortType_output:
                    ret = true;
                    break;
                default:
                    ASSERT(0);
                    break;
            }
            break;
        case QsPortType_output:
            switch(p2->portType) {
                case QsPortType_input:
                    if(!((struct QsInput *) p2)->output)
                        // An output can connect to an input if
                        // the input is not connected.
                        ret = true;
                    break;
                case QsPortType_setter:
                case QsPortType_getter:
                case QsPortType_output:
                    break;
                default:
                    ASSERT(0);
                    break;
            }
            break;
        case QsPortType_setter:
        case QsPortType_getter:
            switch(p2->portType) {
                case QsPortType_input:
                case QsPortType_output:
                    break;
                case QsPortType_setter:
                case QsPortType_getter:
                    ret = qsParameter_canConnect((void *) p1,
                            (void *) p2, false/*verbose*/);
                    break;
                default:
                    ASSERT(0);
                    break;
            }
            break;
        default:
            ASSERT(0);
            break;
    }

    CHECK(pthread_mutex_unlock(&g->mutex));

    return ret;
}
