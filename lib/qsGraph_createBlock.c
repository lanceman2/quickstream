#include <inttypes.h>
#include <dlfcn.h>
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
#include "port.h"
#include "FindFullPath.h"
#include "LoadDSOFromTmpFile.h"
#include "stream.h"

#include "./builtInBlocks/builtInBlocks.h"


// Finds the function (symbol) name and then the pointer to the function
// or struct pointer.
//
void *GetBlockSymbol(void *dlhandle,
        const char *prefix, const char *symbolName) {

    if(!dlhandle && !prefix)
        // We use this case in order to not call a block module
        // callback function.
        return 0;

    if(!dlhandle && prefix) {
        // This is a built-in block module because prefix is set
        // and dlhandle is not.
        //
        // There may still be no pointer to find.
        DASSERT(prefix[0]);
        DASSERT(builtInBlocksFunctions);
        char name[MAX_BUILTIN_SYMBOL_LEN];
        snprintf(name, MAX_BUILTIN_SYMBOL_LEN, "%s_%s",
                prefix, symbolName);
        // This will return 0 if it is not found.
        return qsDictionaryFind(builtInBlocksFunctions, name);
    }

    // dlsym() will return 0 if it is not found and that's okay.
    return dlsym(dlhandle, symbolName);
}


// Returns the built-in module prefix, as we define it here; or 0 if this
// is not the fileName of a built-in module, as we define it here.
//
// Returns malloc() allocated memory that must be free()ed, or 0.
//
static inline char *CheckForBuiltInModule(const char *fileName) {

    DASSERT(fileName);
    DASSERT(*fileName);

#define PLEN  (MAX_BUILTIN_SYMBOL_LEN + 5) // + .xxx with /0

    char *prefix = 0;
    // The prefix is the basename of fileName with any suffix removed.  We
    // limit its length and that's okay given we control all the prefixes
    // that are possible in the projects source code.
    char name[PLEN]; // ".so" == + 3
    // Go to the end of string.
    // It's okay we never change the string fileName.
    // prefix is just a dummy pointer.
    prefix = (char *) ( fileName + strlen(fileName) - 1);
    // Example: fileName = "foo/bar/module.xxx" or "module.xxx" or
    // "module"
    //
    while(prefix != fileName && *prefix != DIRCHR) --prefix;
    if(*prefix == DIRCHR) ++prefix;
    // Now prefix = "module.xxx"
    int ret = snprintf(name, PLEN, "%s", prefix);

    if(ret >= PLEN-1)
        return 0;

    // Now name = "module.so" or "module.dll"  "module.xxx"
    // So strip off the ".xxx" suffix, if present.
    // We are assuming that built-in modules do not have '.' in the symbol
    // names.
    prefix = name;
    while(*prefix && *prefix != '.') ++prefix;
    if(*prefix == '.') *prefix = '\0';
    // Now name = "module"

    // Now check if there is a built-in block module with this prefix.
    //
    // We require there be a "PREFIX_declare()" function for a built-in
    // module to exist.  TODO: maybe "PREFIX_declare()" function is not
    // required, but I don't see how yet.
    //
    if(GetBlockSymbol(0, name, "declare")) {
        prefix = strdup(name);
        ASSERT(prefix, "strdup() failed");
        DSPEW("Block prefix=%s", prefix);
        return prefix;
    }

    return 0;
}


static
int FindHandle_cb(const char *blockName, struct QsBlock *b,
        void **dlhh) {

    if(!(b->type & QS_TYPE_MODULE)) return 0;

    if(b->type == QsBlockType_super) {
        if(((struct QsSuperBlock *)b)->module.dlhandle == *dlhh)
            goto done;
        return 0;
    }

    if(b->type == QsBlockType_simple) {
        if(((struct QsSimpleBlock *)b)->module.dlhandle == *dlhh)
            goto done;
        if(((struct QsSimpleBlock *)b)->runFileDlhandle == *dlhh)
            goto done;
    }

    return 0; // keep looking

done:

    // This DSO was loaded already and we remove this handle reference
    // count by calling dlclose().
    dlclose(*dlhh);
    *dlhh = 0;

    return 1; // done looking
}

