#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <alloca.h>
#include <inttypes.h>
#include <pthread.h>

#include "../include/quickstream/builder.h"

#include "block.h"
#include "builder.h"
#include "app.h"
#include "Dictionary.h"
#include "debug.h"



struct QsBlock *qsAppBlockLoad(struct QsApp *app, const char *fileName,
        const char *blockName_in) {

    // 0. Get block name
    // 1. dlopen()
    // 2. check if already dlopen()ed and fix
    // 3. see if isSuperBlock is defined.
    // 4. Allocate the correct struct.
    // 5. add block to app
    // 6. Call bootscrap()

    DASSERT(app);
    ASSERT(app->mainThread == pthread_self(), "Not app main thread");
    DASSERT(fileName);
    DASSERT(fileName[0]);

    char *blockName = (char *) blockName_in;


    if(!blockName || blockName[0] == '\0') {

        // Generate a unique block name.
        //
        // len from:  fileName + '_2345' + '\0'
        size_t len = strlen(fileName) + 6;
        // Don't blow the stack.
        ASSERT(len < 1024*10);
        blockName = alloca(len);
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

        while(qsDictionaryFind(app->blocks, blockName)) {
           if(++count >= 1000) {
                ERROR("Can't get Block name \"%s\" is in use already",
                        blockName);
                return 0;
            }
           sprintf(ending, "_%" PRIu32, count);
        }

        DSPEW("Found unique block name \"%s\"", blockName);

    } else if(qsDictionaryFind(app->blocks, blockName)) {
        //
        // Because they requested a particular name and the name is
        // already taken, we can fail here.
        //
        ERROR("Block name \"%s\" is in use already", blockName);
        return 0;
    }

    // We now have what will be a valid block name.


    //char *path = GetPluginPath(QS_BLOCK_PREFIX, fileName, ".so");

    //int fd = dlopen(path, RTLD_NOW | RTLD_LOCAL);

    










    struct QsSimpleBlock *b = calloc(1, sizeof(*b));
    ASSERT(b, "calloc(1, %zu) failed", sizeof(*b));


    //free(path);

    return (struct QsBlock *) b;
}


static int qsSimpleBlockUnload(struct QsSimpleBlock *b) {

#ifdef DEBUG
    memset(b, 0, sizeof(*b));
#endif
    return 0;
}


static int qsSuperBlockUnload(struct QsSuperBlock *b) {

#ifdef DEBUG
    memset(b, 0, sizeof(*b));
#endif
    return 0;
}




int qsBlockUnload(struct QsBlock *b) {

    DASSERT(b);

    if(b->isSuperBlock)
        qsSuperBlockUnload((struct QsSuperBlock *) b);
    else
        qsSimpleBlockUnload((struct QsSimpleBlock *) b);

    free(b);
    return 0; // success
}

