#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <alloca.h>
#include <inttypes.h>
#include <dlfcn.h>
#include <setjmp.h>
#include <pthread.h>

#include "../include/quickstream/app.h"
#include "../include/quickstream/block.h"
#include "../include/quickstream/builder.h"

#include "debug.h"
#include "trigger.h"
#include "Dictionary.h"
#include "block.h"
#include "builder.h"
#include "graph.h"
#include "Dictionary.h"
#include "LoadDSOFromTmpFile.h"
#include "GET_BLOCK.h"



static
int FindHandle_cb(const char *blockName, struct QsBlock *b, void **dlhh) {

    if(b->dlhandle == *dlhh) {
        dlclose(*dlhh);
        *dlhh = 0;
        return 1; // We are done.
    }
    return 0; // keep looking
}


// pathRet must be free()ed, if it is not 0.  Failure or not.
//
// pathRet will be the path that the DSO originated in; not the temporary
// copy that we actually load, in the case where this a second time
// loading the same file.
//
void *GetDLHandle(const char *fileName, char **pathRet) {

    char *path = GetPluginPath(QS_BLOCK_PREFIX, fileName, ".so");

    if(pathRet)
        *pathRet = path;

    void *dlhandle = dlopen(path, RTLD_NOW | RTLD_LOCAL);

    if(!dlhandle) {
        WARN("dlopen(\"%s\",) failed: %s", path, dlerror());
        goto done;
    }

    ///////////////////////////////////////////////////////////////////
    // check if already dlopen()ed and fix
    ///////////////////////////////////////////////////////////////////

    // HERE check if this DSO is already loaded in any graphs.  We can't
    // load this DSO this way, if it's just getting functions for a block
    // that already exists.  It must be loaded as an independent code that
    // does not share addresses with symbols that are already loaded.
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
        //
        // TODO: maybe dlmopen() can do better?  I tried and it just
        // failed to do what I want.
        if((dlhandle = LoadDSOFromTmpFile(path)) == 0)
            goto done;

done:
    if(!pathRet)
        free(path);

    return dlhandle;
}


static void qsSimpleBlockUnload(struct QsSimpleBlock *b) {

    qsDictionaryDestroy(b->getters); // getters and constants
    qsDictionaryDestroy(b->setters);

    // We will assume that all the triggers are in the waiting list, so
    // the jobs list must be empty.
    DASSERT(b->firstJob == 0);
    DASSERT(b->lastJob == 0);

    CHECK(pthread_mutex_destroy(&b->mutex));

    struct QsTrigger *next;
    for(struct QsTrigger *t = b->waiting; t; t = next) {
        next = t->next;
        FreeTrigger(t);
    }

#ifdef DEBUG
    memset(b, 0, sizeof(*b));
#endif
}


static void qsSuperBlockUnload(struct QsSuperBlock *b) {

    // Super blocks do not have parameters.

#ifdef DEBUG
    memset(b, 0, sizeof(*b));
#endif
}


// This does not call the block module's destroy() function.
static
void qsBlockUnload_noDestory(struct QsBlock *b) {

    DSPEW("Freeing block named %s", b->name);

    if(b->dlhandle) dlclose(b->dlhandle);

    if(b->name) {
#ifdef DEBUG
        memset((char *)b->name, 0, strlen(b->name));
#endif
        free((void *) b->name);
    }

    {
        // Remove this block, b, from the doubly linked list of blocks
        // that is in the graph.
        struct QsGraph *g = b->graph;

        if(b->prev) {
            DASSERT(b != g->firstBlock);
            b->prev->next = b->next;
        } else {
            DASSERT(b == g->firstBlock);
            g->firstBlock = b->next;
            if(b->next)
                b->next->prev = 0;
        }

        if(b->next) {
            DASSERT(b != g->lastBlock);
            b->next->prev = b->prev;
        } else {
            DASSERT(b == g->lastBlock);
            g->lastBlock = b->prev;
            if(b->prev)
                b->prev->next = 0;
        }
    }

    if(b->isSuperBlock)
        qsSuperBlockUnload((struct QsSuperBlock *) b);
    else
        qsSimpleBlockUnload((struct QsSimpleBlock *) b);

    free(b);
}


