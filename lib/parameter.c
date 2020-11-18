#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "../include/quickstream/block.h"

#include "debug.h"
#include "Dictionary.h"
#include "parameter.h"
#include "block.h"
#include "app.h"



// A terrible CPP macro that sets b to the current block if it is not set
// already.  Also checks that we are in a block bootstrap() call.
// Also checks that this is the main thread that creates graphs.
//
// Note: blocks can create parameters for other blocks.  That's why
// this macro is important.
//
// Maybe this beats repeating this code many times in this file.
//
#define GET_BLOCK(b) \
    do {\
        ASSERT(pname && pname[0]);\
        ASSERT(mainThread == pthread_self(), "Not graph main thread");\
        if(b)\
            ASSERT((GetBlock())->inWhichCallback == _QS_IN_BOOTSTRAP,\
                "%s() must be called from bootstrap", __func__);\
        else {\
            b = GetBlock();\
            ASSERT(b->inWhichCallback == _QS_IN_BOOTSTRAP,\
                "%s() must be called from bootstrap", __func__);\
        }\
        ASSERT(b->isSuperBlock == false);\
    } while(0)


static
void FreeParameter(struct QsParameter *p) {

    DASSERT(p);
    DASSERT(p->name);

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
        struct QsDictionary *pdict,
        size_t psize, enum QsParameterType type,
        const char *bname,
        const char *pname,
        enum QsParameterKind kind, size_t structSize) {

    DASSERT(pdict);
    DASSERT(pname);
    DASSERT(pname[0]);
    DASSERT(psize);
    DASSERT(structSize > sizeof(struct QsParameter));

    // Check that parameter name is not already present in blocks
    // list of this kind of parameter.
    if(qsDictionaryFind(pdict, pname)) {
        ERROR("%s parameter named \"%s\" is already in block \"%s\"",
                parameterKind, pname, bname);
        return 0; // fail
    }

    struct QsParameter *p = calloc(1, structSize);
    ASSERT(p, "calloc(1,%zu) failed", structSize);

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

    GET_BLOCK(b);
    struct QsSimpleBlock *smB = (struct QsSimpleBlock *) b;

    struct QsSetter *p = AllocateParameter("Setter",
        smB->setters, psize, type, b->name, pname,
        QsSetter, sizeof(*p));
    if(!p) return  (struct QsParameter *) p;




    return (struct QsParameter *) p;
}


struct QsParameter *
qsParameterGetterCreate(struct QsBlock *b, const char *pname,
        enum QsParameterType type, size_t psize) {

    GET_BLOCK(b);
    struct QsSimpleBlock *smB = (struct QsSimpleBlock *) b;

    struct QsGetter *p = AllocateParameter("Getter",
        smB->getters, psize, type, b->name, pname,
        QsGetter, sizeof(*p));
    if(!p) return  (struct QsParameter *) p;


    return (struct QsParameter *) p;
}


struct QsParameter *
qsParameterConstantCreate(struct QsBlock *b, const char *pname,
        enum QsParameterType type, size_t psize) {

    GET_BLOCK(b);
    struct QsSimpleBlock *smB = (struct QsSimpleBlock *) b;

    struct QsConstant *p = AllocateParameter("Constant",
        smB->constants, psize, type, b->name, pname,
        QsConstant, sizeof(*p));
    if(!p) return  (struct QsParameter *) p;


    return (struct QsParameter *) p;
}

