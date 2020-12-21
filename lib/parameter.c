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
#include "GET_BLOCK.h"


// For braking parameter connections when there are just 2 parameters that
// are connected to each other.
static
void Disconnect2Parameters(struct QsParameter *p) {

    // This just initializes the 2 parameter connection data elements in
    // the parameter structure: "first", "next", and "numConnections" in
    // addition to un-sharing the "value" memory.

    struct QsParameter *first = p->first;
    DASSERT(p->first);

    struct QsParameter *i = first->next;
    DASSERT(i);
    DASSERT(i->numConnections == 2);
    i->numConnections = 0;
    i->first = 0;
    DASSERT(i->next == 0);

    if(i->kind != QsGetter && first->kind != QsGetter) {
        DASSERT(first->value);
        DASSERT(i->value == first->value);
        // It was sharing the memory for value, now it will not.
        i->value = calloc(1, p->size);
        ASSERT(i->value, "calloc(1,%zu) failed", p->size);
        memcpy(i->value, first->value, p->size);
    }

    DASSERT(first->numConnections == 2);
    first->numConnections = 0;
    first->first = 0;
    first->next = 0;
}


static
void DisconnectConstantParameter(struct QsParameter *p) {

    if(!p->first) return; // no connection

    DASSERT(p->numConnections >= 2);

    if(p->numConnections == 2) {
        // This p and the one it connects with are the only 2 parameters.
        // Now reinitialize both of them.
        Disconnect2Parameters(p);
        return;
    }

    struct QsParameter *i;
    // The new first.
    struct QsParameter *first = p->first;

    // Remove p from the list
    if(p != p->first) {
        for(i = p->first; i; i = i->next)
            if(i->next == p) {
                i->next = p->next;
                break;
            }
        // We should have done a break.
        DASSERT(i);
        // Now the list "next"s is correct, but not the other variables.

    } else {
        DASSERT(p->next);
        first = p->next;
    }

    // Now the list "next"s is correct.

    for(i = first; i; i = i->next) {
        i->first = first;
        --i->numConnections;
    }


    // We need p to have a separate copy of value
    DASSERT(p->first->value);
    DASSERT(p->first->value == p->value);
    p->value = calloc(1, p->size);
    ASSERT(p->value, "calloc(1,%zu) failed", p->size);
    memcpy(p->value, p->first->value, p->size);

    // See if p is the last constant in the connection group
    p->first = 0;
    p->next = 0;
    p->numConnections = 0;


    // Now all parameters in the connection list is correct except we need
    // to be sure there is a constant in the list and put it first;
    // otherwise this connection list will be dissolved.

    struct QsParameter *c;

    for(c = first; c; c = c->next)
        if(c->kind == QsConstant)
            break;

    if(!c) {
        // We do not have a constant parameter in the list, and setters
        // cannot just connect to setters; so we reset connections in this
        // group.
        for(i = first; i; i = i->next) {

            i->numConnections = 0;
            i->next = 0;
            i->first = 0;
            if(i != first) {
                i->value = calloc(1, p->size);
                ASSERT(i->value, "calloc(1,%zu) failed", p->size);
                memcpy(i->value, p->value, p->size);
            }
        }
        return;
    }

    if(c == first)
        // The "first" is a constant parameter. 
        return;

    // Make c first
    for(i = first; i; i = i->next) {
        i->first = c;
        if(i->next == c)
            // rip out c
            i->next = c->next;
    }
    // The old first is after c.
    c->next = first;
}


static
void DisconnectGetterParameter(struct QsParameter *p) {

    if(!p->first) return; // no connection

    DASSERT(p->numConnections >= 2);

    if(p->numConnections == 2) {
        // This p and the one it connects with are the only 2 parameters.
        // Now reinitialize both of them.
        Disconnect2Parameters(p);
        return;
    }

    // Getters are first in the connection list.
    DASSERT(p->first == p);

    // Getters only connect to setters.  Setters need the getter to
    // be connected.  So we reset them all.
    //
    // Note: we are editing the list as we go through the list.
    for(struct QsParameter *i = p->first; i;) {
        struct QsParameter *next = i->next;
        i->numConnections = 0;
        i->next = 0;
        i->first = 0;
        i = next;
    }
}