struct QsBlock *qsGraphBlockLoad(struct QsGraph *graph, const char *fileName,
        const char *blockName_in) {

    // 1. Get unique block name
    // 2. dlopen()
    // 3. see if isSuperBlock is defined and Allocate
    // 4. Add block to graph's block Dictionary list
    // 5. Add this block to the graph doubly linked list of blocks.
    // 6. Call declare()
    // 7. Add cleanup callback for block's entry in graph block Dictionary
    // 8. Add built-in parameters
    // 9. strdup() fileName

    DASSERT(graph);
    ASSERT(mainThread == pthread_self(), "Not graph main thread");
    DASSERT(fileName);
    DASSERT(fileName[0]);
    ASSERT(graph->flowState == QsGraphPaused);

    ///////////////////////////////////////////////////////////////////
    // 1. Get unique block name
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
#define BLOCKS    (DIR_STR "blocks" DIR_STR)
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


    ///////////////////////////////////////////////////////////////////
    // 2. dlopen()
    ///////////////////////////////////////////////////////////////////

    char *path;
    void *dlhandle = GetDLHandle(fileName, &path);
    if(!dlhandle) {
        free(path);
        return 0;
    }


    ///////////////////////////////////////////////////////////////////
    // 3. see if isSuperBlock is defined and Allocate correct block type
    ///////////////////////////////////////////////////////////////////

    struct QsBlock *b;

    if(dlsym(dlhandle, "isSuperBlock")) {
        // SUPER BLOCK
        struct QsSuperBlock *suB = calloc(1, sizeof(*suB));
        ASSERT(suB, "calloc(1, %zu) failed", sizeof(*suB));
        b = &suB->block;
        b->isSuperBlock = true;
        b->graph = graph;
    } else {
        // SIMPLE BLOCK
        struct QsSimpleBlock *smB = calloc(1, sizeof(*smB));
        ASSERT(smB, "calloc(1, %zu) failed", sizeof(*smB));
        b = &smB->block;
        //b->isSuperBlock is false via calloc()
        b->graph = graph;

        // Add parameter dictionaries.
        smB->getters = qsDictionaryCreate(); // getters and constants
        smB->setters = qsDictionaryCreate();

        CHECK(pthread_mutex_init(&smB->mutex, 0));

        // Add this simple block to the default thread pool if there is
        // one.
        if(graph->threadPools)
            qsThreadPoolAddBlock(graph->threadPools, b);
    }

    b->dlhandle = dlhandle;
    b->name = strdup(blockName);
    ASSERT(b->name, "strdup() failed");
    b->inWhichCallback = _QS_IN_NONE;


    ///////////////////////////////////////////////////////////////////
    // 4. Add block to graph's block lists
    ///////////////////////////////////////////////////////////////////

    struct QsDictionary *entry = 0;
    int ret = qsDictionaryInsert(graph->blocks, blockName, b, &entry);
    ASSERT(ret == 0, "qsDictionaryInsert(,\"%s\",,) failed", blockName);
    DASSERT(entry);


    ///////////////////////////////////////////////////////////////////
    // 5. Add this block to the graph doubly linked list of blocks.
    ///////////////////////////////////////////////////////////////////

    // This list is used for calling start() in load order, and stop()
    // in reverse load order.
    if(graph->firstBlock) {
        DASSERT(graph->firstBlock->prev == 0);
        DASSERT(graph->lastBlock);
        DASSERT(graph->lastBlock->next == 0);

        // Add b to the end of the list:
        b->prev = graph->lastBlock;
        graph->lastBlock = b;
        b->prev->next = b;

    } else {
        // There are none in the list.
        DASSERT(!graph->lastBlock);
        graph->lastBlock = graph->firstBlock = b;
    }


    ///////////////////////////////////////////////////////////////////
    // 6. Call declare()
    ///////////////////////////////////////////////////////////////////

    int (*declare)(void) = dlsym(dlhandle, "declare");
    if(declare == 0) {
        ERROR("dlsym(, \"declare\") failed: %s", dlerror());
        free(path);
        qsDictionaryRemove(entry, blockName);
        qsBlockUnload_noDestory(b);
        return 0;
    }


    // Setup thread specfic data for declare call.
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
    b->inWhichCallback = _QS_IN_DECLARE;

    ret = declare();

    b->inWhichCallback = _QS_IN_NONE;

    CHECK(pthread_setspecific(_qsGraphKey, oldBlock));


    if(ret) {

        if(ret < 0) {
            ERROR("delcare() failed for block named \"%s\"", b->name);
            free(path);
            qsDictionaryRemove(entry, blockName);
            qsBlockUnload_noDestory(b);
            return 0;
        }
        // In this case we keep a block that is a struct in the program
        // but there will be no more calling callbacks from the DSO.
        DSPEW("declare() returned %d removing callbacks"
                " for block named \"%s\"",
                ret, b->name);
        dlclose(dlhandle);
        b->dlhandle = 0;
    }


    ///////////////////////////////////////////////////////////////////
    // 7. Add cleanup callback for block's entry in graph block list
    ///////////////////////////////////////////////////////////////////

    qsDictionarySetFreeValueOnDestroy(entry,
            (void (*)(void *)) qsBlockUnload);

    DSPEW("Loaded %s block named \"%s\" from path=%s",
            ((b->isSuperBlock)?"SUPER":"simple"),
            b->name, path);

    free(path);


    ///////////////////////////////////////////////////////////////////
    // 8. Add built-in parameters
    ///////////////////////////////////////////////////////////////////

    if(!b->isSuperBlock) {

        // This adds the option for builders to set the output maxWrite
        // for all output ports.  This can be overridden in the block
        // start().  The user sets the parameter "OutputMaxWrite" and that
        // is set for all output ports for the block before the start()
        // function is called; hence the start() can override it.
        size_t maxWrite = QS_DEFAULTMAXWRITE;
        if(qsParameterConstantCreate(b, "OutputMaxWrite", QsSize,
                    sizeof(maxWrite), 0 /*setCallback*/, b,
                    &maxWrite) == 0) {
            qsBlockUnload(b);
            b->inWhichCallback = 0;
            return 0;
        }
    }


    ///////////////////////////////////////////////////////////////////
    // 9. strdup(fileName);
    ///////////////////////////////////////////////////////////////////

    b->loadName = strdup(fileName);
    ASSERT(b->loadName, "strdup() failed");


    return (struct QsBlock *) b;
}


