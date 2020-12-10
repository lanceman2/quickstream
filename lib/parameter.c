#include <stdint.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <pthread.h>

#include "../include/quickstream/block.h"
#include "../include/quickstream/builder.h"


#include "debug.h"
#include "Dictionary.h"
#include "parameter.h"
#include "block.h"
#include "graph.h"
#include "trigger.h"



// A terrible CPP macro that sets b to the current block if it is not set
// already.  Also checks that we are in a block bootstrap() call.
// Also checks that this is the main thread that creates graphs.
// Also checks that b is a simple block.
//
// Note: blocks can create parameters for other blocks.  That's why
// this macro is important.
//
// Maybe this beats repeating this code many times in this file.
//
// There are 2 cases:
//   1. the creator and owner block are different blocks
//      and they must both be from the same graph.
//   2. the creator and owner block are the same block
//
//   In either case we return the owner block.
//
// The parameter owner block must be a simple block.
//
//
#define GET_OWNER_BLOCK(b) \
    do {\
        ASSERT(pname && pname[0]);\
        ASSERT(mainThread == pthread_self(), "Not graph main thread");\
        if(b) {\
            struct QsBlock *creatorB = GetBlock();\
            ASSERT(creatorB->inWhichCallback == _QS_IN_BOOTSTRAP,\
                "%s() must be called from bootstrap()", __func__);\
            ASSERT(creatorB == b || creatorB->graph == b->graph);\
        } else {\
            b = GetBlock();\
            ASSERT(b->inWhichCallback == _QS_IN_BOOTSTRAP,\
                "%s() must be called from bootstrap()", __func__);\
        }\
        ASSERT(b->isSuperBlock == false);\
    } while(0)


static
void FreeParameter(struct QsParameter *p) {

    DASSERT(p);
    DASSERT(p->name);

    if(p->kind == QsConstant) {

        struct QsConstant *c = (struct QsConstant *) p;

        if(c->connections) {
            DASSERT(c->numConnections);
#ifdef DEBUG
            memset(c->connections, 0,
                    sizeof(*c->connections)*c->numConnections);
#endif
            free(c->connections);
            c->numConnections = 0;
        }


        DASSERT(c->value);
#ifdef DEBUG
        memset(c->value, 0, p->size);
#endif
        free(c->value);

    } else if(p->kind == QsGetter) {

        struct QsGetter *g = (struct QsGetter *) p;

        if(g->value) {
            // The getter value is not set unless qsParameterSet() was
            // called for it.
#ifdef DEBUG
            memset(g->value, 0, p->size);
#endif
            free(g->value);
        }

        if(g->setters) {
            DASSERT(g->numSetters);
#ifdef DEBUG
            memset(g->setters, 0,
                    sizeof(*g->setters)*g->numSetters);
#endif
            free(g->setters);
            g->numSetters = 0;
        }
    } else {

        DASSERT(p->kind == QsSetter);
        struct QsSetter *s = (struct QsSetter *) p;
#ifdef DEBUG
        memset(s->value, 0, p->size);
#endif
        free(s->value);
    }

#ifdef DEBUG
    memset((char *) p->name, 0, strlen(p->name));
#endif
    free((char *) p->name);


#ifdef DEBUG
    switch(p->kind) {
        case QsConstant:
            memset(p, 0, sizeof(struct QsConstant));
            break;
        case QsGetter:
            memset(p, 0, sizeof(struct QsGetter));
            break;
        case QsSetter:
            memset(p, 0, sizeof(struct QsSetter));
            break;
    }
#endif
    free(p);
}