static
int GraphFindHandle_cb(const char *graphName, struct QsGraph *g,
        void **dlhh) {

    DASSERT(g);
    DASSERT(g->blocks);

    qsDictionaryForEach(g->blocks,
            (int (*) (const char *key, void *value,
                void *userData)) FindHandle_cb, dlhh);

    if(*dlhh == 0)
        // We found it and we are done.
        return 1;

    return 0; // keep looking.
}


// This will load a copy if this DSO if it is already loaded;
// unless loadCopy is false or disableDSOLoadCopy is set in
// the DSO "options".
//
// We must have a graph mutex to call this.
static
void *GetDLHandle(const char *path, bool loadCopy) {

    DASSERT(path);

    void *dlhandle = dlopen(path, QS_MODULE_DLOPEN_FLAGS);

    if(!dlhandle) {
        WARN("dlopen(\"%s\",) failed: %s", path, dlerror());
        return 0;
    }

    if(!loadCopy)
        // This block has opted out of loading unique copy of the
        // DSO in the running program; so the block code needs to know
        // it's sharing all it's static global state variables with all
        // the loaded blocks that have the same DSO file, like a regular
        // DSO library.
        return dlhandle;

    struct QsBlockOptions *opt = dlsym(dlhandle, "options");
    if(opt && opt->disableDSOLoadCopy) {
        // This block has opted out of loading unique copy of the
        // DSO in the running program; so the block code needs to know
        // it's sharing all it's static global state variables with all
        // the loaded blocks that have the same DSO file, like a regular
        // DSO library.
        return dlhandle;
    }


    ///////////////////////////////////////////////////////////////////
    // check if already dlopen()ed and fix if we need to.
    ///////////////////////////////////////////////////////////////////

    // HERE check if this DSO is already loaded in any graphs.  We can't
    // load this DSO this way, if it's just getting functions for a block
    // that already exists.  It must be loaded as an independent code that
    // does not share addresses with symbols that are already loaded.
    //
    CHECK(pthread_mutex_lock(&gmutex));

    // We have to search dlhandles in all graphs, because there can be any
    // number of graphs per process, and the blocks that are loaded from
    // DSOs create dl (dynamic linker/loader) objects (all corresponding
    // dlhandles) that map variables that are seen process wide.
    //
    qsDictionaryForEach(graphs,
            (int (*) (const char *key, void *value,
                void *userData)) GraphFindHandle_cb, &dlhandle);

    CHECK(pthread_mutex_unlock(&gmutex));


    if(dlhandle == 0) {

        // We make a temporary copy of the DSO and load it as a different
        // set of independent functions that do not share functions (any
        // symbols) with a block that is already loaded.  The temporary
        // file is automatically removed by the OS when this program
        // exits.  This is a really cool trick.  Using dlmopen() will only
        // let us load 16 copies of a DSO; where this loading a copy has
        // no such coded limit.
        //
        // Ya, 16 is likely enough, but we figured out this solution
        // before we found dlmopen().
        //
        // man dlmopen shows:
        // The glibc implementation supports a maximum of 16 namespaces.
        //
        dlhandle = LoadDSOFromTmpFile(path);
    }

    return dlhandle;
}


