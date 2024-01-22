#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <pthread.h>
#include <stdatomic.h>

#include "../include/quickstream.h"

#include "debug.h"
#include "Dictionary.h"
#include "name.h"

#include "c-rbtree.h"
#include "threadPool.h"
#include "block.h"
#include "graph.h"
#include "job.h"
#include "port.h"
#include "parameter.h"
#include "stream.h"
#include "config.h"

#include "graphConnect.h"



static inline void
PrintThreadPoolAssignment(const struct QsSimpleBlock *b, FILE *f) {

    DASSERT(b);
    DASSERT(b->jobsBlock.threadPool);
    DASSERT(b->jobsBlock.block.name);
    DASSERT(b->jobsBlock.threadPool->name);

    fprintf(f,
"threads-add %s %s\n"
        , b->jobsBlock.threadPool->name, b->jobsBlock.block.name);
}


static void
PrintThreadPoolAssignments(const struct QsParentBlock *p, FILE *f) {

    DASSERT(p);
    DASSERT(f);
    DASSERT(p->block.type & QS_TYPE_PARENT);

    for(struct QsBlock *b = p->firstChild; b; b = b->nextSibling) {
        if(b->type == QsBlockType_simple) {
            PrintThreadPoolAssignment((void *) b, f);
            continue;
        }
        DASSERT(b->type == QsBlockType_super);
        PrintThreadPoolAssignments((void *) b, f);
    }
}


static inline
int _qsGraph_save(const struct QsGraph *g,
        const char *path, const char *gpath_in,
        uint32_t flags) {

    DASSERT(g);
    DASSERT(g->name);
    DASSERT(g->name);
    DASSERT(g->threadPoolStack);
    DASSERT(g->threadPoolStack->name);

    char *gpath = (void *) gpath_in;
    size_t plen = strlen(path);

    char *dsoPath = strdup(path);
    ASSERT(dsoPath, "strdup() failed");
    if(plen >= 3 && strcmp(path + plen-2, ".c") == 0)
        // path ends in ".c"
        dsoPath[plen-2] = '\0';
    else if(plen >= 4 && strcmp(path + plen-3, ".so") == 0)
        // path ends in ".so"
        dsoPath[plen-3] = '\0';

    if(!gpath) {
        ASSERT(plen >= 2);
        gpath = strdup(path);
        ASSERT(gpath, "strdup() failed");
        if(plen >= 3 && strcmp(path + plen-2, ".c") == 0)
            // path ends in ".c"
            gpath[plen-2] = '\0';
        else if(plen >= 4 && strcmp(path + plen-3, ".so") == 0)
            // path ends in ".so"
            gpath[plen-3] = '\0';
        // Else path does not end in .c or .so
    }

    int ret = 0;

    if(access(gpath, F_OK) != -1) {
        ERROR("File \"%s\" exists", gpath);
        ret = -7;
        goto cleanup;
    }

    FILE *f = fopen(gpath, "w");
    if(!f) {
        ERROR("fopen(\"%s\",\"w\") failed", gpath);
        ret = -8;
        goto cleanup;
    }

    // TODO: This is UNIX specific: ':' in PATH thingy.
    //
    char *qs_block_path = getenv("QS_BLOCK_PATH");
    if(!qs_block_path)
        qs_block_path = "";


    fprintf(f,
"#!/usr/bin/env quickstream_interpreter\n"
"#\n"
"# This is a generated file\n"
"\n"
"set-block-path %s\n"
"graph %s %" PRIu32 " %s %s\n"
"############################################\n"
"# Create More Thread Pools\n"
"############################################\n"
        , qs_block_path,
        g->name, g->threadPoolStack->maxThreads,
        g->threadPoolStack->name, dsoPath);

    for(struct QsThreadPool *tp = g->threadPoolStack->next;
            tp; tp = tp->next) {
        DASSERT(tp->maxThreads);
        DASSERT(tp->name);
    fprintf(f,
"threads %" PRIu32 " %s\n"
        , tp->maxThreads, tp->name);
    }

    fprintf(f,
"############################################\n"
"# Assign Blocks to Thread Pools\n"
"############################################\n"
        );

    PrintThreadPoolAssignments((void *) g, f);


    fprintf(f,
"############################################\n"
"start\n"
"wait-for-stream\n"
"\n"
        );


    if(fchmod(fileno(f), 0755) != 0) {
        ERROR("fchmod(%d,0755) failed for file \"%s\"",
                fileno(f), gpath);
        ret = -9;
        goto cleanup;
    }

    INFO("Created file: \"%s\"", gpath);

cleanup:

    if(f)
        fclose(f);


    if(!gpath_in) {
        DZMEM(gpath, plen);
        free(gpath);
    }

    DZMEM(dsoPath, plen);
    free(dsoPath);

    return ret;
}


int qsGraph_save(const struct QsGraph *g,
        const char *path, const char *gpath_in,
        uint32_t flags) {

    NotWorkerThread();
    
    // This is a recursive mutex.
    CHECK(pthread_mutex_lock((void *) &g->mutex));

    int ret = qsGraph_saveSuperBlock(g, path, flags);
    if(ret) goto finish;

    ret = _qsGraph_save(g, path, gpath_in, flags);

finish:

    CHECK(pthread_mutex_unlock((void *) &g->mutex));

    return ret;
}