static inline
void *AllocateParameter(const char *parameterKind,
        struct QsSimpleBlock *b,
        struct QsDictionary *pdict,
        size_t psize, enum QsParameterType type,
        const char *bname,
        const char *pname,
        enum QsParameterKind kind, size_t structSize) {

    DASSERT(pdict);
    DASSERT(psize);
    DASSERT(structSize > sizeof(struct QsParameter));
    DASSERT(((struct QsBlock *)b)->isSuperBlock == false);

    // Check that parameter name is not already present in blocks
    // list of this kind of parameter.
    if(qsDictionaryFind(pdict, pname)) {
        ERROR("%s parameter named \"%s\" is already in block \"%s\"",
                parameterKind, pname, bname);
        return 0; // fail
    }
    // A constant parameter also can't have the same name as a setter and
    // the setters are not in the same dictionary as the constants, so we
    // need another name check for 2 cases:
    //
    // 1. This is a setter and we can't have a constant with that name:
    // 2. This is a constant and we can't have a setter with that name:
    //
    // otherwise parameter connection name space rules do not work
    // unambiguously.  It's because getter and constants act the same as
    // far as connecting to setters, but constants can also connect to
    // other constants, but getters can't connect to constants.
    //
    if(kind == QsSetter) {
        // 1. This is a setter and we can't have a constant with that name:
        struct QsParameter *p =
            // block->getters contains getters and constants.
            qsDictionaryFind(b->getters, pname);
        if(p && p->kind == QsConstant) {
            ERROR("Constant parameter named \"%s\" is already"
                    " in block \"%s\"; setter and constant "
                    "parameter names cannot be the same",
                pname, bname);
            return 0; // fail
        }
    } else if(kind == QsConstant) {
        // 2. This is a constant and we can't have a setter with that name:
        struct QsParameter *p =
            qsDictionaryFind(b->setters, pname);
        if(p) {
            DASSERT(p->kind == QsSetter);
            ERROR("Setter parameter named \"%s\" is already"
                    " in block \"%s\"; setter and constant "
                    "parameter names cannot be the same",
                pname, bname);
            return 0; // fail
        }
    }

    struct QsParameter *p = calloc(1, structSize);
    ASSERT(p, "calloc(1,%zu) failed", structSize);
    p->block = b;
    p->kind = kind;
    p->name = strdup(pname);
    p->size = psize;
    p->type = type;

    struct QsDictionary *entry = 0;
    ASSERT(qsDictionaryInsert(pdict, pname, p, &entry) == 0);
    DASSERT(entry);
    qsDictionarySetFreeValueOnDestroy(entry,
            (void (*)(void *)) FreeParameter);

    return p;
}


struct QsParameter *
qsParameterSetterCreate(struct QsBlock *b, const char *pname,
        enum QsParameterType type, size_t psize,
        int (*setCallback)(struct QsParameter *p,
            void *value, size_t size, void *userData),
        void (*cleanup)(struct QsParameter *, void *userData),
        void *userData, uint32_t flags) {

    GET_OWNER_BLOCK(b);
    struct QsSimpleBlock *smB = (struct QsSimpleBlock *) b;

    struct QsSetter *p = AllocateParameter("Setter",
        smB,
        smB->setters, psize, type, b->name, pname,
        QsSetter, sizeof(*p));
    if(!p) return 0;
    p->userData = userData;
    p->setCallback = setCallback;
    p->mutex = &smB->mutex;
    p->value = calloc(1, psize);
    ASSERT(p->value, "calloc(1,%zu) failed", psize);

    return (struct QsParameter *) p;
}


struct QsParameter *
qsParameterGetterCreate(struct QsBlock *b, const char *pname,
        enum QsParameterType type, size_t psize) {

    GET_OWNER_BLOCK(b);
    struct QsSimpleBlock *smB = (struct QsSimpleBlock *) b;

    struct QsGetter *p = AllocateParameter("Getter",
        smB,
        smB->getters, psize, type, b->name, pname,
        QsGetter, sizeof(*p));
    if(!p) return 0;

    return (struct QsParameter *) p;
}


struct QsParameter *
qsParameterConstantCreate(struct QsBlock *b, const char *pname,
        enum QsParameterType type, size_t psize, void *initialVal) {

    GET_OWNER_BLOCK(b);
    struct QsSimpleBlock *smB = (struct QsSimpleBlock *) b;

    struct QsConstant *p = AllocateParameter("Constant",
        smB,
        smB->getters, psize, type, b->name, pname,
        QsConstant, sizeof(*p));
    if(!p) return 0;

    // p->value will be free()ed in FreeParameter().
    p->value = malloc(psize);
    ASSERT(p->value, "malloc(%zu) failed", psize);

    if(initialVal)
        memcpy(p->value, initialVal, psize);

    return (struct QsParameter *) p;
}


uint32_t qsParameterNumConnections(struct QsParameter *p) {

    DASSERT(p);
    // This can only be called in start or stop, and TODO maybe in the
    // stream runner code.
    DASSERT(p->block);
    ASSERT(((struct QsBlock *)p->block)->inWhichCallback == _QS_IN_START ||
            ((struct QsBlock *)p->block)->inWhichCallback == _QS_IN_STOP); 

    if(p->kind == QsSetter)
        // A setter parameter may only have one connection to it.
        return (((struct QsSetter *)p)->feeder)?(1):0;

    if(p->kind == QsConstant)
        return ((struct QsConstant *) p)->numConnections;

    // this must be a getter.
    DASSERT(p->kind == QsGetter);

    return ((struct QsGetter *) p)->numSetters;
}


