#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
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
#include "metaData.h"

#include "graphConnect.h"



struct Helper {
    FILE *f;
    const struct QsParentBlock *parentBlock;
    const struct QsBlock *block;
};


static
int PrintConfigureCall(const char *name, struct QsAttribute *a,
        const struct Helper *h) {

    DASSERT(name);
    DASSERT(a);
    DASSERT(h);
    DASSERT(h->block);
    DASSERT(h->block->name);

    if(!a->parentBlock || a->parentBlock != h->parentBlock) return 0;

    DASSERT(a->lastArgv);
    DASSERT(a->lastArgc);
    
    const char *bname = h->block->name;
    if(h->parentBlock->block.type & QS_TYPE_MODULE) {
        // Remove the part of the name before a ':'
        // along with the ':'.
        while(*bname && *bname != ':') ++bname;
        DASSERT(*bname);
        ++bname;
        DASSERT(*bname);
    }

    fprintf(h->f,
"    FAIL_IF(qsBlock_configVByName(0/*graph*/, \"%s\",\n"
"            ",
        bname);

    for(int i = 0; i < a->lastArgc; ++i)
        fprintf(h->f," \"%s\",", a->lastArgv[i]);
    fprintf(h->f,"\n"
"             0/*null terminate*/));\n"
"\n"
            );

    return 0;
}


static void
PrintBlockConfigurations(FILE *f, struct QsBlock *b,
        const struct QsParentBlock *matchParent) {

    DASSERT(f);
    DASSERT(b);
    DASSERT(matchParent);

    if(b->type & QS_TYPE_MODULE) {
        struct Helper h = {
            .f = f,
            .parentBlock = matchParent,
            .block = b
        };
        struct QsModule *m = Module(b);

        if(m->attributes)
            ASSERT(qsDictionaryForEach(m->attributes,
                    (int (*) (const char *key, void *value,
                        void *userData)) PrintConfigureCall, &h) >= 0);
    }

    if(b->type & QS_TYPE_PARENT)
        for(b = ((struct QsParentBlock *)b)->firstChild; b;
                b = b->nextSibling) {
            PrintBlockConfigurations(f, b, matchParent);
        }
}


static inline struct QsParameter *GetParameter(const struct QsJob *j) {

    return (void *) ((void *)j - offsetof(struct QsSetter, job));
}


struct PFHelper {

    const struct QsPort *realPort;
    const char *aliasName; // Or the real port name.
    const char *blockName; // Block that owns the alias or port.
};


static int
DFindPort(const char *name, struct QsPort *realPort,
        struct PFHelper *pfh) {


    if(realPort != pfh->realPort)
        return 0; // keep looking

    // We found it.  Return the alias Name which is an alias from the
    // block's port alias dictionary that we are looking through now;
    // or this is the real name of a real port that is owned by a simple
    // block (rock bottom).
    pfh->aliasName = name;

    return 1; // done looking.
}


// We need to find the closest (in generation linage) to the parent Block,
// b, and no higher.  So the closest is direct children of parent Block,
// b.  Direct children of h->parentBlock can be super blocks too.  We are
// looking for closest port aliases, then the next closest port alias, and
// down to the leaf node (simple blocks) "real" ports (that are passed in
// here), the fall back port.
//
// The port we are looking for is there, we just need the route to it that
// is from the grandest parent (or simple block leaf) that is a descendent
// of b.  We are given the fall back port of the simple block, that owns
// this "real" port.
//
// This qsGraph_saveSuperBlock() is a very rare transit (non-tight-loop)
// function so speed in not a issue; just don't put a sleep(10) in here
// and we will be okay.
//
// The "real" ports do not have a pointer back up to all their port
// aliases, that would be a total pain in the ass, given there can be any
// number of aliases, and at many generational levels; so we do this long
// search to find the closest port alias, or the real port as a fall
// back.
//
// Returns with *blockName 0 if the graph owns the alias.
// *portName will be set when it's found, be it alias or real port.
//
static void
FindBlockAndPortNames(const struct QsBlock *b,
        const char **blockName, const char **portName,
        const struct QsPort *realPort) {
    DASSERT(b);
    DASSERT(realPort);

    const struct QsDictionary *d = GetDictionary(b, realPort->portType);
    DASSERT(d);

    struct PFHelper pfh = {
        .realPort = realPort,
        .aliasName = 0, // could also be a real port name.
        .blockName = 0
    };

    qsDictionaryForEach(d, (int (*)(const char *, void *, void *))
            DFindPort, &pfh);

    if(pfh.aliasName) {
        // We found it.
        //
        // If it's from the top block (graph) alias list than we do not
        // name the block; by convention.  Cause the name will change
        // at run time.
        if(!(b->type & QS_TYPE_TOP))
            *blockName = b->name;
        *portName = pfh.aliasName;
        return;
    }

    if(!(b->type & QS_TYPE_PARENT))
        // Go to next sibling.  This is a child that is not a parent; a
        // simple block.
        return;

    // Repurpose variable b.
    b = ((struct QsParentBlock *) b)->firstChild;
    // If we have no more to search through, our search failed; and that
    // can't be this real port (realPort) must exist in one of these
    // block's port lists.  So ya, this can't fail.
    for(; b; b = b->nextSibling) {
        // Search the next block in the family line.  Recurse.
        FindBlockAndPortNames(b, blockName, portName, realPort);
        if(*portName)
            // Done recursing.  We found it.  Keep popping the function
            // call stack.
            return;
    }
}


