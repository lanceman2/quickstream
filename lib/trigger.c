#include <stdlib.h>
#include <signal.h>
#include <string.h>
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
        enum QsTriggerKind kind, void (*callback)(void *userData),
        void *userData) {

    DASSERT(b);
    DASSERT(b->block.graph);

    // This is freed in block.c.
    struct QsTrigger *t = calloc(1, size);
    ASSERT(t, "calloc(1,%zu) failed", size);
    t->block = b;
    t->kind = kind;
    t->size = size;
    // Add this to the list of triggers in the block.
    if(b->triggers) {
        DASSERT(b->triggers->prev == 0);
        b->triggers->prev = t;
        t->next = b->triggers;
    }
    b->triggers = t;

    t->callback = callback;
    t->userData = userData;

    return (void *) t;
}


// TODO: Do we want the ability to make more of these.
//
// We may have just one of these:
static
struct QsSignal *sig = 0;


static
void SigAction(int signum) {

    DASSERT(sig);
    WARN("CAUGHT signal %d", signum);

    // This is an atomic set:
    sig->triggered = 1;
}


static bool
SigCheckTrigger(void) {

    DASSERT(sig);
    return (sig->triggered)?true:false;
}


int qsTriggerSignalCreate(int signum,
        void (*callback)(void *userData),
        void *userData) {

    ASSERT(mainThread == pthread_self(), "Not graph main thread");
    ASSERT(sig == 0, "We already have a QsTriggerSignal");

    // Get the block module that is calling this function:
    struct QsBlock *b = GetBlock();
    ASSERT(b->inWhichCallback == _QS_IN_BOOTSTRAP,
            "this must be called in bootstrap()");
    ASSERT(b->isSuperBlock == false, "SuperBlocks may not call this");
    struct QsSimpleBlock *smB = (struct QsSimpleBlock *) b;

    sig = AllocateTrigger(sizeof(*sig), smB, QsSignal, callback, userData);
    sig->signum = signum;
    sig->trigger.checkTrigger = SigCheckTrigger;

    return 0; // success
}


// Called in qsGraphRun() before running the flow.
//
void TriggerStart(struct QsTrigger *t) {

    DASSERT(t);

    switch(t->kind) {

        case QsSignal: {
            struct sigaction act;
            memset(&act, 0, sizeof(act));
            act.sa_handler = SigAction;
            CHECK(sigaction(((struct QsSignal *) t)->signum, &act, 0));
        }
        break;
    }

}


void TriggerStop(struct QsTrigger *t) {

    DASSERT(t);

    switch(t->kind) {

        case QsSignal: {
            struct sigaction act;
            memset(&act, 0, sizeof(act));
            act.sa_handler = 0;
            CHECK(sigaction(((struct QsSignal *) t)->signum, &act, 0));
        }
        break;
    }
}

