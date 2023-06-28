#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
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
#include "job.h"
#include "config.h"



static inline
void FreeArgv(struct QsAttribute *a) {

    char **argv = a->lastArgv;
    if(!argv) {
        DASSERT(!a->lastArgc);
        DASSERT(!a->parentBlock);
        return;
    }

    DASSERT(a->lastArgc);
    DASSERT(a->parentBlock);

#ifdef DEBUG
    {
        int i = 0;
        for(char **arg = (void *) argv; *arg && i < a->lastArgc; ++arg)
            DZMEM((void *) *arg, strlen(*arg));
    }
#endif
    // Freeing the first string frees them all, because the rest of the
    // strings use the same allocation.
    free((void *) *argv);
    DZMEM((void *) argv, (a->lastArgc+1)*sizeof(*argv));
    // Free the pointers to the strings.
    free(argv);

    a->lastArgc = 0;
    a->lastArgv = 0;
    a->parentBlock = 0;
}


static void
FreeAttribute(struct QsAttribute *a) {

    DASSERT(a);

    FreeArgv(a);

    FreeString(&a->name);
    FreeString(&a->desc);
    FreeString(&a->argSyntax);
    FreeString(&a->currentArgs);
    DZMEM(a, sizeof(*a));
    free(a);
}

static int
ResetSaveConfig(const char *name, struct QsAttribute *a,
        void *userData) {

    FreeArgv(a);

    return 0;
}


void qsGraph_saveConfig(struct QsGraph *g, bool doSave) {

    DASSERT(g);

    CHECK(pthread_mutex_lock(&g->mutex));

    if(doSave)
        g->saveAttributes = true;
    else {
        // Remove all the saved (last) config() argc, argv values:
        //
        // Note: we are only acting on the graph's (top block's)
        // direct children and not lower children.
        for(struct QsBlock *b = g->parentBlock.firstChild;
                b; b = b->nextSibling) {
            struct QsModule *m = Get_Module(b);
            if(m->attributes)
                qsDictionaryForEach(m->attributes,
                        (int (*) (const char *key, void *value,
                                void *userData)) ResetSaveConfig, 0);
        }
        g->saveAttributes = false;
    }

    CHECK(pthread_mutex_unlock(&g->mutex));
}


void qsGraph_removeConfigAttribute(struct QsBlock *b, const char *aName) {

    DASSERT(aName);
    DASSERT(aName[0]);
    DASSERT(b);
    struct QsGraph *g = b->graph;
    DASSERT(g);


    CHECK(pthread_mutex_lock(&g->mutex));

    if(!g->saveAttributes) goto finish;

    struct QsModule *m = Get_Module(b);
    if(m->attributes) {
        struct QsAttribute *a = qsDictionaryFind(m->attributes, aName);
        if(!a) goto finish;
        FreeArgv(a);
    }

finish:

    CHECK(pthread_mutex_unlock(&g->mutex));
}




