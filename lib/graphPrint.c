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


#define LW  "penwidth=2.0" /*line width*/


// We limit the number of indentation spaces to this much:
static const char *Spaces =
    "                          ";


struct Helper {

    FILE *f;
    int depth;
    const char *kind;
    const char *shape;
};


static inline void PrintParameter(const struct QsPort *p,
        const char *kind, FILE *f) {

    fprintf(f, "\"%s:%s:%s:%s\"",
            kind,
            p->block->graph->name,
            p->block->name,
            p->name);
}



static int
PrintPassThroughConnection(const char *name, struct QsOutput *out,
        struct Helper *h) {

    DASSERT(out);

    if(!out->inputs) {
        DASSERT(!out->numInputs);
        return 0;
    }
    DASSERT(out->numInputs);
    DASSERT(out->port.name);
    DASSERT(strcmp(out->port.name, name) == 0);

    // Is this a passThrough output with connections on both ends?
    //
    if(out->passThrough && out->passThrough->output && out->inputs) {

        DASSERT(out->numInputs);
        struct QsInput *in = out->passThrough;
        DASSERT(in);

        fprintf(h->f, "%*.*s\"inputs:%s:%s:%s\"",
                h->depth, h->depth, Spaces,
                in->port.block->graph->name,
                in->port.block->name,
                in->port.name);
        fprintf(h->f, " -> ");
        fprintf(h->f, "\"outputs:%s:%s:%s\"",
                out->port.block->graph->name,
                out->port.block->name,
                out->port.name);
        fprintf(h->f, " [ color=\"#B06524\",style=dashed, " LW "];\n");
    }

    return 0;
}





static int
PrintOutputConnections(const char *name, struct QsOutput *out,
        struct Helper *h) {

    DASSERT(out);

    if(!out->inputs) {
        DASSERT(!out->numInputs);
        return 0;
    }
    DASSERT(out->numInputs);

    uint32_t i = 0;
    uint32_t num = out->numInputs;
    DASSERT(out->port.name);
    DASSERT(strcmp(out->port.name, name) == 0);

    // outputs connect to many inputs:
    for(struct QsInput **in = out->inputs; i < num; ++in) {

        DASSERT(*in);
        DASSERT((*in)->output == out);

        fprintf(h->f, "%*.*s\"outputs:%s:%s:%s\"",
                h->depth, h->depth, Spaces,
                out->port.block->graph->name,
                out->port.block->name,
                out->port.name);
        fprintf(h->f, " -> ");
        fprintf(h->f, "\"inputs:%s:%s:%s\"",
                (*in)->port.block->graph->name,
                (*in)->port.block->name,
                (*in)->port.name);
        fprintf(h->f, " [ color=red, " LW "];\n");

        ++i;
    }

    return 0;
}



static int
PrintParameterConnection(const char *name, struct QsParameter *p,
        struct Helper *h) {

    struct QsGroup *g = p->group;
    if(!g) return 0;
    DASSERT(g->sharedPeers);
    DASSERT(*g->sharedPeers);

    // If the first setter is this parameter we print the connections
    // now:
    if(*g->sharedPeers != ((void *) p + offsetof(struct QsSetter, job)))
        return 0;

    struct QsPort *firstPort = 0;
    const char *firstKind = 0;
    const char *arrow = ", arrowhead=none";

    if(g->getter) {
        firstPort = (void *) g->getter;
        firstKind = "getters";
        arrow = "";
    }

    for(struct QsJob **j = g->sharedPeers; j && *j; ++j) {

        struct QsPort *p = ((void *) (*j)) -
            offsetof(struct QsSetter, job);
        const char *kind = "setters";

        if(firstPort) {
            fprintf(h->f, "%*.*s",
                    h->depth, h->depth, Spaces);
            PrintParameter((void *) firstPort, firstKind, h->f);
            fprintf(h->f, " -> ");
            PrintParameter((void *) p, kind, h->f);
            fprintf(h->f, " [color=blue, " LW "%s];\n", arrow);
        } else {
            firstPort = p;
            firstKind = kind;
        }
    }

    return 0;
}


static inline void
PrintBlockConnections(const struct QsSimpleBlock *b, FILE *f) {

    struct Helper h = {
        .f = f,
        .depth = 2,
        .kind = "setters",
        .shape = ""
    };

    if(b->module.ports.setters)
        ASSERT(qsDictionaryForEach(b->module.ports.setters,
                (int (*) (const char *key, void *value,
                void *userData)) PrintParameterConnection, &h) >= 0);

    if(b->module.ports.outputs) {
        h.kind = "outputs";
        ASSERT(qsDictionaryForEach(b->module.ports.outputs,
                (int (*) (const char *key, void *value,
                void *userData)) PrintOutputConnections, &h) >= 0);

        ASSERT(qsDictionaryForEach(b->module.ports.outputs,
                (int (*) (const char *key, void *value,
                void *userData)) PrintPassThroughConnection, &h) >= 0);
    }
}