void qsBlockUnload(struct QsBlock *b) {

    DASSERT(b);
    DASSERT(b->name);
    DASSERT(b->graph);
    ASSERT(mainThread == pthread_self(), "Not graph main thread");
    ASSERT(b->graph->flowState == QsGraphPaused ||
            b->graph->flowState == QsGraphFailed);
    DASSERT(b->loadName);

#ifdef DEBUG
    memset((void *) b->loadName, 0, strlen(b->loadName));
#endif
    free((void *) b->loadName);


    // TODO: more super block stuff like unload all it's sub-blocks.
    // So, this function needs to be re-entrant.

    if(b->isSuperBlock == 0) {
        // This is a simple block.
        struct QsSimpleBlock *smB = (struct QsSimpleBlock *)b;
        // Disconnect all stream inputs and outputs
        uint32_t num = smB->numInputs;
        DASSERT(num == 0 || smB->inputs[num-1].feederBlock != 0);

        for(uint32_t i = 0; i < num; ++i) {

            if(smB->inputs[i].feederBlock == 0) {
                // This is an input that is not connected.  There has to
                // be an element in the array with a higher index, that is
                // not zeroed.
                continue;
            }
            // Note: we are editing this inputs array while iterating through
            // it, so we need keep qsBlockDisconnect() happy about that.
            qsBlockDisconnect(b, i);
        }
        // And that should have cleaned up all inputs.
        DASSERT(smB->numInputs == 0);
        DASSERT(smB->inputs == 0);

        // We remove all the outputs by removing all the inputs that
        // connect from them.  We have to search for them.
        for(struct QsBlock *bl = b->graph->firstBlock; bl; bl = bl->next) {
            if(bl->isSuperBlock || bl == b) continue;

            struct QsSimpleBlock *smBl = (struct QsSimpleBlock *) bl;
            if(smBl->inputs == 0) {
                DASSERT(smBl->numInputs == 0);
                continue;
            }

            for(uint32_t i = smBl->numInputs - 1; i != -1; --i)
                if(smBl->inputs[i].feederBlock == smB)
                    qsBlockDisconnect(bl, smBl->inputs[i].inputPortNum);
        }

        DASSERT(smB->numOutputs == 0);
        DASSERT(smB->outputs == 0);

        if(smB->passThroughs) {
            DASSERT(smB->numPassThroughs);
#ifdef DEBUG
            memset(smB->passThroughs, 0,
                    smB->numPassThroughs*sizeof(*smB->passThroughs));
#endif
            free(smB->passThroughs);
            smB->passThroughs = 0;
            smB->numPassThroughs = 0;
        }
    }

    if(b->dlhandle) {
        // The user has the option to put the destroy() in one
        // of two places:
        void (*destroy)(void) = dlsym(b->dlhandle, "destroy");
        if(b->runFileDLHandle && !destroy)
            destroy = dlsym(b->runFileDLHandle, "destroy");

        if(destroy) {
            // Setup thread specific data for declare() call.
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

    if(b->runFileDLHandle)
        dlclose(b->runFileDLHandle);
    if(b->runFilename) {
#ifdef DEBUG
        memset(b->runFilename, 0, strlen(b->runFilename));
#endif
        free(b->runFilename);
    }

    qsBlockUnload_noDestory(b);
}


const char* qsBlockGetName(const struct QsBlock *block) {

    if(block) return block->name;
    // This gets the block from pthread_setspecific(_qsGraphKey,block).
    return GetBlock()->name;
}