static inline void PrintConnectParmeters(const struct QsBlock *b,
        const struct QsParameter *p1,
        const struct QsParameter *p2, FILE *f) {

    DASSERT(b);
    DASSERT(p1);
    DASSERT(p2);
    DASSERT(f);

    const char *blockName1 = 0,
          *blockName2 = 0,
          *aliasName1 = 0,
          *aliasName2 = 0;

    FindBlockAndPortNames(b, &blockName1, &aliasName1, (void *) p1);
    DASSERT(aliasName1);

    FindBlockAndPortNames(b, &blockName2, &aliasName2, (void *) p2);
    DASSERT(aliasName2);


    fprintf(f,
"    FAIL_IF(qsGraph_connectByStrings(0, "
        "%s%s%s, \"%c\", \"%s\", "
        "%s%s%s, \"%c\", \"%s\"));\n"
"\n"
,
        blockName1?"\"":"", blockName1?blockName1:"0", blockName1?"\"":"", 
        (p1->port.portType == QsPortType_setter)?'s':'g',
        aliasName1,

        blockName2?"\"":"", blockName2?blockName2:"0", blockName2?"\"":"",
        (p2->port.portType == QsPortType_setter)?'s':'g',
        aliasName2);
}


static int
PrintParameterConnections(const char *name, const struct QsParameter *p,
        const struct Helper *h) {

    DASSERT(name);
    DASSERT(p);
    DASSERT(h);
    DASSERT(h->f);
    DASSERT(h->parentBlock);
    DASSERT(h->parentBlock->block.type & QS_TYPE_TOP);
    DASSERT(p->port.portType == QsPortType_setter);

    // TODO: Do we let super blocks disconnect ports?
    //
    // Oh boy, will such a thing turn this code to a pile of shit; not
    // that it's pretty now.

    struct QsGroup *g = p->group;
    if(!g)
        return 0;

    DASSERT(g->sharedPeers);
    DASSERT(*g->sharedPeers);

    // The first parameter in the group we get from the first job
    // in the job peers.
    struct QsJob **j = g->sharedPeers;
    struct QsParameter *p1 = GetParameter(*j);


    if(p1 != p)
        // So that we do this just once.
        return 0;

    // We only get here once for each parameter connection group.  

    if(p1->port.parentBlock != h->parentBlock)
        p1 = 0;

    // At this point we use p as the parameter iterator that goes through
    // the jobs (parameters) list in the parameter group in
    // g->sharedPeers.
    //
    // p1 is the first match we find, if it does have a parent block
    // match.

    // This may not be how the parameter connections were made, but
    // we think that the results will be the same.
    //
    // If there are disconnections preformed that may not be true.

    if(g->getter &&
            g->getter->parameter.port.parentBlock == h->parentBlock) {
        if(!p1)
            p1 = (void *) g->getter;
        else
            PrintConnectParmeters((void *) h->parentBlock,
                    (void *) g->getter, p1, h->f);
    }

    if(!p1)
        for(++j; *j; ++j)
            if(GetParameter(*j)->port.parentBlock == h->parentBlock) {
                p1 = GetParameter(*j);
                break;
            }

    if(*j) {
        DASSERT(p1);
        for(++j; *j; ++j)
            if(GetParameter(*j)->port.parentBlock == h->parentBlock)
                PrintConnectParmeters((void *) h->parentBlock,
                        p1, GetParameter(*j), h->f);
    }

    return 0;
}