static inline void
PrintGraphConnections(const struct QsParentBlock *pb, FILE *f) {

    DASSERT(pb->block.type & QS_TYPE_PARENT);

    for(const struct QsBlock *b = pb->firstChild; b; b = b->nextSibling) {
        if(b->type == QsBlockType_simple)
            PrintBlockConnections((void *) b, f);
        else if(b->type == QsBlockType_super)
            PrintGraphConnections((void *) b, f);

        // We don't print other block types.
    }
}




static int
PrintPort(const char *name, const struct QsPort *p,
        struct Helper *h) {

#define SLEN  32

    // Show a type and size for control parameters
    char size[SLEN];
    snprintf(size, SLEN, "");
    switch(p->portType) {
        case QsPortType_setter:
        case QsPortType_getter:
            const struct QsParameter *par = (void *) p;
            switch(par->valueType) {
                case QsValueType_uint64:
                    snprintf(size, SLEN, "\\nuint64[%zu]",
                            par->size / sizeof(uint64_t));
                    break;
                case QsValueType_double:
                    snprintf(size, SLEN, "\\ndouble[%zu]",
                            par->size / sizeof(double));
                    break;
                case QsValueType_bool:
                    snprintf(size, SLEN, "\\nbool[%zu]",
                            par->size / sizeof(bool));
                    break;
                case QsValueType_string32:
                    snprintf(size, SLEN, "\\nstring32[%zu]",
                            par->size / 32);
                    break;
                default:
                    ASSERT(0, "Write more code here");
                    break;
            }
            break;
        case QsPortType_input:
            {
                struct QsInput *in = (void *) p;
                if(!in->output)
                    // Skip this unconnected port.
                    return 0;
            }
            break;
        case QsPortType_output:
            {
                struct QsOutput *out = (void *) p;
                if(!out->inputs)
                    // Skip this unconnected port.
                    return 0;
            }
            break;
        default:
            ASSERT(0, "Write more code here");
            break;
    }

    fprintf(h->f,
        "%*.*s\"%s:%s:%s:%s\" [label=\"%s%s\"%s];\n",
        h->depth, h->depth, Spaces,
        h->kind,
        p->block->graph->name, p->block->name,
        name, name, size, h->shape);

    return 0; // keep going through the setters list.
}


struct AHelper {

    FILE *f;
    int depth;
    const struct QsBlock *block;
};


static int
PrintAttribute(const char *name, const struct QsAttribute *a,
        struct AHelper *ah) {

    fprintf(ah->f,
        "%*.*s\"%s:%s:%s:%s\" [label=\"%s\"];\n",
        ah->depth, ah->depth, Spaces,
        "Attribute",
        ah->block->graph->name, ah->block->name,
        name, name);

    return 0; // keep going through the setters list.
}


static uint32_t
PrintConfigAttributes(const struct QsDictionary *d,
        const struct QsBlock *b,
        uint32_t clusterNum, FILE *f, int depth) {

    if(!d || qsDictionaryIsEmpty(d))
        return clusterNum;

    fprintf(f,
        "%*.*ssubgraph cluster_%" PRIu32 " {\n"
        "%*.*slabel=\"%s\";\n",
        depth, depth, Spaces,
        clusterNum++,
        depth+2, depth+2, Spaces,
        "Configurable Attributes");

    struct AHelper ah = {
        .f = f,
        .depth = depth + 2,
        .block = b
    };

    ASSERT(qsDictionaryForEach(d,
            (int (*) (const char *key, void *value,
            void *userData)) PrintAttribute, &ah) >= 0);

    fprintf(f,
            "%*.*s}\n",
            depth, depth, Spaces);

    return clusterNum;
}

static uint32_t
PrintStreamPorts(const struct QsModule *m, uint32_t clusterNum,
        FILE *f, int depth) {

    DASSERT(m->ports.inputs || m->ports.outputs);

    fprintf(f,
        "%*.*ssubgraph cluster_%" PRIu32 " {\n"
        "%*.*slabel=\"%s\";\n",
        depth, depth, Spaces,
        clusterNum++,
        depth+2, depth+2, Spaces,
        "flow()");

    struct Helper h = {
        .f = f,
        .depth = depth + 2,
        .kind = "",
        .shape = ""
    };

    if(m->ports.inputs) {
        h.kind = "inputs";
        h.shape = ", shape=trapezium";
        ASSERT(qsDictionaryForEach(m->ports.inputs,
                (int (*) (const char *key, void *value,
                void *userData)) PrintPort, &h) >= 0);
    }

    if(m->ports.outputs) {
        h.kind = "outputs";
        h.shape = ", shape=invtrapezium";
        ASSERT(qsDictionaryForEach(m->ports.outputs,
                (int (*) (const char *key, void *value,
                void *userData)) PrintPort, &h) >= 0);
    }

    fprintf(f,
        "%*.*s}\n",
        depth, depth, Spaces);

    return clusterNum;
}