struct QsParameter *qsParameterGetPointer(struct QsBlock *block,
        const char *pname, bool isSetter) {

    DASSERT(block);
    DASSERT(!block->isSuperBlock);
    
    if(block->isSuperBlock) return 0;

    struct QsSimpleBlock *smB = (struct QsSimpleBlock *) block;

    DASSERT(smB->setters);
    DASSERT(smB->getters);
 
    struct QsDictionary *d;
    if(isSetter)
        d = smB->setters;
    else
        d = smB->getters;

    return (struct QsParameter *) qsDictionaryFind(d, pname);
}


enum QsParameterKind qsParameterKind(struct QsParameter *p) {

    DASSERT(p);
    return p->kind;
}


static inline
void AddToConstantConnections(struct QsConstant *c,
        struct QsParameter *p) {

    DASSERT(c);
    DASSERT(p);

    uint32_t num = c->numConnections + 1;
    c->connections = realloc(c->connections, num*sizeof(*c->connections));
    ASSERT(c->connections, "realloc(%p, %zu) failed",
            c->connections, num*sizeof(*c->connections));
    c->connections[c->numConnections] = p;
    ++c->numConnections;
}


static inline
void AddToGetterConnections(struct QsGetter *g,
        struct QsSetter *s) {

    DASSERT(g);
    DASSERT(s);

    uint32_t num = g->numSetters + 1;
    g->setters = realloc(g->setters, num*sizeof(*g->setters));
    ASSERT(g->setters, "realloc(%p, %zu) failed",
            g->setters, num*sizeof(*g->setters));
    g->setters[g->numSetters] = s;
    ++g->numSetters;
}


// Is called when not holding a mutex lock.
//
// This ends up being a trigger callback called in run.c in
// CallTriggerCallback().  The thread calling this is from the block that
// owns this Setter parameter.
//
static
int SetterTriggerCB(struct QsSetter *s) {

    // TODO: We decided to shorten the time that the block mutex lock is
    // held and call the setter callback() from outside the mutex lock
    // using a stack memory copy of the value.  We thought that since we
    // do not know how long the users callback will take to be called,
    // this is best.  Clearly this is nothing like how stream data is
    // passed between blocks.  Stream data passing uses larger lock-less
    // inter-thread shared circular buffers.

    // Allocate a value buffer on the stack.  It's assumed to be small
    // like the size of a pointer, or a little larger.  There is no limit,
    // but performance is something to consider.
    uint8_t value[s->parameter.size];

    // This needs to be quick: lock, copy, and unlock.
    CHECK(pthread_mutex_lock(s->mutex));

    // Note: before this lock was set we do not know how many times the
    // value has changed.  This currently queues/saves the last value set.
    s->haveValueQueued = false;

    // copy the data to the value on to the stack memory in value[].
    memcpy(value, s->value, s->parameter.size);
    

    CHECK(pthread_mutex_unlock(s->mutex));

    // Now we do not know how long this will take.  The users setCallback
    // may change the contents of value, but the value memory will only be
    // valid while the function is being called.  They may copy the value
    // yet again.  Note: again, nothing like stream data...
    //
    return s->setCallback(&s->parameter, value,
            s->parameter.size, s->userData);
}


