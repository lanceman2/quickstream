#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>

#include "../include/quickstream/block.h"
#include "../include/quickstream/builder.h"



#include "parameter.h"
#include "trigger.h"
#include "debug.h"
#include "block.h"
#include "graph.h"


static
void *AllocateTrigger(size_t size, struct QsSimpleBlock *b,
        enum QsTriggerKind kind) {

    DASSERT(b);
    DASSERT(b->block.graph);

    struct QsTrigger *t = calloc(1, size);
    ASSERT(t, "calloc(1,%zu) failed", size);
    t->block = b;
    t->kind = kind;
    t->size = size;
    // Add this to the list of triggers in the block.
    t->next = b->triggers;
    b->triggers = t;
    return (void *) t;
}


int qsTriggerSignalCreate(int signum,
        void (*triggerCallback)(void *userData),
        void *userData) {

    ASSERT(mainThread == pthread_self(), "Not graph main thread");
    // Get the block module that is calling this function:
    struct QsBlock *b = GetBlock();
    ASSERT(b->inWhichCallback == _QS_IN_BOOTSTRAP,
            "this must be called in bootstrap()");
    ASSERT(b->isSuperBlock == false, "SuperBlocks may not call this");
    struct QsSimpleBlock *smB = (struct QsSimpleBlock *) b;

    struct QsSignal *sig = AllocateTrigger(sizeof(*sig), smB, QsSignal);
    sig->triggerCallback = triggerCallback;
    sig->userData = userData;
    sig->signum = signum;
    


    return 0; // success
}