// Try, just once, to get the dlhandle for the run file.
//
// If getting the run file dlhandle ever failed return 0
// the next and all other times this is called with this
// block.
//
// If there is no run file set return the regular dlhandle;
// and the "run file" dlhandle is the regular dlhandle.
//
void *GetRunFileDlhandle(struct QsSimpleBlock *b) {

    DASSERT(qsBlockDir);

    if(!b->runFile) {
        DASSERT(!b->runFileDlhandle);
        DASSERT(!b->runFileFailed);
        // There is no separate run file.
        return b->module.dlhandle;
    }

    if(b->runFileDlhandle) {
        // We already got it.
        DASSERT(b->runFile);
        return b->runFileDlhandle;
    }

    if(b->runFileFailed) {
        // We already tried and failed.
        DASSERT(b->runFile);
        DASSERT(!b->runFileDlhandle);
        return 0;
    }

    // Try to dlopen() the run file, b->runFile.
    //
    void *dlhandle = 0;
    char *fullPath = 0;

    ASSERT(b->module.dlhandle, "Write code to support run files for "
                "built-in blocks");
    DASSERT(b->module.fullPath);
    DASSERT(b->module.fullPath[0]);

    // We look in the directory where the block DSO was loaded from.
    {
        // New scope for 3 dummy variables.
        size_t len = strlen(b->module.fullPath);
        char *dir = strdup(b->module.fullPath);
        char *s = dir + len - 1;
        while(s != dir && *s != DIRCHR) --s;
        DASSERT(*s == DIRCHR);
        *s = '\0';
        fullPath = FindFullPath(b->runFile, dir, ".so",
                0/* getenv("QS_BLOCK_PATH") not in this case*/);
#ifdef DEBUG
        memset(dir, 0, len);
#endif
        free(dir);
    }

    if(fullPath)
        // This will load a copy if this DSO if it is already loaded;
        // unless runFileLoadCopy is set (for simple blocks).
        dlhandle = GetDLHandle(fullPath, b->runFileLoadCopy);

    if(!dlhandle) {
        ERROR("Failed to load DSO from path \"%s\"",
                    fullPath?fullPath:b->runFile);
        FreeString(&fullPath);
        b->runFileFailed = true;
        return 0;
    }

    DSPEW("Opened quickstream run DSO (dynamic "
            "shared object) file: \"%s\"", fullPath);

    FreeString(&fullPath);

    return (b->runFileDlhandle = dlhandle);
}


// This is not thread safe.  It requires there be a master thread.
//
const char *qsBlock_getName(const struct QsBlock *b) {

    NotWorkerThread();
    ASSERT(b);
    DASSERT(b->graph);

    // TODO: Lock is kind-of pointless.
    CHECK(pthread_mutex_lock(&b->graph->mutex));

    const char *name = b->name;
    DASSERT(name);

    CHECK(pthread_mutex_unlock(&b->graph->mutex));

    return name;
}