// TODO: We should consider ways to engineer out all the wrong modes in
// the input parameters of this function, instead of adding all these
// stupid failure if() checks in this function.
//
// Connect from p0  to p1
//
int qsParameterConnect(struct QsParameter *p0,
        struct QsParameter *p1) {

    ASSERT(mainThread == pthread_self(), "Not graph main thread");
    DASSERT(p0);
    DASSERT(p1);
    DASSERT(p0 != p1);
    DASSERT(p0->block != p1->block);
    DASSERT(p0->name);
    DASSERT(p1->name);


    // We can connect certain kinds of parameters from and to each other.
    // Here is the complete list of possible kinds of connections:
    //
    //   1. Getter    to  Setter
    //   2. Constant  to  Setter
    //   3. Constant  to  Constant
    //
    if( !(
            (p0->kind == QsGetter   && p1->kind == QsSetter) ||
            (p0->kind == QsConstant && p1->kind == QsSetter) ||
            (p0->kind == QsConstant && p1->kind == QsConstant)
         )
    ) {
        ERROR("Wrong mix of kinds of parameter to connect from and to");
        DASSERT(0);
        return -1;
    }

    if(p0 == p1) {
        ERROR("Parameter \"%s\" cannot connect to itself", p1->name);
        DASSERT(0);
        return -1;
    }

    if(p0->block == p1->block) {
        ERROR("Block \"%s\" cannot connect parameters in itself",
                ((struct QsBlock *)p0->block)->name);
        DASSERT(0);
        return -1;
    }

    // The parameters size and type must match.
    if(p0->type != p1->type || p0->size != p1->size) {
        ERROR("Parameter \"%s:%s\" is size %zu and type %d; and "
                "parameter \"%s:%s\" is size %zu and type %d",
                ((struct QsBlock *)p0->block)->name,
                p0->name, p0->size, p0->type,
                ((struct QsBlock *)p1->block)->name,
                p1->name, p1->size, p1->type);
        return -1;
    }


    if(p1->kind == QsSetter) {
        // We are connecting to a setter.
        // // A setter cannot be connected to twice.
        if(((struct QsSetter *)p1)->feeder) {
            ERROR("Setter \"%s:%s\" already has a connection",
                    ((struct QsBlock *)p1->block)->name, p1->name);
            // User is a dumb shit.
            DASSERT(0);
            return 1;
        }
        struct QsSetter *setter = (struct QsSetter *)p1;
        setter->feeder = p0;
        if(p0->kind == QsConstant) {
            // Constants need their values copied any time they
            // are connected.
            //
            // We do not need a mutex at this time since this is happening
            // before the flow starts and there can only be one thread.
            memcpy(setter->value,
                    ((struct QsConstant *)p0)->value, p0->size);
        } else {
            // We are connecting from getter to setter.
            AllocateTrigger(sizeof(*setter->trigger),
                    p1->block,
                    QsSetterT, (int (*)(void *)) SetterTriggerCB,
                    setter/*userData*/, (bool (*)(void *)) 0,
                    false/*isSource*/);
            // Triggers are managed by the block so we do not need the
            // pointer at this time.  The block will have it.
            //
            // setter->trigger gets set later at TriggerStart().
            // We use setter->trigger as a flag to know that the
            // setter is triggering or not.
        }
    } else {
        // p0 and p1 are a constant parameters
        DASSERT(p1->kind == QsConstant);
        DASSERT(p0->kind == QsConstant);
        memcpy(((struct QsConstant *)p1)->value,
                ((struct QsConstant *)p0)->value, p0->size);
    }

    if(p0->kind == QsConstant)
        AddToConstantConnections((struct QsConstant *) p0, p1);

    if(p1->kind == QsConstant)
        AddToConstantConnections((struct QsConstant *) p1, p0);

    if(p0->kind == QsGetter) {
        DASSERT(p1->kind == QsSetter);
        AddToGetterConnections((struct QsGetter *) p0,
                (struct QsSetter *) p1);
    }

    // TODO: push values across connections, if we can.

    return 0;
}


// These are the only possible connections pairs.  Though very limited,
// can effectively make very complex parameter passing topologies.
//
//
//     Connection types              action
//    ----------------------  ------------------------------------------
//
//    Getter    --> Setter    values pushed repeatedly after graph start
//
//    Constant <--> Constant  values set to all while graph is paused
//                            and before start
//
//    Constant  --> Setter    OPTIONAL, setCallback() is called before
//                            start
//                            
//
//
//
//    Getter is always a source point.
//    Setter is always a sink point.
//    Setter is only connected to once or none.
//
//    Setters have no intrinsic value state.  They just have callbacks in
//    the block that receive values.
//
//    Getters have no intrinsic value state, they just spits out values
//    when the owner block tells it to.
//
//    Constant has intrinsic value state, which can only be changed when
//    the graph is paused.  The value when changed is pushed to all
//    parameters that are connected.  The topology of the Constant
//    connections is always a fully connected topological graph for all
//    Constants in the connection group.
//
//    Implementing: Constant --> Setter led to an unobvious interruption,
//    so we made it optional with default being opt out, or not allowed.
//    If the setter is connected to a Constant then the Setter's
//    setCallback() will be called each time the Constant is connected or
//    the connected Constants change as a group.  In some sense the nature
//    of the setCallback() function as a stream graph flow time callback
//    changes depending on if it is connected from a Constant or a
//    Getter.  If the block writer does not understand this there may be
//    bugs in the block.
//
//    The benefit of having the Getter and Setter, be a super class of
//    parameter like Constant the blocks can more easily compare and share
//    values between these objects, automatically finding
//    incompatibilities.
//


