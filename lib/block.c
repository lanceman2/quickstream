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


#if 0
static
int FindHandle_cb(const char *blockName, struct QsBlock *b, void **dlhh) {

    if(b->dlhandle == *dlhh) {
        *dlhh = 0;
        return 1; // We are done.
    }
    return 0; // keep looking
}
#endif

struct QsBlock *qsGraphBlockLoad(struct QsGraph *graph, const char *fileName,
        const char *blockName_in) {

    // 0. Get block name
    // 1. dlopen()
    // 2. check if already dlopen()ed and fix
    // 3. see if isSuperBlock is defined.
    // 4. Allocate the correct struct.
    // 5. add block to graph
    // 6. Call bootscrap()

    DASSERT(graph);
    ASSERT(mainThread == pthread_self(), "Not graph main thread");
    DASSERT(fileName);
    DASSERT(fileName[0]);

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

    } else if(qsDictionaryFind(graph->blocks, blockName)) {
        //
        // Because they requested a particular name and the name is
        // already taken, we can fail here.
        //
        ERROR("Block name \"%s\" is in use already", blockName);
        return 0;
    }

    // We now have what will be a valid block name.


    char *path = GetPluginPath(QS_BLOCK_PREFIX, fileName, ".so");

    void *dlhandle = dlopen(path, RTLD_NOW | RTLD_LOCAL);

    // See if this dlhandle has been created before in another block in
    // this graph.

    if(!dlhandle) {
        WARN("dlopen(\"%s\",) failed: %s", path, dlerror());
        free(path);
        return 0;
    }



    // TODO: HERE check that this DSO is not loaded in any graphs.


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
    struct QsDictionary *entry = 0;
    int ret = qsDictionaryInsert(graph->blocks, blockName, b, &entry);
    ASSERT(ret == 0, "qsDictionaryInsert(,\"%s\",,) failed", blockName);
    DASSERT(entry);
    qsDictionarySetFreeValueOnDestroy(entry,
            (void (*)(void *)) qsBlockUnload);

    DSPEW("Loaded %s block named \"%s\" from path=%s",
            ((b->isSuperBlock)?"SUPER":"simple"),
            b->name, path);

    free(path);

    return (struct QsBlock *) b;
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



void qsBlockUnload(struct QsBlock *b) {

    DASSERT(b);
    ASSERT(mainThread == pthread_self(), "Not graph main thread");


    DSPEW("Freeing block named %s", b->name);

    if(b->name) free((void *) b->name);

    if(b->dlhandle) dlclose(b->dlhandle);

    if(b->isSuperBlock)
        qsSuperBlockUnload((struct QsSuperBlock *) b);
    else
        qsSimpleBlockUnload((struct QsSimpleBlock *) b);

    free(b);
}