static int
PrintStreamConnection(const char *name, struct QsInput *in,
        const struct Helper *h) {

    DASSERT(name);
    DASSERT(in);
    DASSERT(h);
    DASSERT(h->f);
    DASSERT(h->parentBlock);
    DASSERT(h->parentBlock->block.type & QS_TYPE_TOP);

    // TODO: do we let super blocks disconnect ports?

    if(!in->output || in->port.parentBlock != h->parentBlock)
        return 0;

    // The graph made the connection with this input, in.
    // There must be a stream input port and a stream output
    // port in a simple block that is a descendent of this
    // h->parentBlock, graph, top block.  But first is there
    // a more direct linage parent (super) block that has an
    // alias in it's list of aliased ports.
    //
    const char *oBlockName = 0,
          *iBlockName = 0,
          *oPortName = 0,
          *iPortName = 0;

    FindBlockAndPortNames((void *) h->parentBlock,
            &oBlockName, &oPortName, (void *) in->output);
    DASSERT(oPortName);
    FindBlockAndPortNames((void *) h->parentBlock,
            &iBlockName, &iPortName, (void *) in);
    DASSERT(iPortName);


    fprintf(h->f,
"    FAIL_IF(qsGraph_connectByStrings(0, "
        "%s%s%s, \"o\", \"%s\", "
        "%s%s%s, \"i\", \"%s\"));\n"
"\n",
        oBlockName?"\"":"", oBlockName?oBlockName:"0", oBlockName?"\"":"", 
        oPortName,

        iBlockName?"\"":"", iBlockName?iBlockName:"0", iBlockName?"\"":"", 
        iPortName);

    return 0;
}


static void
PrintChildConnections(FILE *f, const struct QsParentBlock *p,
        const struct QsParentBlock *matchParent) {

    DASSERT(f);
    DASSERT(p->block.type & QS_TYPE_PARENT);

    struct Helper h = { .f = f, .parentBlock = matchParent };

    for(struct QsBlock *b = p->firstChild; b; b = b->nextSibling) {
        if(b->type & QS_TYPE_PARENT) {
            DASSERT(b->type == QsBlockType_super);
            // Graphs and super blocks can make connections between
            // children's children, and so on...
            //
            PrintChildConnections(f, (void *) b, matchParent);
        } else {
            // We only need to bother with connections between simple
            // blocks.  Super blocks have port aliases, but they
            // only make connections between between ports of simple
            // blocks using the aliases.
            DASSERT(b->type == QsBlockType_simple);
            struct QsModule *m = Get_Module(b);
            DASSERT(m->ports.inputs);
            ASSERT(qsDictionaryForEach(m->ports.inputs,
                    (int (*) (const char *key, void *value,
                    void *userData)) PrintStreamConnection, &h) >= 0);
            DASSERT(m->ports.setters);
            ASSERT(qsDictionaryForEach(m->ports.setters,
                    (int (*) (const char *key, void *value,
                    void *userData)) PrintParameterConnections, &h) >= 0);
        }
    }
}


static int
PrintAlias(const char *name, struct QsPort *p,
        const struct Helper *h) {

    DASSERT(name);
    DASSERT(h);
    DASSERT(h->f);
    DASSERT(p);
    DASSERT(p->name);

    fprintf(h->f,
"    FAIL_IF(qsBlock_makePortAlias(0, \"%s\", \"%s\", \"%s\", \"%s\"));\n"
"\n"
        , p->block->name, GetPortTypeString(p), p->name, name);

    return 0;
}


