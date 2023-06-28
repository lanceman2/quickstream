#define _GNU_SOURCE
#include <link.h>

#include <inttypes.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
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


///////////////////////////////////////////////////////////////////
//  qsGetMemory() is a simple inter-thread communication method.
//
// Works for just quickstream thread pool worker threads.
// The blocks must know the name/key to share this memory
// between them.
//
///////////////////////////////////////////////////////////////////

// We used one mutex to protect all the graphs memDict dictionaries.
// Thread pool workers thread cannot use the graph mutex (QsGraph::mutex).
//
// That's okay the creation of the inter-thread shared memory thingys
// does not happen often.
//
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;



struct Mem {

    pthread_mutex_t mutex;

    // The size of userData.
    size_t size;
    void *userData;
};



static inline struct QsGraph *GetGraph(void) {

    struct QsSimpleBlock *b = GetBlock(CB_ANY, 0, QsBlockType_simple);
    ASSERT(b, "Not a simple block calling this");
    DASSERT(b->jobsBlock.block.graph);
    return b->jobsBlock.block.graph;
}



static
int FreeMem(const char *name, struct Mem *mem, void *userData) {

    DASSERT(name);
    DASSERT(mem);
    DASSERT(mem->size);
    DASSERT(mem->userData);

    CHECK(pthread_mutex_destroy(&mem->mutex));

#ifdef DEBUG
    memset(mem->userData, 0, mem->size);
#endif
    free(mem->userData);
#ifdef DEBUG
    memset(mem, 0, sizeof(*mem));
#endif
    free(mem);

    return 0;
}


// CleanupQsGetMemory() is called in library destructor in
// lib_constructor.c.
//
void CleanupQsGetMemory(struct QsGraph *g) {

    CHECK(pthread_mutex_lock(&mutex));

    if(g->memDict) {
        if(!qsDictionaryIsEmpty(g->memDict))
            ASSERT(qsDictionaryForEach(g->memDict,
                    (int (*) (const char *, void *, void *))
                     FreeMem, 0) >= 1);
        qsDictionaryDestroy(g->memDict);
        g->memDict = 0;
    }

    CHECK(pthread_mutex_unlock(&mutex));
}



// The caller must make sure only one thread calls this once per name.
// The mutex in the struct Mem will not help insure that and can't.  It's
// just not a good design to do that.  It would increase complexity by a
// lot.  This is as much complexity that we were willing to bare.
//
void qsFreeMemory(const char *name) {

    struct QsGraph *g = GetGraph();

    DASSERT(name);
    DASSERT(name[0]);

    CHECK(pthread_mutex_lock(&mutex));
    
    struct Mem *mem = qsDictionaryFind(g->memDict, name);
    ASSERT(mem, "Memory named \"%s\" not found", name);
    ASSERT(qsDictionaryRemove(g->memDict, name) == 0);
    FreeMem(name, mem, 0);

    CHECK(pthread_mutex_unlock(&mutex));
}


// Return process wide memory that is shared between threads
// that call this.
void *qsGetMemory(const char *name, size_t size,
        pthread_mutex_t **rmutex, bool *firstCaller,
        uint32_t flags) {

    struct QsGraph *g = GetGraph();
    DASSERT(g);
    DASSERT(size);
    DASSERT(firstCaller);
    DASSERT(name);
    DASSERT(name[0]);
    DASSERT(rmutex);

    struct Mem *mem = 0;

    CHECK(pthread_mutex_lock(&mutex));

    if(g->memDict)
        mem = qsDictionaryFind(g->memDict, name);
    else {
        g->memDict = qsDictionaryCreate();
        DASSERT(g->memDict);
    }

    if(!mem) {
        // I'm a dumb-ass, how can I do this in one allocation without
        // making assumptions about compiler structure padding?  I could
        // just add a full word pad between the two, would be, allocations
        // and make one allocation, but wouldn't that be about the same
        // overhead as two allocations, especially given the writers of
        // the libc memory allocator are much smarter than me. 
        //
        mem = calloc(1, sizeof(*mem));
        ASSERT(mem, "calloc(1,%zu) failed", size);
        mem->userData = calloc(1, size);
        ASSERT(mem->userData, "calloc(1,%zu) failed", size);
        mem->size = size;
        ASSERT(0 == qsDictionaryInsert(g->memDict, name, mem, 0));
        if(flags & QS_GETMEMORY_RECURSIVE) {
            pthread_mutexattr_t at;
            CHECK(pthread_mutexattr_init(&at));
            CHECK(pthread_mutexattr_settype(&at,
                    PTHREAD_MUTEX_RECURSIVE_NP));
            CHECK(pthread_mutex_init(&mem->mutex, &at));
        } else
            CHECK(pthread_mutex_init(&mem->mutex, 0));

        // This creator thread will have this lock after this function
        // returns.  Any other thread that calls this function with this
        // key (name) will not get this locked and will not have
        // *firstCaller set; or at least until this memory is freed with
        // qsFreeMemory().
        //
        CHECK(pthread_mutex_lock(&mem->mutex));

        *firstCaller = true;
    } else
        *firstCaller = false;


    *rmutex = &mem->mutex;

    CHECK(pthread_mutex_unlock(&mutex));

    // Dumb-ass simple and short.  ;)

    return mem->userData;
}