static inline
int _qsBlock_destroy(struct QsBlock *b, bool callUndeclare) {

    NotWorkerThread();
    DASSERT(b);
    struct QsGraph *g = b->graph;
    DASSERT(g);

    // g->mutex is recursive.
    CHECK(pthread_mutex_lock(&g->mutex));

    // TODO: is it necessary to stop the streams?
    //
    // Maybe just if the block has a stream somewhere in it.
    if(g->runningStreams)
        qsGraph_stop(g);


    DASSERT(b->type == QsBlockType_simple ||
            b->type == QsBlockType_super);

    struct QsModule *m;

    if(b->type == QsBlockType_simple)
        m = &((struct QsSimpleBlock *)b)->module;
    else {
        DASSERT(b->type == QsBlockType_super);
        m = &((struct QsSuperBlock *) b)->module;
    }

    if(b->type & QS_TYPE_PARENT) {
        // Destroy all my children before I destroy myself.
        struct QsParentBlock *p = (struct QsParentBlock *) b;
        while(p->lastChild)
            qsBlock_destroy(p->lastChild);
    }

    DSPEW("Destroying block: %s \"%s\"", m->fileName, b->name);


    if(b->type == QsBlockType_simple &&
            ((struct QsSimpleBlock *)b)->startChecked) {
        struct QsSimpleBlock *sb = (void *) b;
        int (*destroy)(void *) = 0;

        if(sb->module.dlhandle == 0)
            // This is a built-in module.
            destroy = GetBlockSymbol(0, m->fileName, "destroy");
        else {
            // If there is a run file we must get the symbol from the
            // run file and not the sb->module.dlhandle.
            //
            void *dlhandle = GetRunFileDlhandle(sb);
            destroy = GetBlockSymbol(dlhandle, m->fileName, "destroy");
        }

        // Call destroy() if there is one.
        if(destroy) {
            // This code is reentrant.
            struct QsWhichBlock stackSave;
            SetBlockCallback(b, CB_DESTROY, &stackSave);
            int ret = destroy(b->userData);
            RestoreBlockCallback(&stackSave);
            if(ret <= -1)
                ERROR("Block \"%s\" destroy failed (ret=%d)",
                        b->name, ret);
            else if(ret >= 1)
                INFO("Block \"%s\" destroy returned %d",
                        b->name, ret);
        }
    }

    int ret = 0;

    if(callUndeclare) {
        int (*undeclare)(void *) =
            GetBlockSymbol(m->dlhandle, m->fileName, "undeclare");

        if(undeclare) {
            // This code is reentrant.
            struct QsWhichBlock stackSave;
            SetBlockCallback(b, CB_UNDECLARE, &stackSave);
            ret = undeclare(b->userData);
            RestoreBlockCallback(&stackSave);
            if(ret <= -1)
                ERROR("Block \"%s\" undeclare failed (ret=%d)",
                        b->name, ret);
            else if(ret >= 1)
                INFO("Block \"%s\" undeclare returned %d",
                        b->name, ret);
        }
    }


    switch(b->type) {
        case QsBlockType_simple:
        {
            struct QsSimpleBlock *sb = (void *) b;
            if(sb->streamJob)
                DestroyStreamJob(sb, sb->streamJob);
            if(sb->runFile) {
#ifdef DEBUG
                memset(sb->runFile, 0, strlen(sb->runFile));
#endif
                free(sb->runFile);
            }
            if(sb->runFileDlhandle)
                ASSERT(dlclose(sb->runFileDlhandle) == 0);
        }
            break;
        case QsBlockType_super:
            break;
        default:
            ASSERT(0);
    }


    if(m->attributes)
        qsDictionaryDestroy(m->attributes);

    qsDictionaryDestroy(m->ports.getters); 
    qsDictionaryDestroy(m->ports.setters); 
    qsDictionaryDestroy(m->ports.inputs); 
    qsDictionaryDestroy(m->ports.outputs); 


#ifdef DEBUG
    // Save type so we can zero the memory after qsBlock_cleanup(b).
    enum QsBlockType type = b->type;
#endif

    qsBlock_cleanup(b);


    if(m->dlhandle) {
        ASSERT(dlclose(m->dlhandle) == 0);
        m->dlhandle = 0;
    }

    FreeString(&m->fileName);
    FreeString(&m->fullPath);


#ifdef DEBUG
    switch(type) {
        case QsBlockType_simple:
            DZMEM(b, sizeof(struct QsSimpleBlock));
            break;
        case QsBlockType_super:
            DZMEM(b, sizeof(struct QsSuperBlock));
            break;
        default:
            ASSERT(0);
    }
#endif

    free(b);

    CHECK(pthread_mutex_unlock(&g->mutex));

    return ret;
}


int qsBlock_destroy(struct QsBlock *b) {
    return _qsBlock_destroy(b, true);
}


