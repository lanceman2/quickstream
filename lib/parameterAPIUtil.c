#include <stdint.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <pthread.h>

#include "../include/quickstream/block.h"
#include "../include/quickstream/builder.h"
#include "../include/quickstream/app.h"

#include "debug.h"
#include "Dictionary.h"
#include "block.h"
#include "threadPool.h"
#include "parameter.h"
#include "graph.h"
#include "trigger.h"
#include "GET_BLOCK.h"



void qsParameterGetValue(struct QsParameter *p, void *value) {

    DASSERT(p);
    DASSERT(value);

    switch(p->kind) {

        case QsSetter:
        {
            struct QsSetter *s = (struct QsSetter *) p;

            if(p->block->block.graph->flowState == QsGraphFlowing) {
                DASSERT(s->mutex);
                CHECK(pthread_mutex_lock(s->mutex));
                memcpy(value, p->value, p->size);
                CHECK(pthread_mutex_unlock(s->mutex));
                return;
            }
            ASSERT(mainThread == pthread_self(), "Not graph main thread");
            memcpy(value, p->value, p->size);
            return;
        }
        case QsConstant:
            ASSERT(mainThread == pthread_self(), "Not graph main thread");
            memcpy(value, p->value, p->size);
            break;
        case QsGetter:
            memcpy(value, p->value, p->size);
            break;
        case QsAny:
            ASSERT(0);
            break;
    };
}


// Called when graph is paused.  Parameter, p, must be a constant or a
// getter.
//
int qsParameterSetValue(struct QsParameter *p, const void *value) {

    DASSERT(p);
    DASSERT(value);

    // Catch API user coding errors.
    ASSERT(mainThread == pthread_self(), "Not graph main thread");
    ASSERT(p->block->block.graph->flowState == QsGraphPaused);


    // Called when graph is paused.  Parameter, p, must be a constant or a
    // getter, or a disconnected setter.
    DASSERT(p->kind != QsSetter, "Setter parameters cannot be directly"
            " set, they must be connected");

    if(p->kind == QsSetter) {
        ERROR("parameter %s:%s is setter", p->block->block.name, p->name);
        return -1; // error
    }

    // Constant parameters keep an internal value.
    // Getter parameter just push values to setters but can be initialized
    // here.  Setters can be initialized if they are not connected yet.

    memcpy(p->value, value, p->size);

    if(p->kind == QsConstant)
        for(struct QsParameter *i = p->first; i; i = i->next) {
            int (*setCallback)(struct QsParameter *p,
                const void *value, void *userData);
            void *userData;
            if(i->kind == QsConstant) {
                setCallback = ((struct QsConstant *)i)->setCallback;
                userData = ((struct QsConstant *)i)->userData;
            } else {
                DASSERT(i->kind == QsSetter);
                setCallback = ((struct QsSetter *)i)->setCallback;
                userData = ((struct QsSetter *)i)->userData;
            }
            DASSERT(i->value == p->value);
            DASSERT(pthread_getspecific(_qsGraphKey) == 0);
            if(!setCallback) continue;

            CHECK(pthread_setspecific(_qsGraphKey, &i->block->block));
            setCallback(i, i->value, userData);
            CHECK(pthread_setspecific(_qsGraphKey, 0));
        }

    return 0;
}


void QueueUpSetterFromGetter(struct QsSetter *s, struct QsParameter *p) {

    DASSERT(s);
    DASSERT(s->parameter.first == p);
    DASSERT(s->parameter.first->kind == QsGetter);
    DASSERT(s->parameter.type == p->type);
    DASSERT(s->parameter.size == p->size);
    DASSERT(s->mutex);

    CHECK(pthread_mutex_lock(s->mutex));

    struct QsTrigger *trigger = s->trigger;
    DASSERT(trigger);

    if(!s->haveValueQueued) {
        // queue up this event.
        //
        // We need an inner mutex lock.  Kind of scary.
        CHECK(pthread_mutex_lock(&
                ((struct QsBlock *)p->block)->graph->mutex));

        if(!trigger || !trigger->isRunning) {
            // The trigger is off for one of two reasons:
            //   1. We let triggers be freed at flow time.
            //   2. We also let triggers be stopped at flow time.
            //
            // We argue that this wasted mutexing is better than not
            // having this feature.  Dummying this code here is more
            // performant then letting the callback dummy it's self.
            //
            // Unlock mutexes and continue looping to the next setter.
            CHECK(pthread_mutex_unlock(&
                    ((struct QsBlock *)p->block)->graph->mutex));
            CHECK(pthread_mutex_unlock(s->mutex));
            return;
        }

        // Queue a job for this trigger.
        CheckAndQueueTrigger(trigger, trigger->block->threadPool);
        CHECK(pthread_mutex_unlock(&
                    ((struct QsBlock *)p->block)->graph->mutex));
        s->haveValueQueued = true;
    }

    memcpy(s->parameter.value, p->value, p->size);

    CHECK(pthread_mutex_unlock(s->mutex)); 
}


