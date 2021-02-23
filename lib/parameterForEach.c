#include <stdint.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <sys/types.h>
#include <regex.h>
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




struct Parameter_St {

    // Return the total number of parameters this was called with.
    ssize_t ret;

    struct QsBlock *block;
    enum QsParameterKind kind;
    enum QsParameterType type;
    size_t size;
    const char *name;
    void *userData;
    int (*callback)(struct QsParameter *p,
            enum QsParameterKind kind,
            enum QsParameterType type,
            size_t size,
            const char *pName, void *userData);
    // So called pattern buffer storage area.
    // See "man 3 regcomp".
    regex_t regex;
    // They did not tell us how to zero the above regex_t, so
    // we need a flag that tells us to use the regex_t regex:
    bool useRegex;
};


static inline
int CallCallback(struct QsParameter *p, struct Parameter_St *st) {

    DASSERT(strcmp(st->name, p->name) == 0 || st->useRegex);
    DASSERT(p->kind == st->kind || st->kind == QsAny);
    DASSERT(p->size == st->size);

    if(st->useRegex && regexec(&st->regex, p->name, 0, 0, 0))
        return 0;

    ++st->ret;
    return st->callback(p, p->kind, p->type, p->size,
                                p->name, st->userData);
}


static
int BlockForEachParameter_CB(const char *key, struct QsParameter *p,
        struct Parameter_St *st) {

    if(st->type != p->type || st->size != p->size)
        return 0;

    if(CallCallback(p, st))
        // discontinue calling callback.
        return 1;

    return 0; // continue.
}


static
int GraphForEachBlock_CB(const char *key, struct QsBlock *b,
        struct Parameter_St *st) {

    if(b->isSuperBlock)
        // super blocks do not have any parameters.
        return 0;

    
    struct QsSimpleBlock *smB = (struct QsSimpleBlock *) b;

    if(st->name && !st->useRegex) {
        struct QsParameter *p;
        switch(st->kind) {

            case QsAny:
                p = qsDictionaryFind(smB->constants, st->name);
                if(p && CallCallback(p, st))
                    return 1;
                p = qsDictionaryFind(smB->getters, st->name);
                if(p && CallCallback(p, st))
                    return 1;
                p = qsDictionaryFind(smB->setters, st->name);
                if(p && CallCallback(p, st))
                    return 1;
                break;

            case QsConstant:
                p = qsDictionaryFind(smB->constants, st->name);
                if(p && CallCallback(p, st))
                    return 1;
                break;

            case QsGetter:
                p = qsDictionaryFind(smB->getters, st->name);
                if(p && CallCallback(p, st))
                    return 1;
                break;

            case QsSetter:
                p = qsDictionaryFind(smB->setters, st->name);
                if(p && CallCallback(p, st))
                    return 1;
                break;
        }
        return 0;
    }

    switch(st->kind) {

        case QsAny:
            qsDictionaryForEach(smB->constants,
                (int (*) (const char *, void *, void *))
                BlockForEachParameter_CB, st);
            qsDictionaryForEach(smB->getters,
                (int (*) (const char *, void *, void *))
                BlockForEachParameter_CB, st);
            qsDictionaryForEach(smB->setters,
                (int (*) (const char *, void *, void *))
                BlockForEachParameter_CB, st);
        break;

        case QsConstant:
            qsDictionaryForEach(smB->constants,
                (int (*) (const char *, void *, void *))
                BlockForEachParameter_CB, st);
        break;

        case QsGetter:
            qsDictionaryForEach(smB->getters,
                (int (*) (const char *, void *, void *))
                BlockForEachParameter_CB, st);
        break;

        case QsSetter:
            qsDictionaryForEach(smB->setters,
                (int (*) (const char *, void *, void *))
                BlockForEachParameter_CB, st);
        break;
    }


    return 0; // 0 => keep going
}


ssize_t qsParameterForEach(struct QsGraph *graph,
        struct QsBlock *block,
        enum QsParameterKind kind,
        enum QsParameterType type,
        size_t size,
        const char *pName,
        int (*callback)(struct QsParameter *p,
            enum QsParameterKind kind,
            enum QsParameterType type,
            size_t size,
            const char *pName, void *userData),
        void *userData, uint32_t flags) {

    if(!block && !graph) {
        ERROR("block not set");
        return -1;
    }

    struct Parameter_St st;

    st.block = block;
    st.kind = kind;
    st.type = type;
    st.size = size;
    st.name = pName;
    st.userData = userData;
    st.callback = callback;
    st.ret = 0;
    st.useRegex = false;

    if(flags & QS_PNAME_REGEX) {
        int ret;
        if(flags & QS_PNAME_ICASE)
            // TODO: REG_EXTENDED ??
            ret = regcomp(&st.regex, pName, REG_ICASE);
        else
            ret = regcomp(&st.regex, pName, 0);
        if(ret) {
            char error[512];
            regerror(ret, &st.regex, error, 512);
            ERROR("regcomp(,\"%s\",) failed:%s", pName, error);
            return -1;
        }
    }

    if(block == 0 && graph)
        qsDictionaryForEach(graph->blocks,
                (int (*) (const char *, void *,
                        void *)) GraphForEachBlock_CB,
                &st /*userData*/);
    else
        GraphForEachBlock_CB(block->name, block, &st);

    if(flags & QS_PNAME_REGEX)
        regfree(&st.regex);

    return st.ret;
}