// argv is not necessarily 0 terminated; but it will be when we call the
// attribute config() function.
//
// This is where the config attribute action takes place.
static int
_qsBlock_config(struct QsBlock *block,
            int argc, const char * const *argv,
            bool didAllocate) {

    DASSERT(block);
    DASSERT(argc);
    struct QsGraph *g = block->graph;
    DASSERT(g);

    if(!didAllocate) {
        size_t slen = 0;
        char **s = (void *) argv;
        int i = 0;
        while(*s && i < argc) {
            slen += strlen(*s) + 1;
            ++i;
            ++s;
        }
        slen += 1; // for the 0 string terminator at the end.
        char **ss = (s = calloc(argc+1, sizeof(*s)));
        ASSERT(s, "calloc(%d,%zu) failed", argc+1, sizeof(*s));
        char *str = calloc(1, slen);
        ASSERT(str, "calloc(1,%zu) failed", slen);
        slen = 0;
        // TODO: This could be a lot more efficient.
        for(i=0; i<argc; ++i) {
            *s = str + slen;
            strcpy(*s, argv[i]);
            ++s;
            slen += strlen(argv[i]) + 1;
        }
        // Terminate it.
        *s = 0;
        // Replace the argv with it.
        argv = (void *) ss;
    }

#if 0
    {
        fprintf(stderr, "Configure Block \"%s\" attribute \"%s\"\n",
                block->name, argv[0]);
        int i=0;
        for(const char * const *arg = argv; *arg; ++arg) {
            DASSERT(i<argc);
            fprintf(stderr, "argv[%d]=%s\n", i++, *arg);
        }
    }
#endif


    CHECK(pthread_mutex_lock(&g->mutex));

    // This module could be a part of a super block or a simple block.
    struct QsModule *m = Module(block);
    struct QsAttribute *a = 0;
    int ret = 0;

    if(m->attributes)
        a = qsDictionaryFind(m->attributes, argv[0]);

    if(!a) {
        ERROR("Block \"%s\" does not have a "
                "configurable attribute named \"%s\"",
                block->name, argv[0]);
        ret = -1; // total failure given there is no correct attribute.
        goto finish;
    }

    DASSERT(a->config);
    // Setup the parser thingy as in qsParseUint32tArray(), for example.
    m->argc = argc;
    m->i = 1; // We skip the argv[0] which is the name of the attribute.
    m->argv = (const char * const *) argv;
    // It should be zero terminated:
    DASSERT(m->argv[argc] == 0);

    struct QsWhichBlock stackSave;
    SetBlockCallback(block, CB_CONFIG, &stackSave);

    // The graph thread pools are not all necessarily halted yet; but they
    // could be.
    //
    // This lock is a recursive lock, so we can just call it again if it
    // is locked already or not.
    //
    // We cannot have this thread run while the affiliated blocks have
    // thread pool worker threads running their block code; so we briefly
    // stop those threads while we call a->config().
    //
    // We could change this to halt fewer blocks things may break in
    // blocks that are not in the quickstream test programs in tests/.
    //
    // Again note: This configuration thingy could be on a super block or
    // a simple block.
    //
    // We thought about running this block callback in thread pool
    // workers, but that is not possible in super blocks since they do not
    // run in thread pool worker threads.
    //
    // Do we really want to halt all the thread pools here?  Maybe just
    // halt the thread pool that runs this block.  Halting them all should
    // be unnecessary if the libquickstream.so API does any additional
    // thread pool halts that it needs to keep worker threads from hosing
    // inter-thread memory.  qsGraph_threadPoolHaltLock() is a recursive
    // lock that compounds as the block calls the libquickstream.so API
    // which in turn calls itself.  Okay trying that...

    uint32_t numHalts = 0;

    if(block->type == QsBlockType_simple)
        numHalts = qsBlock_threadPoolHaltLock((void *)block);

    //qsGraph_threadPoolHaltLock(g, 0);

    // a->config() returns malloc() alloacted string that will be
    // a->currentArgs, or returns 0.
    //
    char *configRet = a->config(argc, argv, block->userData);

    //qsGraph_threadPoolHaltUnlock(g);

    while(numHalts--)
        qsGraph_threadPoolHaltUnlock(g);


    RestoreBlockCallback(&stackSave);


    if(configRet == QS_CONFIG_FAIL) {

        WARN("Failed to configure block \"%s\" attribute \"%s\"",
                block->name, argv[0]);

        // If it failed; we do not change the attribute state data that is
        // saved.  It may be that there is attribute state data saved from
        // before, but we just don't mess with it.  What the state of the
        // block is, is up to the block code, we don't know shit about
        // it.
        //
        // We are intentionally keeping this configure attribute
        // protocol/interface simple, so as to not make it a pain in the
        // ass to write blocks.
        //
        // Note: The qsBlock_config() call is synchronous.  The block that
        // is being configured is forced to do this configure attribute
        // thingy by making the block callback be called in this thread
        // after any possible thread pool worker thread finishes what it
        // was doing just before this call; that's if there was a worker
        // thread working on this block.
        //
        ret = 1; // A failure to configure, but not so bad that anything
                 // will crash.
                 //
                 // ret = 0 would be complete success.
                 //
                 // ret == -1 is user error, bad attribute name.
        goto finish;
    }


    DASSERT(a->currentArgs);
#ifdef DEBUG
    memset(a->currentArgs, 0, strlen(a->currentArgs));
#endif
    free(a->currentArgs);
    a->currentArgs = 0;

    if(!configRet) {
        DASSERT(argc >= 1);
        DASSERT(argv[0]);
        // We need to compose a new a->currentArgs
        size_t l = strlen(argv[0]) + 1;
        a->currentArgs = strdup(argv[0]);
        for(int i=1; i<argc; ++i) {
            size_t newL = l + strlen(argv[i]) + 1;
            a->currentArgs = realloc(a->currentArgs, newL);
            ASSERT(a->currentArgs, "realloc(,%zu) failed", newL);
            l += snprintf(a->currentArgs + l - 1, newL, " %s", argv[i]);
            DASSERT(l == newL);
        }
        //DSPEW("a->currentArgs=\"%s\"", a->currentArgs);
    } else
        // This attribute takes the ownership of this malloc()ed memory.
        // The block code will no longer will need to free() it.
        a->currentArgs = configRet;


    if(g->saveAttributes) {

        // Dump the last values:
        FreeArgv(a);

        // Save them.
        a->lastArgv = (void *) argv;
        a->lastArgc = argc;

        struct QsParentBlock *p =
            GetBlock(CB_DECLARE|CB_CONFIG|CB_UNDECLARE, 0, 0);
        if(!p) p = (void *) g;
        a->parentBlock = p;

    } else {
        a->lastArgv = (void *) argv;
        a->lastArgc = argc;
        // Just make it something.
        a->parentBlock = (void *) 1;
        FreeArgv(a);
    }


finish:

    CHECK(pthread_mutex_unlock(&g->mutex));

    return ret;
}


