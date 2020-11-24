#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "../include/quickstream/block.h"
#include "../include/quickstream/builder.h"


#include "debug.h"
#include "Dictionary.h"
#include "parameter.h"
#include "block.h"
#include "graph.h"



// A terrible CPP macro that sets b to the current block if it is not set
// already.  Also checks that we are in a block bootstrap() call.
// Also checks that this is the main thread that creates graphs.
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
        struct QsBlock *b,
        struct QsDictionary *pdict,
        size_t psize, enum QsParameterType type,
        const char *bname,
        const char *pname,
        enum QsParameterKind kind, size_t structSize) {

    DASSERT(pdict);
    DASSERT(psize);
    DASSERT(structSize > sizeof(struct QsParameter));
    DASSERT(b->isSuperBlock == false);

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
            qsDictionaryFind(((struct QsSimpleBlock *)b)->getters, pname);
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
            qsDictionaryFind(((struct QsSimpleBlock *)b)->setters, pname);
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
        void (*setCallback)(struct QsParameter *p,
            const void *value, size_t size, void *userData),
        void (*cleanup)(struct QsParameter *, void *userData),
        void *userData, uint32_t flags) {

    GET_OWNER_BLOCK(b);
    struct QsSimpleBlock *smB = (struct QsSimpleBlock *) b;

    struct QsSetter *p = AllocateParameter("Setter",
        b,
        smB->setters, psize, type, b->name, pname,
        QsSetter, sizeof(*p));
    if(!p) return 0;
    p->userData = userData;
    p->setCallback = setCallback;
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
        b,
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
        b,
        smB->getters, psize, type, b->name, pname,
        QsConstant, sizeof(*p));
    if(!p) return 0;

    // p->value will be free()ed in FreeParameter().
    p->value = malloc(psize);
    ASSERT(p->value, "malloc(%zu) failed", psize);

    memcpy(p->value, initialVal, psize);

    return (struct QsParameter *) p;
}


uint32_t qsParameterNumConnections(struct QsParameter *p) {

    DASSERT(p);
    // This can only be called in start or stop, and TODO maybe in the
    // stream runner code.
    DASSERT(p->block);
    ASSERT(p->block->inWhichCallback == _QS_IN_START ||
            p->block->inWhichCallback == _QS_IN_STOP); 

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
    if( !(p0->kind == QsGetter   && p1->kind == QsSetter) ||
        !(p0->kind == QsConstant && p1->kind == QsSetter) ||
        !(p0->kind == QsConstant && p1->kind == QsConstant) 
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

    if(p0->block != p1->block) {
        ERROR("Block \"%s\" cannot connect parameters in itself",
                p0->block->name);
        DASSERT(0);
        return -1;
    }

    // The parameters size and type must match.
    if(p0->type != p1->type || p0->size != p1->size) {
        ERROR("Parameter \"%s:%s\" is size %zu and type %d; and "
                "parameter \"%s:%s\" is size %zu and type %d",
                p0->block->name, p0->name, p0->size, p0->type,
                p1->block->name, p1->name, p1->size, p1->type);
        return -1;
    }


    if(p1->kind == QsSetter) {
        if(((struct QsSetter *)p1)->feeder) {
            ERROR("Setter \"%s:%s\" already has a connection",
                    p1->block->name, p1->name);
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

    return 0;
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

    fprintf(file, "%s %s\n", p->block->name, pname);
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