// Called when graph is paused.  Parameter, p, must be a constant or a
// getter.
//
void qsParameterSet(struct QsParameter *p, const void *value) {

    // Catch API user coding errors.
    ASSERT(mainThread == pthread_self(), "Not graph main thread");
    ASSERT(p->block->block.graph->flowState == QsGraphPaused);
    ASSERT(p->kind != QsSetter, "Parameter cannot be a setter");

    DASSERT(p);
    DASSERT(value);

    // Constant parameters keep an internal value.
    // Getter parameter just push values to setters but can be initialize
    // here.

    void *val;

    if(p->kind == QsGetter) {
        if(((struct QsGetter *)p)->value == 0) {
            val = ((struct QsGetter *)p)->value = calloc(1, p->size);
            ASSERT(val, "calloc(1,%zu) failed", p->size);
        } else
            val = ((struct QsGetter *)p)->value;
    } else {
        // kind == QsConstant
        val = ((struct QsConstant *)p)->value;
        DASSERT(val);
    }
    memcpy(val, value, p->size);
}

#if 0
static int
InitGetters(const char *name, struct QsParameter *p, void *userData) {

    


    return 0; // keep looping
}


// This is called just after the graph flow starts, in the main thread,
// just after the block start()'s are called.
//
// Do the initial push to the setters if there is a getter value.
//
void GettersStart(struct QsGraph *g) {

    DASSERT(g);
    ASSERT(g->flowState == QsGraphPaused);
    DASSERT(mainThread == pthread_self(), "Not graph main thread");

    for(struct QsBlock *b = g->firstBlock; b; b = b->next) {
            if(b->isSuperBlock) continue;
            struct QsSimpleBlock *smB = (struct QsSimpleBlock *) b;
            qsDictionaryForEach(smB->getters, 

    }

    DASSERT(p->kind != QsSetter, "Parameter cannot be a setter");




}
#endif


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


    // Getters only connect to 0 to N setters.

    struct QsGetter *g = (struct QsGetter *) p;

    for(uint32_t i = g->numSetters - 1; i != -1; --i) {

        struct QsSetter *s = g->setters[i];
        DASSERT(s);
        DASSERT(s->feeder == p);
        DASSERT(s->parameter.type == p->type);
        DASSERT(s->parameter.size == p->size);

        CHECK(pthread_mutex_lock(s->mutex));

        if(!s->haveValueQueued) {
            // queue up this event.
            //
            // We need an inner mutex lock.  Kind of scary.
            CHECK(pthread_mutex_lock(&
                    ((struct QsBlock *)p->block)->graph->mutex));

            if(!s->trigger) {
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
                continue;
            }

            // Queue a job for this trigger.
            CheckAndQueueTrigger(s->trigger);

            CHECK(pthread_mutex_unlock(&
                    ((struct QsBlock *)p->block)->graph->mutex));
            s->haveValueQueued = true;
        }

        memcpy(s->value, value, p->size);
        
        CHECK(pthread_mutex_unlock(s->mutex)); 
    }

    return g->numSetters;
}


static
int PrintParameterCB(const char *pname, struct QsParameter *p,
            FILE *file) {

    if(p->kind == QsConstant)
        fprintf(file, "Constant ");
    if(p->kind == QsGetter)
        fprintf(file, "Getter   ");
    if(p->kind == QsSetter)
        fprintf(file, "Setter   ");

    switch(p->type) {
        case QS_NONE:
            fprintf(file, "NONE   %4zu ", p->size);
            break;
        case QS_DOUBLE:
            fprintf(file, "DOUBLE %4zu ", p->size);
            break;
        case QS_FLOAT:
            fprintf(file, "FLOAT  %4zu ", p->size);
            break;
        case QS_UINT64:
            fprintf(file, "UINT64 %4zu ", p->size);
            break;
    }

    fprintf(file, "%s %s\n", ((struct QsBlock *)p->block)->name, pname);
    return 0; // keep going.
}


void qsGraphParametersPrint(struct QsGraph *graph, FILE *file) {

    DASSERT(graph);
    DASSERT(file);

    fprintf(file, " kind    type   size block  parameter_name\n"
                  "------   ------ ---- ------ --------------\n");

    for(struct QsBlock *b = graph->firstBlock; b; b = b->next) {
        if(b->isSuperBlock) continue;
        qsDictionaryForEach(((struct QsSimpleBlock *)b)->getters,
                (int (*) (const char *key, void *value,
                    void *userData)) PrintParameterCB, file);
        qsDictionaryForEach(((struct QsSimpleBlock *)b)->setters,
                (int (*) (const char *key, void *value,
                    void *userData)) PrintParameterCB, file);
    }
}