int qsBlock_config(struct QsBlock *block,
            int argc, const char * const *argv) {

    NotWorkerThread();

    return _qsBlock_config(block, argc, argv, false);
}


static int
_qsBlock_configV(struct QsBlock *block, va_list ap) {

    int argc = 0;
    char **argv = 0;
    char *str = 0;
    size_t slen = 0;
    char *s;

    va_list ap_c;
    va_copy(ap_c, ap);

    while((s = va_arg(ap_c, char *))) {
        ++argc;
        slen += strlen(s) + 1;
    }
    va_end(ap_c);
    slen += 1; // for the 0 string terminator at the end.

    str = calloc(1, slen);
    ASSERT(str, "calloc(1,%zu) failed", slen);
    argv = calloc(argc + 1, sizeof(*argv));
    ASSERT(argv, "calloc(%d,%zu) failed", argc, sizeof(*argv));

    int i = 0;

    while((s = va_arg(ap, char *))) {
        strcpy(str, s);
        argv[i++] = str;
        str += strlen(s) + 1;
    }
    // Testing the consistency of the mystical va_list:
    DASSERT(i == argc);
    va_end(ap);
    argv[i] = 0;

    return _qsBlock_config(block, argc,
            (const char * const *) argv, true);
}


int qsBlock_configVByName(struct QsGraph *g, const char *bname,
            ...) {

    NotWorkerThread();

    struct QsBlock *b = qsGraph_getBlock(g, bname);
    if(!b) {
        ERROR("Block \"%s\" not found", bname);
        return -1;
    }
    va_list ap;
    va_start(ap, bname);

    return _qsBlock_configV(b, ap);
}


int qsBlock_configV(struct QsBlock *block, ...) {

    NotWorkerThread();

    va_list ap;
    va_start(ap, block);

    return _qsBlock_configV(block, ap);
}


void qsAddConfig(char *(*config)(int argc, const char * const *argv, void *userData),
        const char *name, const char *desc,
        const char *argSyntax, const char *currentArgs) {

    NotWorkerThread();

    DASSERT(config);
    DASSERT(name && name[0]);
    DASSERT(desc && desc[0]);
    DASSERT(argSyntax && argSyntax[0]);
    DASSERT(currentArgs && currentArgs[0]);

    // This module could be a part of a super block or a simple block.
    //struct QsModule *m = GetModule(CB_DECLARE|CB_CONFIG, 0);
    struct QsModule *m = GetModule(CB_DECLARE, 0);

    // We are in declare() or config block callbacks so we already have
    // the graph mutex lock.

    if(!m->attributes)
        m->attributes = qsDictionaryCreate();

    struct QsAttribute *a = qsDictionaryFind(m->attributes, name);

    if(a)
        // If a block is stupid enough to create a new config with a name
        // that exists already we just remove the old one.
        ASSERT(0 == qsDictionaryRemove(m->attributes, name));

    a = calloc(1, sizeof(*a));
    ASSERT(a, "calloc(1,%zu) failed", sizeof(*a));
    a->config = config;

    struct QsDictionary *d;
    ASSERT(qsDictionaryInsert(m->attributes, name, a, &d) == 0);
    qsDictionarySetFreeValueOnDestroy(d, (void (*)(void *)) FreeAttribute);

    a->name = strdup(name);
    ASSERT(a->name, "strdup() failed");
    a->desc = strdup(desc);
    ASSERT(a->desc, "strdup() failed");
    a->argSyntax = strdup(argSyntax);
    ASSERT(a->argSyntax, "strdup() failed");
    a->currentArgs = strdup(currentArgs);
    ASSERT(a->currentArgs, "strdup() failed");
}