static uint32_t
PrintPorts(const struct QsDictionary *d, uint32_t clusterNum,
        FILE *f, int depth, const char *kind) {

    if(!d || qsDictionaryIsEmpty(d))
        return clusterNum;

    fprintf(f,
        "%*.*ssubgraph cluster_%" PRIu32 " {\n"
        "%*.*slabel=%s;\n",
        depth, depth, Spaces,
        clusterNum++,
        depth+2, depth+2, Spaces,
        kind);

    struct Helper h = {
        .f = f,
        .depth = depth + 2,
        .kind = kind,
        .shape = ""
    };
    ASSERT(qsDictionaryForEach(d,
            (int (*) (const char *key, void *value,
            void *userData)) PrintPort, &h) >= 0);

    fprintf(f,
        "%*.*s}\n",
            depth, depth, Spaces);

    return clusterNum;
}



static uint32_t
PrintChildBlocks(const struct QsParentBlock *pb, uint32_t clusterNum,
        FILE *f, int depth);


static inline uint32_t
PrintBlock(const struct QsBlock *b, uint32_t clusterNum,
        FILE *f, int depth) {


    struct QsModule *m = 0;

    char *fileName;
    switch(b->type) {
        case QsBlockType_top:
            // This is the graph's top block:
            fileName = "top block";
            break;
        case QsBlockType_super:
            m = &((struct QsSuperBlock *) b)->module;
            fileName = m->fileName;
            break;
        case QsBlockType_simple:
            m = &((struct QsSimpleBlock *) b)->module;
            fileName = m->fileName;
            break;
        default:
            ASSERT(0, "Write more code here");
            break;
    }

    fprintf(f,
        "%*.*ssubgraph cluster_%" PRIu32 " {\n"
        "%*.*slabel=\"%s [%s]\";\n",
        depth, depth, Spaces,
        clusterNum++,
        depth+2, depth+2, Spaces,
        b->name, fileName);

#if 0
    fprintf(f,
        "%*.*s%s_%" PRIu32 "-> %s_%" PRIu32 ";",
        depth+2, depth+2, Spaces,
        b->name, clusterNum, b->name, clusterNum + 1000);
#endif


    switch(b->type) {

        case QsBlockType_simple:
        {
            struct QsSimpleBlock *sb = (void *) b;
            uint32_t initClusterNum = clusterNum;

            if(sb->module.ports.setters &&
                    !qsDictionaryIsEmpty(sb->module.ports.setters))
                clusterNum = PrintPorts(sb->module.ports.setters,
                        clusterNum, f, depth+2, "setters");
            if(sb->module.ports.getters &&
                    !qsDictionaryIsEmpty(sb->module.ports.getters))
                clusterNum = PrintPorts(sb->module.ports.getters,
                        clusterNum, f, depth+2, "getters");
            if((sb->module.ports.inputs &&
                        !qsDictionaryIsEmpty(sb->module.ports.inputs))
                    ||
                    (sb->module.ports.outputs &&
                        !qsDictionaryIsEmpty(sb->module.ports.outputs)))
                // We decided to put the input and output streams
                // together in one subgraph cluster.
                clusterNum = PrintStreamPorts(&sb->module,
                        clusterNum, f, depth+2);

            if(clusterNum == initClusterNum)
                // We have not printed any content to the simple
                // block subgraph so we print this:
                fprintf(f,
        "%*.*s\"%s:%s\" [label=\" \",shape=box];\n",
        depth+2, depth+2, Spaces,
        b->graph->name, b->name);
        }
            break;

        case QsBlockType_super:
        case QsBlockType_top:
            clusterNum = PrintChildBlocks((void *) b,
                    clusterNum, f, depth);
            break;
        default:
            ASSERT(0);
            break;
    }

    if(m)
        clusterNum = PrintConfigAttributes(m->attributes, b,
                clusterNum, f, depth+2);

    fprintf(f,
            "%*.*s}\n",
            depth, depth, Spaces);

    return clusterNum;
}


