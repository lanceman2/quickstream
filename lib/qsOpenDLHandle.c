#define _GNU_SOURCE
#include <link.h>

#include <inttypes.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
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


// This adds finding the DSO file in the same directory as the
// as the DSO block that is calling this.  This ASSERTs if
// the DSO file cannot be loaded.
//
// Note: This loads the same DSO for a given file path, not like when
// loading a block with the QsBlockOptions::disableDSOLoadCopy not set;
// that is regular (the default) block loading which loads a copy.
void *qsOpenRelativeDLHandle(const char *relpath) {

    ASSERT(relpath);
    ASSERT(relpath[0]);
    struct QsSimpleBlock *b = GetBlock(CB_ANY, 0, QsBlockType_simple);
    DASSERT(b);
    DASSERT(b->module.fullPath);
    // This must be a full path.
    // TODO: Linux specific code:
    DASSERT(b->module.fullPath[0] == '/');
    DASSERT(b->module.fullPath[1]);
     size_t fplen = strlen(b->module.fullPath);
    size_t len = fplen + strlen(relpath) + 2;
    char *dsoPath = calloc(1, len);
    ASSERT(dsoPath, "calloc(1,%zu) failed", len);

    strcpy(dsoPath, b->module.fullPath);

    char *s = dsoPath + fplen;
    while(*s != '/') --s;
    DASSERT(*s == '/');
    ++s;
    *s = '\0';
    strcat(dsoPath, relpath);

    void *dlhandle = dlopen(dsoPath, QS_MODULE_DLOPEN_FLAGS);

    ASSERT(dlhandle, "dlopen(\"%s\",%d) failed",
            dsoPath, QS_MODULE_DLOPEN_FLAGS);

    DSPEW("Block \"%s\" dlopen()ed \"%s\"",
            b->jobsBlock.block.name, dsoPath);

    DZMEM(dsoPath, strlen(dsoPath));
    free(dsoPath);

    return dlhandle;
}


// Very simple wrapper.
void *qsGetDLSymbol(void *dlhandle, const char *symbol) {
    ASSERT(dlhandle);
    void *ret = dlsym(dlhandle, symbol);
    // We are assuming it's not a symbol that is null.
    ASSERT(ret, "dlsym(%p,\"%s\") failed: %s",
            dlhandle, symbol, dlerror());

    return ret;
}

// Very simple wrapper.
void qsCloseDLHandle(void *dlhandle) {

    ASSERT(dlclose(dlhandle) == 0, "dlclose(%p) failed", dlhandle);
}
