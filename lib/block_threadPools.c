#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
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


// Managing a list of thread pools for a jobs block.  The block's thread
// pool list is a red black tree using the c-rbtree API.


void Block_addThreadPool(struct QsJobsBlock *b,
        struct QsThreadPool *tp) {

    // b->threadPools is a CRBTree 
    CRBNode **i = &b->threadPools.root;
    CRBNode *parent = 0;

    while(*i) {

        parent = *i;

        struct QsThreadPoolNode *n = c_rbnode_entry(*i,
                struct QsThreadPoolNode, node);
        if(n->threadPool < tp)
            i = &parent->left;
        else if(n->threadPool > tp)
            i = &parent->right;
        else {
            // Now we add it again by counting.
            ++n->refCount;
#if 0
    DSPEW("            Adding to block \"%s\" \"%s\" refCount=%" PRIu32,
                b->block.name, tp->name, n->refCount);
#endif
            return;
        }
    }

    struct QsThreadPoolNode *n = calloc(1, sizeof(*n));
    ASSERT(n, "calloc(1,%zu) failed", sizeof(*n));
    n->threadPool = tp;
    n->refCount = 1;

#if 0
    DSPEW("            Adding to block \"%s\" \"%s\" refCount=%" PRIu32,
                b->block.name, tp->name, n->refCount);
#endif

    c_rbtree_add(&b->threadPools, parent, i, &n->node);
}


static
uint32_t  ForeachThreadPool(CRBNode *node,
        void (*callback)(struct QsThreadPool *tp)) {

    struct QsThreadPoolNode *n = c_rbnode_entry(node,
            struct QsThreadPoolNode, node);
    DASSERT(n);
    DASSERT(n->threadPool);

    uint32_t ret = 1;

    callback(n->threadPool);

    if(n->node.left)
        ret += ForeachThreadPool(n->node.left, callback);

    if(n->node.right)
        ret += ForeachThreadPool(n->node.right, callback);

    return ret;
}


static
uint32_t Block_foreachThreadPool(struct QsJobsBlock *b,
        void (*callback)(struct QsThreadPool *tp)) {

    if(b->threadPools.root)
        return ForeachThreadPool(b->threadPools.root, callback);
    return 0;
}


static
void HaltThreadPool(struct QsThreadPool *tp) {

    DASSERT(tp);
    DASSERT(tp->graph);
    qsGraph_threadPoolHaltLock(tp->graph, tp);
}


// This figures out which thread pools to lock for the jobs block.
//
// Returns the number of times that it called
// qsGraph_threadPoolHaltLock().
//
uint32_t qsBlock_threadPoolHaltLock(struct QsJobsBlock *b) {

    DASSERT(b);
    DASSERT(b->threadPools.root);

    return Block_foreachThreadPool(b,  HaltThreadPool);
}


void Block_removeThreadPool(struct QsJobsBlock *b,
        struct QsThreadPool *tp) {

    CRBNode *i = b->threadPools.root;
    struct QsThreadPoolNode *n;

    while(i) {

        n = c_rbnode_entry(i, struct QsThreadPoolNode, node);

        struct QsThreadPool *p = n->threadPool;
        if(p < tp)
            i = i->left;
        else if(p > tp)
            i = i->right;
        else
            break;
    }
    if(!i) {
        ASSERT(0, "Block \"%s\" listed thread pool \"%s\" was not found",
                b->block.name, tp->name);
        // This thread pool, tp, is not in this list.
        return;
    }

#if 0
    DSPEW("          Removing from block \"%s\" \"%s\" refCount=%" PRIu32,
                b->block.name, tp->name, n->refCount-1);
#endif

    if(--n->refCount)
        // We still have as least one reference count.
        return;

    c_rbnode_unlink(&n->node);

    DZMEM(n, sizeof(*n));
    free(n);
}