static void
PrintPortAliases(FILE *f, const struct QsParentBlock *p) {

    DASSERT(f);
    DASSERT(p->block.type & QS_TYPE_PARENT);

    struct QsPortDicts *ports;

    if(p->block.type == QsBlockType_top) {
        // The parent block is the graph.
        ports = &((struct QsGraph *)p)->ports;
    } else {
        DASSERT(p->block.type == QsBlockType_super);
        ports = &((struct QsSuperBlock *) p)->module.ports;
    }

    struct Helper h = { .f = f, .parentBlock = p };

    ASSERT(qsDictionaryForEach(ports->inputs,
                    (int (*) (const char *key, void *value,
                    void *userData)) PrintAlias, &h) >= 0);
    ASSERT(qsDictionaryForEach(ports->outputs,
                    (int (*) (const char *key, void *value,
                    void *userData)) PrintAlias, &h) >= 0);
    ASSERT(qsDictionaryForEach(ports->setters,
                    (int (*) (const char *key, void *value,
                    void *userData)) PrintAlias, &h) >= 0);
    ASSERT(qsDictionaryForEach(ports->getters,
                    (int (*) (const char *key, void *value,
                    void *userData)) PrintAlias, &h) >= 0);
}


static void
PrintLoadChildBlocks(FILE *f, const struct QsParentBlock *p) {

    DASSERT(f);
    DASSERT(p->block.type & QS_TYPE_PARENT);

    for(struct QsBlock *b = p->firstChild; b; b = b->nextSibling) {
        struct QsModule *m = Get_Module(b);
        fprintf(f,
"    FAIL_IF(!qsGraph_createBlock(0, 0, \"%s\", \"%s\", 0));\n"
"\n"
                , m->fileName, b->name);
    }
}