static
void DisconnectSetterParameter(struct QsParameter *p) {

    if(!p->first) return; // no connection

    DASSERT(p->numConnections >= 2);

    if(p->numConnections == 2) {
        // This p and the one it connects with are the only 2 parameters.
        // Now reinitialize both of them.
        Disconnect2Parameters(p);
        return;
    }

    // A Setter will never be first in the connection list, so we
    // just need to reinitialize it.
    //
    // The first could be a getter or a constant, but not a setter.
    //
    DASSERT(p != p->first);
    DASSERT(p->first);
    DASSERT(p->first == p->first->first);

    for(struct QsParameter *i = p->first; i; i = i->next) {
        --i->numConnections;
        if(i->next == p) {
            // p is ripped out.
            i->next = p->next;
        }
    }

    if(p->first->kind == QsConstant) {
        p->value = calloc(1, p->size);
        ASSERT(p->value, "calloc(1,%zu) failed", p->size);
        memcpy(p->value, p->first->value, p->size);
    }
    p->first = 0;
    p->next = 0;
    p->numConnections = 0;
}


static
void FreeParameter(struct QsParameter *p) {

    DASSERT(p);
    DASSERT(p->name);
    DASSERT(p->value);


    // Make sure it's disconnected from other parameters.
    switch(p->kind) {
        case QsConstant:
            DisconnectConstantParameter(p);
            break;
        case QsGetter:
            DisconnectGetterParameter(p);
            break;
        case QsSetter:
            DisconnectSetterParameter(p);
            break;
    }

    DASSERT(p->value);

#ifdef DEBUG
    memset(p->value, 0, p->size);
#endif
    free(p->value);


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
        enum QsParameterKind kind, size_t structSize,
        const void *initValue) {

    DASSERT(pdict);
    DASSERT(psize);
    DASSERT(structSize >= sizeof(struct QsParameter));
    DASSERT(b->block.isSuperBlock == false);

    // Check that parameter name is not already present in blocks
    // list of this kind of parameter.
    if(qsDictionaryFind(pdict, pname)) {
        ERROR("%s parameter named \"%s\" is already in block \"%s\"",
                parameterKind, pname, bname);
        return 0; // fail
    }
    // A constant parameter also can't have the same name as a setter and
    // the setters are not in the same dictionary as the constants, so we
    // need another parameter name check for 2 cases:
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
    p->value = calloc(1, psize);
    ASSERT(p->value, "calloc(1,%zu) failed", psize);
    if(initValue)
        memcpy(p->value, initValue, psize);

    struct QsDictionary *entry = 0;
    ASSERT(qsDictionaryInsert(pdict, pname, p, &entry) == 0);
    DASSERT(entry);
    qsDictionarySetFreeValueOnDestroy(entry,
            (void (*)(void *)) FreeParameter);

    return p;
}


struct QsParameter *
qsParameterGetterCreate(struct QsBlock *b, const char *pname,
        enum QsParameterType type, size_t psize, const void *initVal) {

    ASSERT(pname);
    ASSERT(pname[0]);
    GET_SIMPLEBLOCK_IN_DECLARE(b);
    struct QsSimpleBlock *smB = (struct QsSimpleBlock *) b;

    struct QsGetter *p = AllocateParameter("Getter",
        smB, smB->getters, psize, type, b->name, pname,
        QsGetter, sizeof(*p), initVal);
    if(!p) return 0;

    return (struct QsParameter *) p;
}


struct QsParameter *
qsParameterSetterCreate(struct QsBlock *b, const char *pname,
        enum QsParameterType type, size_t psize,
        int (*setCallback)(struct QsParameter *p,
            const void *value, void *userData),
        void *userData, uint32_t flags, const void *initialValue) {

    ASSERT(pname);
    ASSERT(pname[0]);
    GET_SIMPLEBLOCK_IN_DECLARE(b);
    struct QsSimpleBlock *smB = (struct QsSimpleBlock *) b;

    struct QsSetter *p = AllocateParameter("Setter",
        smB, smB->setters, psize, type, b->name, pname,
        QsSetter, sizeof(*p), initialValue);
    if(!p) return 0;
    p->userData = userData;
    p->setCallback = setCallback;
    p->mutex = &smB->mutex;
    p->callbackWhilePaused = flags & QS_SETS_WHILE_PAUSED;

    return (struct QsParameter *) p;
}


