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


// For braking parameter connections when there are just 2 parameters that
// are connected to each other.
static
void Disconnect2Parameters(struct QsParameter *p) {

    // This just initializes the 2 parameter connection data elements in
    // the parameter structure: "first", "next", and "numConnections" in
    // addition to un-sharing the "value" memory, if need be.

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
    DASSERT(first);

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
    // to be sure if there is a constant in the list, then we put it
    // first.
    //
    struct QsParameter *c;

    for(c = first; c; c = c->next)
        if(c->kind == QsConstant)
            break;

    if(!c || c == first)
        // We have no constant in a group of setters.  They will
        // act like a group of constants unless a getter is connected to
        // them later.  OR We have a first in the list that is a
        // constant.
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

    // Getters are always the first in the connection list.  There is
    // only one getter in the group.
    DASSERT(p->first == p);

    // Getters only connect to setters.  With this getter the remaining
    // setters will act like a group of constants with just setters in the
    // group; until they get connected to another getter.
    //
    struct QsParameter *first = p->first->next;
    DASSERT(first);
    DASSERT(first->kind == QsSetter);
    uint32_t numConnections = first->numConnections - 1;
    first->numConnections = numConnections;
    DASSERT(first->next);

    for(struct QsParameter *i = first->next; i; i = i->next) {
        i->numConnections = numConnections;
        i->first = first;
        DASSERT(i->kind == QsSetter);
        DASSERT(i->value);
        DASSERT(i->value != first->value);
        DASSERT(i->size == first->size);
#ifdef DEBUG
        memset(i->value, 0, i->size);
#endif
        free(i->value);
        i->value = first->value;
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

    DASSERT(p->first);
    struct QsParameter *first = p->first;

    if(p->first == p) {
        // This is not a Getter group yet.
        DASSERT(p->next);
        DASSERT(p->next->kind == QsSetter);
        // We have a new first.
        first = p->next;
    }

    if(p->first->kind != QsGetter) {
        // This parameter was in a constant like group so we need to
        // reallocate the value memory that will no longer be shared in
        // the group.
        DASSERT(p->value == p->first->value);
        // TODO: We may just be freeing this soon.
        p->value = calloc(1, p->size);
        ASSERT(p->value, "calloc(1,%zu) failed", p->size);
        memcpy(p->value, p->first->value, p->size);
    }

    uint32_t numConnections = p->numConnections - 1;

    for(struct QsParameter *i = first; i; i = i->next) {
        DASSERT(i->numConnections == numConnections + 1);
        i->numConnections = numConnections;
        i->first = first;
        if(i->next == p) {
            // p is ripped out if it was not first.
            i->next = p->next;
        }
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
    DASSERT(p->block);

    // Make sure it's disconnected from other parameters.
    switch(p->kind) {
        case QsConstant:
            DisconnectConstantParameter(p);
            --p->block->numConstants;
            break;
        case QsGetter:
            DisconnectGetterParameter(p);
            --p->block->numGetters;
            break;
        case QsSetter:
            DisconnectSetterParameter(p);
            --p->block->numSetters;
            break;
        case QsAny:
            ASSERT(0);
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
        case QsAny:
            ASSERT(0);
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
    // But a setter and getter can have the same name.
    //
    // otherwise parameter connection name space rules do not work
    // unambiguously.  It's because getter and constants act the same as
    // far as connecting to setters, but constants can also connect to
    // other constants, but getters can't connect to constants.
    //
    if(kind == QsSetter) {
        // 1. This is a setter and we can't have a constant with that name:
        struct QsParameter *p =
            qsDictionaryFind(b->constants, pname);
        if(p) {
            DASSERT(p->kind == QsConstant);
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

    ++smB->numGetters;
    return (struct QsParameter *) p;
}


struct QsParameter *
qsParameterSetterCreate(struct QsBlock *b, const char *pname,
        enum QsParameterType type, size_t psize,
        int (*setCallback)(struct QsParameter *p,
            const void *value, void *userData),
        void *userData, const void *initialValue) {

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
    //p->callbackWhilePaused = flags & QS_SETS_WHILE_PAUSED;

    ++smB->numSetters;
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
        smB, smB->constants, psize, type, b->name, pname,
        QsConstant, sizeof(*p), initialVal);
    if(!p) return 0;

    p->setCallback = setCallback;
    p->userData = userData;

    ++smB->numConstants;
    return (struct QsParameter *) p;
}


void qsParameterSetCallback(struct QsParameter *p,
        int (*setCallback)(struct QsParameter *p,
            const void *value, void *userData)) {

    ASSERT(mainThread == pthread_self(), "Not graph main thread");
    DASSERT(p);
    ASSERT(p->kind != QsGetter,
            "QsParameter of type getter do not have setCallbacks");

    if(p->kind == QsSetter)
        ((struct QsSetter *)p)->setCallback = setCallback;
    else {
        DASSERT(p->kind == QsConstant);
        ((struct QsConstant *)p)->setCallback = setCallback;
    }
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
        const char *pname, enum QsParameterKind kind) {

    if(!block)
        block = GetBlock();

    DASSERT(block);
    DASSERT(!block->isSuperBlock);
    
    if(block->isSuperBlock) return 0;

    struct QsSimpleBlock *smB = (struct QsSimpleBlock *) block;

    DASSERT(smB->setters);
    DASSERT(smB->getters);
    DASSERT(smB->constants);

    struct QsDictionary *d;
    if(kind == QsSetter)
        d = smB->setters;
    else if(kind == QsGetter)
        d = smB->getters;
    else {
        DASSERT(kind == QsConstant);
        d = smB->constants;
    }

    return (struct QsParameter *) qsDictionaryFind(d, pname);
}


void
qsParameterGetValueByName(const char *pname, void *value, size_t size) {
    struct QsParameter *p = qsParameterGetPointer(0, pname, QsConstant);
    if(!p) p = qsParameterGetPointer(0, pname, QsGetter);

    ASSERT(p, "parameter named \"%s\" not found", pname);
    ASSERT(size == p->size,
            "parameter named \"%s\" is the wrong size",
            pname);
    qsParameterGetValue(p, value);
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

    // There is 2 kinds of groups of parameters:
    //
    //   1. constant group that can have any mix of constants and setters,
    //      the first in the list will be a constant, if there is a
    //      constant in the group
    //
    //   2. a getter group that has one getter first and any number of
    //      setters.
    //

    // Merge list1 with list2.

    DASSERT(p1 != p2);
    DASSERT(!p1->first || !p2->first || p1->first != p2->first);

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

    // They are not connected yet.  So none of the parameters should be in
    // common between the 2 groups, so we just need to connect the two
    // lists.  And make all of list 2 be list 1, with list 2 appended.
    struct QsParameter *i;
    uint32_t numConnections = p1->numConnections + p2->numConnections;
    DASSERT(numConnections >= 2);

    for(i = p1->first; i->next; i = i->next) {
        i->numConnections = numConnections;
        DASSERT(p1->kind == QsGetter || i->value == i->next->value);
    }
    i->numConnections = numConnections;
    // i->next ==0 and now connect them
    i->next = p2->first;
    i = i->next;

    DASSERT(i->value);

    if(p1->kind != QsGetter) {
        // The old list 2 should have been sharing the value memory, now
        // we need to make it not share.
        DASSERT(!i->next || i->value == i->next->value);
#ifdef DEBUG
        memset(i->value, 0, i->size);
#endif
        free(i->value);
    }
    for(; i; i = i->next) {
        i->first = p1;
        i->numConnections = numConnections;
        if(p1->kind != QsGetter) {
            // Make the value be shared with p1
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

    if(!s->setCallback) return 0;

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
    DASSERT(p0->name);
    DASSERT(p1->name);


    if(p1->kind == QsGetter) {
        // We'll switch the two parameters keeping the to parameter, p0,
        // as a getter. 
        struct QsParameter *p = p0;
        p0 = p1;
        p1 = p;
    }

    // We can connect certain kinds of parameters from and to each other.
    // Here is the complete list of possible kinds of connections:
    //
    //   1. Getter    to  Setter
    //   2. Constant  to  Setter
    //   3. Setter    to  Setter
    //   4. Constant  to  Constant
    //
    if(p0->kind == QsGetter)
        if(p1->kind == QsGetter || p1->kind == QsConstant || 
                (p1->first && p1->first->kind != QsSetter)) {
            ERROR("Wrong mix of kinds of parameter to "
                    "connect from and to");
            DASSERT(0);
            return -1;
        }

    if(p0 == p1) {
        ERROR("The parameter \"%s\" cannot connect to itself", p1->name);
        DASSERT(0);
        return -2;
    }

#if 0
    if(p0->block == p1->block && p0->kind == QsGetter) {
        ERROR("Block \"%s\" cannot connect getter parameters in itself",
                ((struct QsBlock *)p0->block)->name);
        DASSERT(0);
        return -3;
    }
#endif

    // The parameters size and type must match.
    if(p0->type != p1->type || p0->size != p1->size) {
        ERROR("Parameter \"%s:%s\" is size %zu and type %d; and "
                "parameter \"%s:%s\" is size %zu and type %d",
                ((struct QsBlock *)p0->block)->name,
                p0->name, p0->size, p0->type,
                ((struct QsBlock *)p1->block)->name,
                p1->name, p1->size, p1->type);
        DASSERT(0);
        return -4;
    }

    if(p0->first && p1->first && p0->first == p1->first) {
        // These parameters shared the same connection list.
        WARN("Parameters already connected");
        return 0; // success
    }

    if(p1->kind == QsSetter) {
        // We are connecting to a setter.
        struct QsSetter *setter = (struct QsSetter *)p1;
        if(p0->kind == QsGetter) {

            // We need to make sure there are no constants in the
            // connection list.  Getters cannot connect to constants.
            if(p1->first)
                for(struct QsParameter *p = p1->first; p; p = p->next)
                    if(p->kind == QsConstant) {
                        ERROR("Getter parameter \"%s:%s\" cannot be "
                                "connected to setter parameter \"%s:%s\""
                                " because it is connected with a constant"
                                " \"%s:%s\"",
                                p0->block->block.name, p0->name,
                                p1->block->block.name, p1->name,
                                p->block->block.name, p->name);
                        return -5;
                    }

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

    // Note if there is a getter, it is p0.

    AddParameterConnections(p0, p1);

    return 0;
}
