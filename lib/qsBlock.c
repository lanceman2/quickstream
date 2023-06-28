#include <string.h>
#include <stddef.h>
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



const char *qsBlockGetName(void) {

    struct QsBlock *b = GetBlock(CB_ANY, QS_TYPE_MODULE, 0);
    ASSERT(b);
    // If the block is in a callback then this string access
    // is protected; but the block cannot share this pointer
    // from outside the callback function.
    return b->name;
}


bool qsIsRunning(void) {

    struct QsBlock *b = GetBlock(CB_ANY, QS_TYPE_MODULE, 0);
    ASSERT(b);
    // If the block is in a callback then this graph access
    // is protected; but the block cannot share this pointer
    // from outside the callback function.
    DASSERT(b->graph);

    return b->graph->runningStreams;
}


// Change this block and all it's children.
static inline
void ChangeBlockName(struct QsGraph *g, struct QsBlock *b,
        const char *newsurname) {

    DASSERT(g);
    DASSERT(b);
    DASSERT(b->name);
    DASSERT(b->name[0]);
    DASSERT(newsurname);
    DASSERT(newsurname[0]);

    // Examples:
    //
    // Changing oldsurname:joe:betty to newsurname:joe:betty
    //
    // Or  oldsurname to newsurname

    // len will be the total length of the new block name.
    size_t len = strlen(newsurname) + 1; // 1 for '\0'
    const char *n = b->name; // n = "oldsurname:joe:betty"
    while(*n && *n != ':') ++n;
    if(*n) {
        // n = ":joe:betty"
        len += strlen(n);
    }
    // else There was no ':'

    char *newName = calloc(1, len);
    ASSERT(newName, "calloc(1,%zu) failed", len);
    strcpy(newName, newsurname);
    if(*n)
        strcat(newName, n);
    ASSERT(0 == qsDictionaryRemove(g->blocks, b->name));
    ASSERT(0 == qsDictionaryInsert(g->blocks, newName, b, 0));

    DZMEM(b->name, strlen(b->name));
    free(b->name);
    b->name = newName;

    if(b->type & QS_TYPE_PARENT) {
        struct QsParentBlock *p = (void *) b;
        // Repurpose the b variable.
        for(b = p->firstChild; b; b = b->nextSibling)
            ChangeBlockName(g, b, newsurname);
    }
}


int qsBlock_rename(struct QsBlock *b, const char *name) {

    NotWorkerThread();
    DASSERT(b);
    ASSERT(b->name);
    ASSERT(b->name[0]);
    struct QsGraph *g = b->graph;
    DASSERT(g);

    int ret = 0;

    CHECK(pthread_mutex_lock(&g->mutex));

    // There are lots of failure cases to check for:
    //
    // Exclude special characters.
    if(!ValidNameString(name)) {
        ERROR("Block name \"%s\" is invalid", name);
        ret = -1;
        goto unlock;
    }
    if(qsDictionaryFind(g->blocks, name)) {
        ERROR("Block name \"%s\" is in use", name);
        ret = -2;
        goto unlock;
    }
    // This can only change the name of a block that is a direct
    // child of the graph (top block).
    struct QsBlock *bl = g->parentBlock.firstChild;
    for(; bl; bl = bl->nextSibling)
        if(bl == b)
            break;
    if(!bl) {
        ERROR("Block \"%s\" is not a direct descendent of a graph",
                b->name);
        ret = -3;
        goto unlock;
    }

    // We can't edit the graph while worker threads are busy:
    qsGraph_threadPoolHaltLock(g, 0);

    // Okay: Good to go:

    ChangeBlockName(g, b, name);

    qsGraph_threadPoolHaltUnlock(g);

unlock:

    CHECK(pthread_mutex_unlock(&g->mutex));


    return ret;
}