static inline uint32_t
PrintChildBlocks(const struct QsParentBlock *pb, uint32_t clusterNum,
        FILE *f, int depth) {

    for(struct QsBlock *b = pb->firstChild; b; b = b->nextSibling)
        clusterNum = PrintBlock(b, clusterNum, f, depth+2);

    if(!pb->firstChild) {
        if(pb->block.graph)
            // Add some content to a parent block with no children.
            fprintf(f,
        "%*.*s\"%s:%s\" [label=\"%s\",shape=box];\n",
                depth, depth, Spaces,
                pb->block.graph->name, pb->block.name, pb->block.name);
        else
            fprintf(f,
        "%*.*s\"%s:%s\" [label=\"empty\",shape=box];\n",
                depth, depth, Spaces,
                pb->block.name, pb->block.name);
    }

    return clusterNum;
}


struct PrintGraphHelper {

    uint32_t clusterNum;
    FILE *f;
};



static int PrintGraph(const char *name,
        const struct QsGraph *g,
        struct PrintGraphHelper *h) {

    DASSERT(g);
    DASSERT(name);
    DASSERT(name[0]);
    DASSERT(g->parentBlock.block.type == QsBlockType_top);

    CHECK(pthread_mutex_lock((void *) &g->mutex));

    DASSERT(g->parentBlock.block.type & QS_TYPE_PARENT);

    h->clusterNum = PrintBlock((void *) &g->parentBlock,
            h->clusterNum, h->f, 2);

    fprintf(h->f, "  /* Connections */\n");

    PrintGraphConnections((void *) g, h->f);

    fprintf(h->f, "\n");

    CHECK(pthread_mutex_unlock((void *) &g->mutex));

    return 0;
}


static int PrintGraphName(const char *name, const struct QsGraph *graph,
        FILE *f) {

    DASSERT(name);

    fprintf(f, " %s", name);
    return 0;
}


int qsGraph_printDot(const struct QsGraph *graph, FILE *f,
        uint32_t opts) {

    NotWorkerThread();

    // If graph == 0 print all graphs.
    //
    DASSERT(f);

    // The mutex data in the graph must be written, so the graph is only
    // sort-of const.  Or put another way it's not const.
    //
    struct QsGraph *g = 0;
    if(graph)
        g = (void *) graph;


    CHECK(pthread_mutex_lock(&gmutex));


    if(numGraphs == 0) {
        fprintf(f,
            "digraph {\n"
            "  label=\"There are no quickstream graphs to display.\";\n"
            "}\n");
        CHECK(pthread_mutex_unlock(&gmutex));
        return 0;
    }

    fprintf(f,
            "digraph {\n"
            "  label=\"quickstream graph%s:",
            (numGraphs == 1)?"":"s");

    if(g)
        PrintGraphName(g->name, g, f);
    else
        ASSERT(qsDictionaryForEach(graphs,
                (int (*) (const char *key, void *value,
                void *userData)) PrintGraphName, f) >= 0);

    fprintf(f,
            "\";\n"
            "\n");


    struct PrintGraphHelper h = {
        .clusterNum = 0,
        .f = f
    };

    if(g)
        PrintGraph(g->name, g, &h);
    else
        ASSERT(qsDictionaryForEach(graphs,
                (int (*) (const char *key, void *value,
                void *userData)) PrintGraph, &h) >= 0);

    fprintf(f,
            "}\n");


    CHECK(pthread_mutex_unlock(&gmutex));

    return 0;
}


int qsGraph_printDotDisplay(const struct QsGraph *g,
        bool waitForDisplay, uint32_t opts) {

    NotWorkerThread();


    int fd[2];
    int ret = 0;

    if(pipe(fd) != 0) {
        ERROR("pipe() failed");
        return 1;
    }

    pid_t pid = fork();

    if(pid < 0) {
        ERROR("fork() failed");
        return 1;
    }

    if(pid) {
        //
        // I'm the parent
        //
        close(fd[0]);
        FILE *f = fdopen(fd[1], "w");
        if(!f) {
            ERROR("fdopen() failed");
            return 1;
        }

        if(qsGraph_printDot(g, f, opts))
            ret = 1; // failure
        fclose(f);

        if(waitForDisplay) {
            int status = 0;
            if(waitpid(pid, &status, 0) == -1) {
                WARN("waitpid(%u,,0) failed", pid);
                ret = 1;
            }
            INFO("waitpid() returned status=%d", status);
        }
    } else {
        //
        // I'm the child
        //
        DASSERT(pid == 0, "Am I a child process or not? WTF");
        close(fd[1]);
        if(fd[0] != 0) {
            // Use dup2() to turn the read part of the pipe, fd[0], into
            // the stdin, fd=0, of the child process.
            if(dup2(fd[0], 0)) { 
                WARN("dup2(%d,%d) failed", fd[0], 0);
                exit(1);
            }
            close(fd[0]);
        }
        // To see the dot file in the terminal:
        //execlp("cat", "cat", (const char *) 0);

        execlp("display", "display", (const char *) 0);
        WARN("execlp(\"%s\", \"%s\", 0) failed", "display", "display");
        exit(3);
    }

    return ret; // success ret=0
}
