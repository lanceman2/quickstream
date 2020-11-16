#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <alloca.h>
#include <inttypes.h>
#include <dlfcn.h>
#include <pthread.h>

#include "../include/quickstream/builder.h"

#include "block.h"
#include "builder.h"
#include "app.h"
#include "Dictionary.h"
#include "debug.h"
#include "LoadDSOFromTmpFile.h"

static
int FindHandle_cb(const char *blockName, struct QsBlock *b, void **dlhh) {

    if(b->dlhandle == *dlhh) {
        dlclose(*dlhh);
        *dlhh = 0;
        return 1; // We are done.
    }
    return 0; // keep looking
}


static void qsSimpleBlockUnload(struct QsSimpleBlock *b) {

#ifdef DEBUG
    memset(b, 0, sizeof(*b));
#endif
}


static void qsSuperBlockUnload(struct QsSuperBlock *b) {

#ifdef DEBUG
    memset(b, 0, sizeof(*b));
#endif
}


static
void qsBlockUnload_noDestory(struct QsBlock *b) {

    DASSERT(b);
    DASSERT(b->name);
    ASSERT(mainThread == pthread_self(), "Not graph main thread");

    DSPEW("Freeing block named %s", b->name);

    if(b->dlhandle) dlclose(b->dlhandle);

    if(b->name) {
#ifdef DEBUG
        memset((char *)b->name, 0, strlen(b->name));
#endif
        free((void *) b->name);
    }

    if(b->isSuperBlock)
        qsSuperBlockUnload((struct QsSuperBlock *) b);
    else
        qsSimpleBlockUnload((struct QsSimpleBlock *) b);

    free(b);
}