// Called at flow-time.
//
uint32_t qsParameterGetterPush(struct QsParameter *p,
        const void *value) {

    DASSERT(p);
    DASSERT(p->block);
    DASSERT(p->kind == QsGetter);
    DASSERT(value);
    DASSERT(p->block->block.graph);
    ASSERT(p->block->block.graph->flowState == QsGraphFlowing);

    // We assume that the block that is calling this code is doing this in
    // one thread at a time.
    memcpy(p->value, value, p->size);

    if(p->numConnections < 2) {
        // We should never have 1 connection, because the list of
        // connections included this parameter as the first one.
        DASSERT(p->numConnections == 0);
        DASSERT(p->first == 0);
        return 0;
    }

    DASSERT(p->first == p);

    // Getters only connect to 0 to N setters.

    struct QsParameter *s = p->first->next;

    for(; s; s = s->next)
        QueueUpSetterFromGetter((struct QsSetter *) s, p);

    return p->numConnections - 1;
}



static
int PrintParameterCB(const char *pname, struct QsParameter *p,
            FILE *file) {

    switch(p->kind) {
        case QsConstant:
            fprintf(file, "Constant ");
            break;
        case QsGetter:
            fprintf(file, "Getter   ");
            break;
        case QsSetter:
            fprintf(file, "Setter   ");
            break;
        case QsAny:
            ASSERT(0);
            break;
    }

    switch(p->type) {
        case QsNone:
            fprintf(file, "QsNone   %4zu ", p->size);
            break;
        case QsDouble:
            fprintf(file, "QsDouble %4zu ", p->size);
            break;
        case QsUint64:
            fprintf(file, "QsUint64 %4zu ", p->size);
            break;
        case QsString:
            fprintf(file, "QsString %4zu ", p->size);
            break;
    }

    fprintf(file, "%s %s", ((struct QsBlock *)p->block)->name, pname);

    uint32_t n;


    switch(p->type) {
        case QsNone:
            break;
        case QsDouble:
            n = p->size/sizeof(double);
            for(uint32_t i=0; i<n; ++i)
                printf(" %16.16g ", ((double *)p->value)[i]);
            break;
        case QsUint64:
            n = p->size/sizeof(uint64_t);
            for(uint32_t i=0; i<n; ++i)
                printf(" %.10" PRIu64 " ", ((uint64_t *)p->value)[i]);
            break;
        case QsString:
            fprintf(file, " \"%*.*s\"", (int) p->size, (int) p->size,
                    (char *) p->value);
            break;
    }

    fprintf(file, "\n");

    return 0; // keep going.
}


void qsGraphParametersPrint(struct QsGraph *graph, FILE *file) {

    DASSERT(graph);
    DASSERT(file);

    fprintf(file, " kind    type   size block  parameter_name  values\n"
                  "------   ------ ---- ------ --------------\n");

    for(struct QsBlock *b = graph->firstBlock; b; b = b->next) {
        if(b->isSuperBlock) continue;
        qsDictionaryForEach(((struct QsSimpleBlock *)b)->constants,
                (int (*) (const char *key, void *value,
                    void *userData)) PrintParameterCB, file);
        qsDictionaryForEach(((struct QsSimpleBlock *)b)->getters,
                (int (*) (const char *key, void *value,
                    void *userData)) PrintParameterCB, file);
        qsDictionaryForEach(((struct QsSimpleBlock *)b)->setters,
                (int (*) (const char *key, void *value,
                    void *userData)) PrintParameterCB, file);
    }
}


const char *qsParameterGetName(const struct QsParameter *p) {
    DASSERT(p);
    return p->name;
}


size_t qsParameterGetSize(const struct QsParameter *p) {
    DASSERT(p);
    return p->size;
}


enum QsParameterType qsParameterGetType(const struct QsParameter *p) {
    DASSERT(p);
    return p->type;
}


enum QsParameterKind qsParameterGetKind(const struct QsParameter *p) {
    DASSERT(p);
    return p->kind;
}
