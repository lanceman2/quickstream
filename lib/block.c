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



// We only zero the part of the memory that the qsBlock_init() and
// qsBlock_cleanup() are concerned with.  qsGraph_createBlock() and
// qsBlock_destroy() need to deal with their additional memory.
//
static inline
void ZeroSomeBlockMemory(struct QsBlock *b, enum QsBlockType type) {

    DASSERT(b);

    switch(type) {
        case QsBlockType_jobs:
        case QsBlockType_simple:
            memset(b, 0, sizeof(struct QsJobsBlock));
            break;
        case QsBlockType_super:
        case QsBlockType_parent:
            memset(b, 0, sizeof(struct QsParentBlock));
            break;
         default:
            ASSERT(0, "Block type not supported");
    }
}



// For setting up the block's in the family tree (parent/child stuff) and
// the thread pool event/job stuff.
//
// If name is set it must have been verified with the g->blocks
// dictionary.
//
void qsBlock_init(struct QsGraph *g, struct QsBlock *b,
        struct QsBlock *parent/*parentBlock*/,
        struct QsThreadPool *tp, enum QsBlockType type,
        const char *name, void *userData) {

    NotWorkerThread();
    DASSERT(b);
    DASSERT(type);
    DASSERT(g);

    // g->mutex is a recursive mutex.  If we initialize a child block in
    // this block we can get the lock before calling this.
    CHECK(pthread_mutex_lock(&g->mutex));

    ASSERT(!parent || parent->type & QS_TYPE_PARENT,
            "parent arg is not a parent block");
 
    ZeroSomeBlockMemory(b, type);

    if(!name) {
        b->name = GetUniqueName(g->blocks, 0, 0/*name*/, "b");
        DASSERT(b->name);
    } else {
        DASSERT(name[0]);
        b->name = strdup(name);
        ASSERT(b->name, "strdup() failed");
    }

    ASSERT(0 == qsDictionaryInsert(g->blocks, b->name, b, 0));
    //++g->numBlocks;

    b->type = type;
    b->graph = g;
    b->userData = userData;

    struct QsParentBlock *p = (struct QsParentBlock *) parent;
    if(!p)
        // default parent is the graph.
        p = &g->parentBlock;

    if(!tp)
        // Set the thread pool to the default thread pool.
        tp = g->threadPoolStack;


    // Add this block as the last child of parentBlock.
    b->parentBlock = p;
    //
    if(p->lastChild) {
        DASSERT(p->firstChild);
        DASSERT(!p->firstChild->prevSibling);
        DASSERT(!p->lastChild->nextSibling);
        p->lastChild->nextSibling = b;
        b->prevSibling = p->lastChild;
        p->lastChild = b;
    } else {
        DASSERT(!p->firstChild);
        p->firstChild = p->lastChild = b;
    }

    if(type & QS_TYPE_JOBS) {
        // Add this block to the thread pool.
        //
        // We know this block can't have jobs that can be queued yet,
        // because it has just been allocated.  We just need to write to
        // the thread pool data structure.
        //
        CHECK(pthread_mutex_lock(&tp->mutex));

        Block_addThreadPool((struct QsJobsBlock *)b, tp);

        DASSERT(tp->numThreads > 0);
        ((struct QsJobsBlock *)b)->threadPool = tp;
        ++tp->numJobsBlocks;
        tp->maxThreadsRun = Get_maxThreadsRun(tp);

        CHECK(pthread_mutex_unlock(&tp->mutex));
    }

    CHECK(pthread_mutex_unlock(&g->mutex));
}


// For cleaning up the stuff that _qsBlock_init() set up.
//
void qsBlock_cleanup(struct QsBlock *b) {

    NotWorkerThread();
    DASSERT(b);
    struct QsGraph *g = b->graph;
    DASSERT(g);

    // g->mutex is a recursive mutex.  It will very likely get locked many
    // many times as this function recurses on it's self.
    CHECK(pthread_mutex_lock(&g->mutex));

    struct QsParentBlock *p = b->parentBlock;
    DASSERT(p);

    DASSERT(b->name);

    // Cleanup all children
    if(b->type & QS_TYPE_PARENT) {
        struct QsParentBlock *p = (struct QsParentBlock *) b;
        while(p->lastChild) {
            switch(b->type) {
                case QsBlockType_simple:
                case QsBlockType_super:
                    // Recurse
                    qsBlock_destroy(p->lastChild);
                    break;
                default:
                    // Recurse
                    qsBlock_cleanup(p->lastChild);
            }
        }
    }

    // All my children are cleaned-up now.


    if(b->type & QS_TYPE_JOBS) {

        struct QsJobsBlock *jb = (struct QsJobsBlock *) b;
        struct QsThreadPool *tp = jb->threadPool;
        DASSERT(tp);

        uint32_t numHalts = qsBlock_threadPoolHaltLock(jb);


        // Cleanup all jobs owned by this block, in reverse order that
        // they were created.
        while(jb->jobsStack)
            qsJob_cleanup(jb->jobsStack);

        DASSERT(tp->numJobsBlocks);
        DASSERT(tp->numJobsBlocks != -1);

        --tp->numJobsBlocks;
        Block_removeThreadPool(jb, jb->threadPool);

        // The block threadPools list should be empty now.
        DASSERT(!jb->threadPools.root);

        uint32_t maxThreadsRun = Get_maxThreadsRun(jb->threadPool);
        if(tp->numThreads > maxThreadsRun) {
            CHECK(pthread_mutex_lock(&jb->threadPool->mutex));
            JoinThreads(jb->threadPool, maxThreadsRun);
            CHECK(pthread_mutex_unlock(&jb->threadPool->mutex));
        } else
            jb->threadPool->maxThreadsRun = maxThreadsRun;

        while(numHalts--)
            qsGraph_threadPoolHaltUnlock(g);
    }


    // Remove this block, b, from the block family tree.
    DASSERT(p->lastChild);
    DASSERT(p->firstChild);

    if(b->nextSibling) {
        DASSERT(p->lastChild != b);
        b->nextSibling->prevSibling = b->prevSibling;
    } else {
        DASSERT(p->lastChild == b);
        p->lastChild = b->prevSibling;
    }

    if(b->prevSibling) {
        DASSERT(p->firstChild != b);
        b->prevSibling->nextSibling = b->nextSibling;
        b->prevSibling = 0;
    } else {
        DASSERT(p->firstChild == b);
        p->firstChild = b->nextSibling;
    }
    b->nextSibling = 0;

    //--g->numBlocks;
    ASSERT(0 == qsDictionaryRemove(g->blocks, b->name));

    DZMEM(b->name, strlen(b->name));
    free(b->name);

#ifdef DEBUG
    switch(b->type) {
        case QsBlockType_jobs:
        case QsBlockType_simple:
            DZMEM(b, sizeof(struct QsJobsBlock));
            break;
        case QsBlockType_super:
        case QsBlockType_parent:
            DZMEM(b, sizeof(struct QsParentBlock));
            break;
         default:
            ASSERT(0, "Block type not supported");
    }
#endif

    CHECK(pthread_mutex_unlock(&g->mutex));
}