struct QsParameter *
qsParameterConstantCreate(struct QsBlock *b, const char *pname,
        enum QsParameterType type, size_t psize,
        int (*setCallback)(struct QsParameter *p,
            const void *value, void *userData),
        void *userData, const void *initialVal) {

    ASSERT(pname);
    ASSERT(pname[0]);
    GET_SIMPLEBLOCK_IN_DECLARE(b);
    struct QsSimpleBlock *smB = (struct QsSimpleBlock *) b;

    struct QsConstant *p = AllocateParameter("Constant",
        smB, smB->getters, psize, type, b->name, pname,
        QsConstant, sizeof(*p), initialVal);
    if(!p) return 0;

    p->setCallback = setCallback;
    p->userData = userData;

    return (struct QsParameter *) p;
}


uint32_t qsParameterNumConnections(struct QsParameter *p) {

    DASSERT(p);
    // This can only be called in start or stop, and TODO maybe in the
    // stream runner code.
    DASSERT(p->block);
    ASSERT(((struct QsBlock *)p->block)->inWhichCallback == _QS_IN_START ||
            ((struct QsBlock *)p->block)->inWhichCallback == _QS_IN_STOP); 
    
    uint32_t num = p->numConnections;

    if(num == 2) num = 1;
    return num;
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


//
// p1 is a constant parameter or a getter
//
// p2 can be a constant parameter or a setter parameter
//
// Add to connections list and then make the values consistent.
//
// I'm pleasantly surprised how simple this worked out to be.  This
// one simple function handles making all connections.  We don't need
// any memory allocations to make connections.  The connections are in
// a singly linked list we make with
//
//  QsParameter::first, QsParameter::next, and QsParameter::numConnections
//
// All the parameters in a connection group have the same "first" and
// "numConnections".   For a Getter connection group "first" always points
// to the one getter in the group, and the rest are always setters.
//
// The only other type of connection group is a group of N constants and M
// setters, where N = 1,2,3,... and M = 0,1,2,3,...
//
static inline void
AddParameterConnections(struct QsParameter *p1, struct QsParameter *p2) {

    // Merge list1 with list2.

    if(!p1->first) {
        // list 1 empty.  First add itself.
        p1->first = p1;
        DASSERT(p1->next == 0);
        DASSERT(p1->numConnections == 0);
        p1->numConnections = 1;
    }
    if(!p2->first) {
        // list 2 empty.  First add itself.
        p2->first = p2;
        DASSERT(p2->next == 0);
        DASSERT(p2->numConnections == 0);
        p2->numConnections = 1;
    }

    // Make sure that they are not connected already.  If they are, the 2
    // lists should be the same; that's just how we define it to be.
    if(p1->first == p2->first) {
        DASSERT(p1->first->next);
        DASSERT(p1->numConnections == p2->numConnections);
        // They are already connected and that's it.
        return;
    }

    // They are not connected yet.  So none of the parameters should be in
    // common between the 2 groups, so we just need to connect the two
    // lists.  And make all of list 2 first be list 1.
    struct QsParameter *i;
    uint32_t numConnections = p1->numConnections + p2->numConnections;
    DASSERT(numConnections >= 2);
    DASSERT(p1->first == p1);
    for(i = p1->first; i->next; i = i->next) {
        DASSERT(i->first == p1);
        i->numConnections = numConnections;
    }
    DASSERT(i->first == p1);
    i->numConnections = numConnections;
    // i->next ==0 and now connection them
    i->next = p2->first;
    for(i = i->next; i; i = i->next) {
        i->first = p1;
        i->numConnections = numConnections;
        if(p1->kind == QsConstant) {
            // Make the value be shared with p1
            DASSERT(i->value);
#ifdef DEBUG
            memset(i->value, 0, i->size);
#endif
            free(i->value);
            i->value = p1->value;
        }
    }
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
    memcpy(value, s->parameter.value, s->parameter.size);


    CHECK(pthread_mutex_unlock(s->mutex));

    // Now we do not know how long this will take.  The users setCallback
    // may change the contents of value, but the value memory will only be
    // valid while the function is being called.  They may copy the value
    // yet again.  Note: again, nothing like stream data...
    //
    return s->setCallback(&s->parameter, value, s->userData);
}


// This is called when the value is changed in a connected group of parameters that
// has a constant parameter in the group.
//static
void PushContantValues(struct QsConstant *c, void *value) {

    ASSERT(mainThread == pthread_self(), "Not graph main thread");

    // MORE HEREEEEEEEEEEEEEEEEEEEEEEEEEEEE

}

void qsParameterDisconnect(struct QsParameter *p, struct QsParameter *p1) {

    ASSERT(mainThread == pthread_self(), "Not graph main thread");
    DASSERT(p);

    ASSERT(0, "Write this CODE");

    // TODO: Oh, what fun it will be to write this.

}



// TODO: We should consider ways to engineer out all the wrong modes in
// the input parameters of this function, instead of adding all these
// stupid failure if() checks in this function.
//
// Connect from p0 to p1
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

    if(p0->kind == QsConstant && p1->kind == QsSetter &&
            !((struct QsSetter *)p1)->callbackWhilePaused) {
        ERROR("Setter parameter %s:%s cannot be connected to from a constant"
                " parameter; setters QS_SETS_WHILE_PAUSED flag was not set",
                p1->block->block.name, p1->name);
        DASSERT(0);
        return -2;
    }

    if(p0 == p1) {
        ERROR("No parameter \"%s\" can connect to itself", p1->name);
        DASSERT(0);
        return -3;
    }

    if(p0->block == p1->block) {
        ERROR("Block \"%s\" cannot connect parameters in itself",
                ((struct QsBlock *)p0->block)->name);
        DASSERT(0);
        return -4;
    }

    // The parameters size and type must match.
    if(p0->type != p1->type || p0->size != p1->size) {
        ERROR("Parameter \"%s:%s\" is size %zu and type %d; and "
                "parameter \"%s:%s\" is size %zu and type %d",
                ((struct QsBlock *)p0->block)->name,
                p0->name, p0->size, p0->type,
                ((struct QsBlock *)p1->block)->name,
                p1->name, p1->size, p1->type);
        DASSERT(0);
        return -5;
    }

    if(p0->first && p1->first && p0->first == p1->first) {
        // These parameters shared the same connection list.
        WARN("Parameters already connected");
        return 0; // success
    }


    if(p1->kind == QsSetter) {
        // We are connecting to a setter.
        // // A setter cannot be connected to twice.
        if(p1->first) {
            ERROR("Setter \"%s:%s\" already has a connection",
                    ((struct QsBlock *)p1->block)->name, p1->name);
            DASSERT(0);
            return -7;
        }
        struct QsSetter *setter = (struct QsSetter *)p1;
        if(p0->kind == QsGetter) {
            // We are connecting from getter to setter.
            AllocateTrigger(sizeof(*setter->trigger),
                    p1->block,
                    QsSetterTrigger, (int (*)(void *)) SetterTriggerCB,
                    setter/*userData*/, (bool (*)(void *)) 0,
                    false/*isSource*/);
            // Triggers are managed by the block so we do not need the
            // pointer at this time.  The block will have it.
            //
            // setter->trigger gets set later at TriggerStart().
            // We use setter->trigger as a flag to know that the
            // setter is triggering or not.
        }
    }

    // Which we connect "to" matters.
    if(p0->kind == QsConstant)
        AddParameterConnections(p0, p1);
    else if(p1->kind == QsConstant) {
        DASSERT(p0->kind == QsSetter);
        AddParameterConnections(p1, p0);
    } else if(p0->kind == QsGetter) {
        DASSERT(p1->kind == QsSetter);
        AddParameterConnections(p0, p1);
    }

    return 0;
}


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


void QueueUpSetterFromGetter(struct QsSetter *s, struct QsParameter *p) {

    DASSERT(s);
    DASSERT(s->parameter.first == p);
    DASSERT(s->parameter.first->kind == QsGetter);
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
            return;
        }

        // Queue a job for this trigger.
        CheckAndQueueTrigger(s->trigger);

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
    }

    switch(p->type) {
        case QsNone:
            fprintf(file, "NONE   %4zu ", p->size);
            break;
        case QsDouble:
            fprintf(file, "DOUBLE %4zu ", p->size);
            break;
        case QsFloat:
            fprintf(file, "FLOAT  %4zu ", p->size);
            break;
        case QsUint64:
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