int qsGraph_saveSuperBlock(const struct QsGraph *g,
        const char *path, uint32_t optsFlag) {

    NotWorkerThread();

    DASSERT(g);
    ASSERT(path);
    DASSERT(path[0]);

    char *cPath = 0;
    char *dsoPath = 0;

    size_t plen = strlen(path);
    ASSERT(plen >= 2);

    // TODO: this code is GNU/Linux (UNIX-like) specific (.so suffix).
    //
    if(plen >= 3 && strcmp(path + plen-2, ".c") == 0) {
        // The path ends in ".c"
        cPath = strdup(path);
        ASSERT(cPath, "strdup() failed");
        dsoPath = malloc(plen+2);
        ASSERT(dsoPath, "malloc(%zu) failed", plen+2);
        strcpy(dsoPath, path);
        // clip off the ending ".c"
        dsoPath[plen-2] = '\0';
        strcat(dsoPath, ".so");
    } else if(plen >= 4 && strcmp(path + plen-3, ".so") == 0) {
        // The path ends in ".so"
        dsoPath = strdup(path);
        ASSERT(dsoPath, "strdup() failed");
        cPath = malloc(plen);
        ASSERT(cPath, "malloc%zu) failed", plen);
        strncpy(cPath, path, plen);
        cPath[plen-2] = 'c';
        cPath[plen-1] = '\0';
    } else {
        // path + ".so" + 1
        dsoPath = malloc(plen+4);
        ASSERT(dsoPath, "malloc(%zu) failed", plen+4);
        strcpy(dsoPath, path);
        strcat(dsoPath, ".so");

        // path + ".c" + 1
        cPath = malloc(plen+3);
        ASSERT(cPath, "malloc(%zu) failed", plen+3);
        strcpy(cPath, path);
        strcat(cPath, ".c");
    }

    int ret = 0;
    FILE *cFile = 0;
    char *run = 0;

    errno = 0;

    if(access(cPath, F_OK) != -1) {
        ERROR("File \"%s\" exists", cPath);
        ret = -1;
        goto cleanup;
    }
    if(access(dsoPath, F_OK) != -1) {
        ERROR("File \"%s\" exists", dsoPath);
        ret = -2;
        goto cleanup;
    }

    DSPEW("cPath=%s dsoPath=%s", cPath, dsoPath);

    cFile = fopen(cPath, "w");
    if(!cFile) {
        ERROR("fopen(\"%s\",\"w\") failed", cPath);
        ret = -3;
        goto cleanup;
    }

    CHECK(pthread_mutex_lock(&((struct QsGraph *)g)->mutex));

#define SEPARATOR \
"////////////////////////////////////////"\
"///////////////////////\n"

    fprintf(cFile,
// This is like C heredoc, but not:
//
"// This is a generated file.\n"
"//\n"
"// This is the source to a quickstream Super Block module.\n"
"\n"
"#include <string.h>\n"
"\n"
"#include <quickstream.h>\n"
"#include <quickstream_debug.h>\n"
"\n"
"\n"
"struct QsBlockOptions options = { .type = QsBlockType_super };\n"
"\n"
"// ERROR() will print a line number which is a big help.\n"
"#define FAIL_IF(x)   do {if( (x) ) { ERROR(); return -1; } } while(0)\n"
"\n"
"\n"
    );

    if(g->metaData) {

         fprintf(cFile, "////%s//  Block Meta Data\n////%s",
            SEPARATOR, SEPARATOR);

        WriteMetaDataToSuperBlock(cFile, g);
        fprintf(cFile, "////%s\n", SEPARATOR);
    }

    fprintf(cFile,
"\n"
"int declare(void) {\n"
"\n"
    );

    fprintf(cFile, "    %s    //    Load child blocks\n    %s\n",
            SEPARATOR, SEPARATOR);
    PrintLoadChildBlocks(cFile, (void *) g);

    fprintf(cFile, "    %s    //    Setup Port Aliases to child block ports\n    %s\n",
            SEPARATOR, SEPARATOR);
    PrintPortAliases(cFile, (void *) g);

    fprintf(cFile, "    %s    //    Configure child blocks\n    %s\n",
            SEPARATOR, SEPARATOR);
    PrintBlockConfigurations(cFile, (void *) g, (void *) g);

    fprintf(cFile, "    %s    //    Connect child blocks\n    %s\n",
            SEPARATOR, SEPARATOR);
    PrintChildConnections(cFile, (void *) g, (void *) g);

    fprintf(cFile, "    %s    //    Maybe add some qsAddConfig() calls below here\n    %s\n",
            SEPARATOR, SEPARATOR);



    fprintf(cFile,
"\n"
"    return 0;\n"
"}\n"
    );

    // Close and flush the C file.
    fclose(cFile);
    cFile = 0;

    CHECK(pthread_mutex_unlock(&((struct QsGraph *)g)->mutex));

    ///////////////////////////////////////////////////////////////
    // Now compile it.
    ///////////////////////////////////////////////////////////////

    // TODO: compile options and how to vary them ...
    // -- Using quickstream installation configure options.
    // -- Using environment variable(s).
    // In the end it's just getting a command line string.
    //
#define COMPILE1 "cc -shared -I"
// TODO: maybe remove the "/lib/.." from this composed compile
// command-line string.  But this is maybe more robust.  Not sure.
#define COMPILE3 "/../include -g -Wall -Werror -fPIC -DDEBUG -DSPEW_LEVEL_DEBUG "
#define OUT_OPT " -o "

    DASSERT(qsLibDir);
    DASSERT(qsLibDir[0] == '/');

    pid_t pid = fork();

    size_t rlen = strlen(COMPILE1) + strlen(qsLibDir) +
        strlen(COMPILE3) + strlen(OUT_OPT) +
        strlen(cPath) + strlen(dsoPath) + 1;

    // TODO: Should we just use stack memory to do this?
    // Then again, compile command-lines can get very long.
    //
    run = calloc(1, rlen);
    ASSERT(run, "calloc(1, %zu) failed", rlen);
    sprintf(run, "%s%s%s%s%s%s",
            COMPILE1, qsLibDir, COMPILE3,
            cPath, OUT_OPT, dsoPath);

    if(pid == -1) {
        ERROR("fork() failed");
        ret = -4; // error
        goto cleanup;
    }

    if(pid == 0) {

        // I am the child.
        //
        // TODO: Maybe run this without bash?  But somethings bash does
        // magical/helpful things.
        //
        INFO("Running: /bin/bash -c \"%s\"", run);
        execlp("bash", "/bin/bash", "-c", run, NULL);
        ERROR("execlp(\"\" ,\"%s\", 0) failed", run);
        // error
        exit(1);
    }

    // I am the parent.
    siginfo_t info;

    if(waitid(P_PID, pid, &info, WEXITED) != 0) {

        ERROR("Running: \"%s\" failed", run);
        ret = -5;
        goto cleanup;
    }

    if(info.si_code != CLD_EXITED || info.si_status != 0) {

        ERROR("Running: \"%s\" failed", run);
        ret = -6;
        goto cleanup;
    }

    INFO("Created: \"%s\"", dsoPath);


cleanup:


    if(run) {
        DZMEM(run, strlen(run));
        free(run);
    }


    if(cFile)
        fclose(cFile);

    DZMEM(cPath, strlen(cPath));
    DZMEM(dsoPath, strlen(dsoPath));
    free(cPath);
    free(dsoPath);

    return ret;
}