struct QsBlock *qsGraph_createBlock(struct QsGraph *g,
        struct QsThreadPool *tp,
        const char *fileName_in, const char *name_in,
        void *userData) {

    NotWorkerThread();

    struct QsBlock *parent = GetBlock(CB_DECLARE|CB_CONFIG,
            QS_TYPE_MODULE, 0);
    if(parent) {
        ASSERT(parent->type == QsBlockType_super,
                "Block \"%s\" is not a super block",
                parent->name);
        g = parent->graph;
    }

    DASSERT(g);
    ASSERT(fileName_in);
    ASSERT(fileName_in[0]);
    if(name_in && !name_in[0])
        name_in = 0;

    // g->mutex is recursive.
    CHECK(pthread_mutex_lock(&g->mutex));

    //
    // This is the order of things for the rest of this function:
    //
    /////////////////////////////////////////////////////////////////////
    //
    // 1. Get a per graph unique name for the block.
    //
    // Try to load it as a DSO:
    //
    // 2a. Find the file path to the DSO (dynamic shared object), dlopen()
    //     it or fail.  If it is dlopened already dlclose() it and make a
    //     copy and ldopen() the copy, and remove the copy so no other
    //     process may see it.
    //
    // Or else it's a built-in :
    //
    // 2b. Check to see if this is a built-in block, and if it is get and
    //     set the block's callback prefix to fileName.
    //
    // Or else:
    //
    // 2c. Fail and return 0.
    //
    // 3. Figure out what kind of block this is making.
    //
    // 4. Allocate block structure and get and set block options.
    //
    // 5. Add this block the graph's parent/child block tree.
    //
    // 6. Add block to graph block Dictionaries.
    //
    // 7. Set block name.
    //
    // 8. Set the block's thread pool.
    //
    // 9. Setup the rest of struct values.
    //
    // 10. Add built-in parameters and stuff like that.
    //
    // 11. If there is a block declare() function call it.
    //     Note: this declare() for a super block may recurse and call
    //     this function again creating child blocks.  So this function
    //     must be re-entrant in addition to being thread-safe.
    //     We set threadspecific blockKey data before calling declare().
    //
    /////////////////////////////////////////////////////////////////////


    /////////////////////////////////////////////////////////////////////
    // 1. Get a per graph unique name for the block.
    /////////////////////////////////////////////////////////////////////

    char *name = 0;

    // Find surname from a parent block if a super block declare or
    // configure block callback function called this.
    //
    if(parent) {

        // There is a super block loading this block.
        //
        // Super blocks are not allowed to set thread pools of blocks
        // they load.
        tp = 0;
        // Super blocks are not allowed to set userData of blocks they
        // load.
        userData = 0;

        // We have some standards that super blocks must obey or
        // we just fail the program with an assert; it's a coding
        // error that we cannot run the program with.
        //
        // We are creating a child block of a super block (parent).
        DASSERT(parent->name);
        ASSERT(parent->type & QS_TYPE_PARENT,
                "Block \"%s\" is not a parent block", parent->name);
        // Yes.  There is a parent block calling this.
        // We need super blocks to name their children so that the
        // end user gets a description of the blocks.
        ASSERT(name_in,
                "Block \"%s\" must give a name to load a block",
                parent->name);
        ASSERT(ValidNameString(name_in), "\"%s\" is an invalid block name",
                name_in);
        size_t len = strlen(parent->name) + strlen(name_in) + 2;
        char *fullName = malloc(len);
        ASSERT(fullName, "malloc(%zu) failed", len);
        strcpy(fullName, parent->name);
        strcat(fullName, ":");
        strcat(fullName, name_in);
        // We assert if a super block stupidly tries to load two
        // blocks with the same name.  Shit makes more shit.
        ASSERT(!qsDictionaryFind(g->blocks, fullName),
                "The block name \"%s\" is already in use",
                fullName);
        name = fullName;
    } else {
        // This new block will be a child of the top block, graph.
        if(name_in) {
            if(!qsDictionaryFind(g->blocks, name_in)) {
                name = strdup(name_in);
                ASSERT(name, "strdup() failed");
            } else
                ERROR("Block named \"%s\" already exists", name_in);
        } else {
            DASSERT(g->blocks);
            // Generate a name from fileName_in.
            uint32_t count = 1;
            const char *basename = fileName_in + strlen(fileName_in);
            // TODO: UNIX specific code.
            while(*basename != '/' && basename != fileName_in)
                --basename;
            if(*basename == '/')
                ++basename;

            const size_t len = strlen(basename) + 12;
            name = calloc(1, len);
            ASSERT(name, "calloc(1,%zu) failed", len);
            int l = snprintf(name, len, "%s", basename);
            DASSERT(l > 0);
            // Remove ".so" if any.
            if(l > 3 && strcmp(name + l - 3, ".so") == 0) {
                name[l - 3] = '\0';
#ifdef DEBUG
                // Zero the end memory for the DEBUG case.
                name[l - 2] = '\0';
                name[l - 1] = '\0';
#endif
                l -= 3;
            }
            // Find a unique name.
            while(qsDictionaryFind(g->blocks, name))
                snprintf(name, len, "%*.*s_%" PRIu32, l, l,
                        basename, ++count);
        }
        if(!name) {
            CHECK(pthread_mutex_unlock(&g->mutex));
            // Fail.
            return 0;
        }
    }

    // After this point name must be used or free()ed.

    /////////////////////////////////////////////////////////////////////
    // 2a. Find the file path to the DSO (dynamic shared object), dlopen()
    //     it or fail.  If it is dlopened already dlclose() it and make a
    //     copy and ldopen() the copy, and remove the copy so no other
    //     process may see it.
    /////////////////////////////////////////////////////////////////////

    char *fullPath = 0;
    char *fileName = 0;
    void *dlhandle = 0;

    DASSERT(qsBlockDir);
    fullPath = FindFullPath(fileName_in, qsBlockDir, ".so",
            getenv("QS_BLOCK_PATH"));

    // Make sure that we do not load the same path as the parent's path.
    // That would recurse forever.
    if(parent && fullPath) {
        DASSERT(parent->type == QsBlockType_super);
        struct QsSuperBlock *sb = (struct QsSuperBlock *) parent;
        if(sb->module.fullPath && strcmp(fullPath, sb->module.fullPath) == 0) {
            ERROR("A super block cannot load itself. path=\"%s\"", fullPath);
            FreeString(&fullPath);
            FreeString(&name);
            return 0;
        }
    }

    if(fullPath)
        dlhandle = GetDLHandle(fullPath, true);
    if(!dlhandle)
        FreeString(&fullPath);

    /////////////////////////////////////////////////////////////////////
    // 2b. Check to see if this is a built-in block, and if it is get set
    //     the block's callback prefix to fileName.
    /////////////////////////////////////////////////////////////////////

    if(!dlhandle) {
        // This is a built-in block or it's a failure.
        fileName = CheckForBuiltInModule(fileName_in);
        if(parent && fileName) {
            DASSERT(parent->type == QsBlockType_super);
            struct QsSuperBlock *sb = (struct QsSuperBlock *) parent;
            DASSERT(sb->module.fileName);
            if(!sb->module.fullPath &&
                    strcmp(fileName, sb->module.fileName) == 0) {
                ERROR("A built-in super block cannot load "
                        "itself. fileName=\"%s\"", fileName);
                FreeString(&fileName);
                FreeString(&name);
                return 0;
            }
        }
    } else {
        fileName = strdup(fileName_in);
        ASSERT(fileName, "strdup() failed");
    }

    /////////////////////////////////////////////////////////////////////
    // 2c. Fail and return 0.
    /////////////////////////////////////////////////////////////////////

    if(!dlhandle && !fileName) {
        // Cannot load a DSO and cannot find a built-in.
        FreeString(&name);
        ERROR("Loading block %s failed", fileName_in);
        CHECK(pthread_mutex_unlock(&g->mutex));
        return 0;
    }

    /////////////////////////////////////////////////////////////////////
    // 3. Figure out what kind of block this is making.
    /////////////////////////////////////////////////////////////////////

    // Start with the default block type defined here and only here;
    enum QsBlockType type = QsBlockType_simple;
    {
        // Now look at the block module and see if they override this
        // default type.
        const struct QsBlockOptions *options = GetBlockSymbol(
                dlhandle, fileName, "options");
        // Note: valid block types are non-zero.
        if(options && options->type) {
            type = options->type;
            ASSERT(type == QsBlockType_super ||
                    type == QsBlockType_simple);
            DASSERT(type);
        }
    }

    /////////////////////////////////////////////////////////////////////
    // 4. Allocate block structure and get and set block options.
    /////////////////////////////////////////////////////////////////////

    // We are making a super block or a simple block.
    size_t size;
    switch (type) {
        case QsBlockType_simple:
            size = sizeof(struct QsSimpleBlock);
            break;
        case QsBlockType_super:
            size = sizeof(struct QsSuperBlock);
            break;
        default:
            ASSERT(0);
    }

    struct QsBlock *b = calloc(1, size);
    ASSERT(b, "calloc(1,%zu) failed", size);

    /////////////////////////////////////////////////////////////////////
    // 5. Add this block the graph's parent/child block tree.
    //
    // 6. Add block to graph block Dictionaries.
    //
    // 7. Set block name.
    //
    // 8. Set the block's thread pool.
    /////////////////////////////////////////////////////////////////////

    qsBlock_init(g, b, parent, tp, type, name, userData);
    // name was strdup()ed in qsBlock_init() so free it now.
    FreeString(&name);

    /////////////////////////////////////////////////////////////////////
    // 9. Setup the rest of struct values.
    /////////////////////////////////////////////////////////////////////

    struct QsModule *m;

    if(type == QsBlockType_simple)
        m = &((struct QsSimpleBlock *)b)->module;
    else {
        DASSERT(type == QsBlockType_super);
        m = &((struct QsSuperBlock *) b)->module;
    }
    m->dlhandle = dlhandle;
    m->fileName = fileName;
    m->fullPath = fullPath;

    /////////////////////////////////////////////////////////////////////
    // 10. Add built-in parameters and stuff like that.
    /////////////////////////////////////////////////////////////////////

    ASSERT((m->ports.getters = qsDictionaryCreate()));
    ASSERT((m->ports.setters = qsDictionaryCreate()));
    ASSERT((m->ports.inputs = qsDictionaryCreate()));
    ASSERT((m->ports.outputs = qsDictionaryCreate()));

    // TODO: Add some built-in control parameters?

    /////////////////////////////////////////////////////////////////////
    // 11. If there is a block declare() function call it.
    //     Note: this declare() for a super block may recurse and call
    //     this function again creating child blocks.  So this function
    //     must be re-entrant in addition to being thread-safe.
    /////////////////////////////////////////////////////////////////////

    int (*declare)(void *) =
        GetBlockSymbol(dlhandle, fileName, "declare");

    if(declare) {
        int ret;

        // This code must be reentrant.  declare() can call
        // qsGraph_createBlock() directly or indirectly, to load blocks
        // while it is loading.
        //
        // Hence, we where careful in the order in which we setup the
        // block data.
        struct QsWhichBlock stackSave;
        SetBlockCallback(b, CB_DECLARE, &stackSave);
        ret = declare(userData);
        RestoreBlockCallback(&stackSave);
        if(ret < 0)
            // Total failure and do not call undeclare().  Do not pass Go
            // and do not collect 200 dollars.
            ERROR("Block \"%s\" declare failed (ret=%d)", b->name, ret);
        else if(ret > 0)
            // Semi-failure, or put another way, it's just a odd mode
            // where the declare() is telling us to unload this block.
            // We will call undeclare() if it exists.
            INFO("Block \"%s\" declare returned %d", b->name, ret);

        if(ret) {
            // Destroy the block, because declare() told us to.
            //
            // We do not let undeclare() be called if ret < 0.
            _qsBlock_destroy(b, ret > 0);
            // Fail return value b = 0.
            b = 0;
        }
    }

    if(b) {
        // Successful block module load.
        DSPEW("Loaded %s block: %s \"%s\"",
                (b->type == QsBlockType_simple)?"simple":"super",
                m->fileName, b->name);
    }

    CHECK(pthread_mutex_unlock(&g->mutex));

    return b;
}


void qsSetUserData(void *userData) {

    struct QsBlock *b = GetBlock(CB_DECLARE|CB_CONFIG, 0, 0);
    ASSERT(b);
    struct QsGraph *g = b->graph;
    DASSERT(g);

    CHECK(pthread_mutex_lock(&g->mutex));
    b->userData = userData;
    CHECK(pthread_mutex_unlock(&g->mutex));
}


void qsAddRunFile(const char *fileName, bool runFileLoadCopy) {

    struct QsSimpleBlock *b = GetBlock(CB_DECLARE, 0, QsBlockType_simple);
    ASSERT(b, "Not a simple block in declare()");

    ASSERT(!b->runFile);
    DASSERT(!b->runFileDlhandle);

    b->runFile = strdup(fileName);
    ASSERT(b->runFile, "strdup() failed");
    b->runFileLoadCopy = runFileLoadCopy;
}