struct QsBlock *qsGraphBlockLoad(struct QsGraph *graph, const char *fileName,
        const char *blockName_in) {

    // 1. Get block name
    // 2. dlopen()
    // 3. check if already dlopen()ed and fix
    // 4. see if isSuperBlock is defined and Allocate
    // 5. add block to graph's block list
    // 6. Call bootscrap()
    // 7. Get callbacks
    // 8. Add cleanup callback for block's entry in graph block list

    DASSERT(graph);
    ASSERT(mainThread == pthread_self(), "Not graph main thread");
    DASSERT(fileName);
    DASSERT(fileName[0]);

    ///////////////////////////////////////////////////////////////////
    // 1. Get block name
    ///////////////////////////////////////////////////////////////////
 
    char *blockName = (char *) blockName_in;

    if(!blockName || blockName[0] == '\0') {

        // Generate a unique block name.
        //
        // alen from:  fileName + '_2345' + '\0'
        size_t len = strlen(fileName);
        size_t alen = len + 6;
        // Don't blow the stack.
        ASSERT(alen < 1024*10);
        blockName = alloca(alen);
        strcpy(blockName, fileName);
        // Take off '.so' on the end if there is one:
        // example blockName = "test/foo/filename.so"
        // example blockName = "/bar/foo/blocks/test/foo/filename.so"
        if(len > 3 && strcmp(blockName + len - 3, ".so") == 0)
            *(blockName + len - 3) = '\0';
        // example blockName = "test/foo/filename"
        // example blockName = "/bar/foo/blocks/test/foo/filename"

        // Strip off leading '/':
        while(*blockName == DIR_CHAR) ++blockName;

        {
            // Strip off the last "/blocks/" if there is one
#define BLOCKS    DIR_STR "blocks" DIR_STR
            char *s = blockName;
            size_t bl = strlen(BLOCKS);
            while(*s) {
                if(strcmp(s, BLOCKS) == 0)
                    blockName = s + bl;
                ++s;
            }
            // now example: blockName = "test/foo/filename"
        }
        {
            // Strip off the '/' before the last '/'
            char *s = blockName;
            char *prev = 0;
             while(*s) {
                if(*s == DIR_CHAR && *(s+1)) {
                    if(prev)
                        blockName = prev;
                    prev = s+1;
                }
                ++s;
            }
            // now example: blockName = "foo/filename"
        }

        uint32_t count = 1;
        char *ending = blockName + strlen(blockName);
        DASSERT(strlen(blockName));

        while(qsDictionaryFind(graph->blocks, blockName)) {
           if(++count >= 1000) {
                ERROR("Can't get Block name \"%s\" is in use already",
                        blockName);
                return 0;
            }
           sprintf(ending, "_%" PRIu32, count);
        }

        DSPEW("Found unique block name \"%s\"", blockName);

    } else {

        // TODO: We decided that block names are unique for all blocks in
        // a program.  This avoids introducing another name layer to
        // accessing block parameters.  For example a parameter named
        // "freq" in block named "tx" and program "my_transmitter" can
        // be accessed by a total name like "my_transmitter:tx:freq".
        // With graph names it'd be like:
        // "my_transmitter:graph_0:tx:freq", and that sucks.
        //
        for(struct QsGraph *g = graphs; g; g = g->next)
            if(qsDictionaryFind(g->blocks, blockName)) {
                //
                // Because they requested a particular name and the name
                // is already taken, we can fail here.
                //
                ERROR("Block name \"%s\" is in use already", blockName);
                return 0;
            }
    }

    // We now have what will be a valid block name.


    char *path = GetPluginPath(QS_BLOCK_PREFIX, fileName, ".so");


    ///////////////////////////////////////////////////////////////////
    // 2. dlopen()
    ///////////////////////////////////////////////////////////////////


    void *dlhandle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if(!dlhandle) {
        WARN("dlopen(\"%s\",) failed: %s", path, dlerror());
        free(path);
        return 0;
    }


    ///////////////////////////////////////////////////////////////////
    // 3. check if already dlopen()ed and fix
    ///////////////////////////////////////////////////////////////////

    // HERE check if this DSO is already loaded in any graphs.  We can't
    // load this DSO this way, if it's just getting functions for a block
    // that already exists.  It must be loaded as an independent code.
    for(struct QsGraph *g = graphs; g; g = g->next)
        qsDictionaryForEach(g->blocks,
            (int (*) (const char *key, void *value,
                void *userData)) FindHandle_cb, &dlhandle);

    if(dlhandle == 0)
        // This DSO was loaded already.  So we make a temporary copy of
        // the DSO and load it as a different set of independent functions
        // that do not share functions with a block that is already
        // loaded.  The temporary file is automatically removed by the OS
        // when this program exits.  This is a really cool trick.
        if((dlhandle = LoadDSOFromTmpFile(path)) == 0) {
            free(path);
            return 0;
        }


    ///////////////////////////////////////////////////////////////////
    // 4. see if isSuperBlock is defined and Allocate correct block type
    ///////////////////////////////////////////////////////////////////

    struct QsBlock *b;

    if(dlsym(dlhandle, "isSuperBlock")) {
        // SUPER BLOCK
        struct QsSuperBlock *suB = calloc(1, sizeof(*suB));
        ASSERT(suB, "calloc(1, %zu) failed", sizeof(*suB));
        b = &suB->block;
        b->isSuperBlock = true;
    } else {
        // SIMPLE BLOCK
        struct QsSimpleBlock *smB = calloc(1, sizeof(*smB));
        ASSERT(smB, "calloc(1, %zu) failed", sizeof(*smB));
        b = &smB->block;
        //b->isSuperBlock is false via calloc()
    }

    b->dlhandle = dlhandle;
    b->graph = graph;
    b->name = strdup(blockName);
    ASSERT(b->name, "strdup() failed");
    b->inWhichCallback = _QS_IN_NONE;

    ///////////////////////////////////////////////////////////////////
    // 5. add block to graph
    ///////////////////////////////////////////////////////////////////

    struct QsDictionary *entry = 0;
    int ret = qsDictionaryInsert(graph->blocks, blockName, b, &entry);
    ASSERT(ret == 0, "qsDictionaryInsert(,\"%s\",,) failed", blockName);
    DASSERT(entry);


    ///////////////////////////////////////////////////////////////////
    // 6. Call bootscrap()
    ///////////////////////////////////////////////////////////////////

    int (*bootstrap)(struct QsGraph *graph) = dlsym(dlhandle, "bootstrap");
    if(bootstrap == 0) {
        ERROR("dlsym(, \"bootstrap\") failed: %s", dlerror());
        free(path);
        qsDictionaryRemove(entry, blockName);
        qsBlockUnload_noDestory(b);
        return 0;
    }

    // Setup thread specfic data for bootstrap call.
    // And Make this re-entrant code.
    //
    struct QsBlock *oldBlock = pthread_getspecific(_qsGraphKey);
    //
    DASSERT(oldBlock != b);
    //
    // Set the thread specific data to the block structure.
    CHECK(pthread_setspecific(_qsGraphKey, b));

    DASSERT(b->inWhichCallback == _QS_IN_NONE);

    // We add a marker to block struct so we know what block callback
    // function is being called.
    //
    b->inWhichCallback = _QS_IN_BOOTSTRAP;

    ret = bootstrap(graph);

    b->inWhichCallback = _QS_IN_NONE;

    CHECK(pthread_setspecific(_qsGraphKey, oldBlock));


    if(ret) {

        if(ret < 0) {
            ERROR("bootstrap() failed for block named \"%s\"", b->name);
            free(path);
            qsDictionaryRemove(entry, blockName);
            qsBlockUnload_noDestory(b);
            return 0;
        }
        // In this case we keep a block that is a struct in the program
        // but there will be no more calling callbacks from the DSO.
        DSPEW("bootstrap() returned %d removing callbacks"
                " for block named \"%s\"",
                ret, b->name);
        dlclose(dlhandle);
        b->dlhandle = 0;
    }

    ///////////////////////////////////////////////////////////////////
    // 7. Get callbacks
    ///////////////////////////////////////////////////////////////////

    if(!b->isSuperBlock) {
        ((struct QsSimpleBlock *) b)->flow = dlsym(dlhandle, "flow");
        ((struct QsSimpleBlock *) b)->flush = dlsym(dlhandle, "flush");
    }
    b->start = dlsym(dlhandle, "start");
    b->stop = dlsym(dlhandle, "stop");


    ///////////////////////////////////////////////////////////////////
    // 8. Add cleanup callback for block's entry in graph block list
    ///////////////////////////////////////////////////////////////////

    qsDictionarySetFreeValueOnDestroy(entry,
            (void (*)(void *)) qsBlockUnload);

    DSPEW("Loaded %s block named \"%s\" from path=%s",
            ((b->isSuperBlock)?"SUPER":"simple"),
            b->name, path);


    free(path);

    return (struct QsBlock *) b;
}


void qsBlockUnload(struct QsBlock *b) {

    if(b->dlhandle) {
        void (*destroy)(void) = dlsym(b->dlhandle, "destroy");

        if(destroy) {
            // Setup thread specfic data for bootstrap call.
            // And Make this re-entrant code.
            //
            struct QsBlock *oldBlock = pthread_getspecific(_qsGraphKey);
            //
            DASSERT(oldBlock != b);
            //
            // Set the thread specific data to the block structure.
            CHECK(pthread_setspecific(_qsGraphKey, b));

            // A block cannot call destroy() on itself.
            DASSERT(b->inWhichCallback == _QS_IN_NONE);

            // We add a marker to block struct so we know what block
            // callback function is being called.
            //
            b->inWhichCallback = _QS_IN_DESTROY;

            destroy();

            b->inWhichCallback = _QS_IN_NONE;

            CHECK(pthread_setspecific(_qsGraphKey, oldBlock));
        }
    }


    qsBlockUnload_noDestory(b);
}
