#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <setjmp.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <pthread.h>

#include "../include/quickstream/app.h" // public interfaces
#include "../include/quickstream/block.h" // public interfaces
#include "../include/quickstream/builder.h" // public interfaces


#include "debug.h"
#include "Dictionary.h"
#include "block.h"
#include "parameter.h"
#include "graph.h"
#include "trigger.h"
#include "threadPool.h"
#include "builder.h"



// We define some strings that we use more than once in this file.  This
// keeps them all the same everywhere they are used in this file.
//
#define GETTER   "Getter"
#define SETTER   "Setter"
#define CONSTANT "Constant"

static inline
const char *GetKindString(const struct QsParameter *p) {

    switch(p->kind) {

        case QsConstant:
            return CONSTANT;
        case QsGetter:
            return GETTER;
        case QsSetter:
            return SETTER;
    }

    ASSERT(0, "WTF");
    return "WTF";
}


static inline
const char *ParameterFillColor(const struct QsParameter *p) {

    switch(p->kind) {

        case QsConstant:
            return "green";
        case QsGetter:
            return "blue";
        case QsSetter:
            return "orange";
    }

    ASSERT(0, "WTF");
    return "WTF";
}



// Used to pass function parameter arguments to printing callback.
struct PrintParameterStruct {
    FILE *file;
    const char *desc; // description string
    const char *blockName;
    int *clusterNum;
    int graphNum;
    enum QsParameterKind kind;
    bool gotOne;
};

static int
PrintParameterDotContent(const char *key, const struct QsParameter *p,
                        struct PrintParameterStruct *ps) {

    // Getters and Constants are in the same dictionary list so we need to
    // check that we get the right kind.

    if(qsParameterGetKind(p) != ps->kind)
        return 0; // wrong kind

    if(!ps->gotOne) {
        // This is the first parameter of this kind found.
        fprintf(ps->file,
            "    subgraph cluster_%d {\n"
            "      label=\"%ss\";\n"
            "\n",
            *ps->clusterNum, ps->desc);
        ps->gotOne = true;
        ++(*ps->clusterNum);
    }

    fprintf(ps->file,
            "      \"%d:%s:%s:%s\" [label=\"%s\",color=%s];\n",
            ps->graphNum, ps->desc, ps->blockName, qsParameterGetName(p),
            qsParameterGetName(p), ParameterFillColor(p));

    return 0;
}


static inline
void
PrintParameters(const char *desc, struct QsDictionary *d,
    enum QsParameterKind kind, const char *blockName,
    FILE *file, int *clusterNum, int graphNum) {

    if(qsDictionaryIsEmpty(d))
        // No parameters to print
        return;

    struct PrintParameterStruct ps;
    ps.file = file;
    ps.kind = kind;
    ps.desc = desc;
    ps.graphNum = graphNum;
    ps.blockName = blockName;
    ps.gotOne = false;
    ps.clusterNum = clusterNum;

    qsDictionaryForEach(d,
            // Butt ugly function cast
            (int (*) (const char *key, void *value,
                void *userData)) PrintParameterDotContent, &ps);
    if(ps.gotOne)
        fprintf(file,
            "    }\n");
}


struct PrintParameterConnectionStruct {

    FILE *file;
    int graphNum;
};


static
int PrintGetterConnectionDotContent(const char *key,
        const struct QsParameter *p,
        struct PrintParameterConnectionStruct *ps) {

    if(p->kind != QsGetter) return 0;

    if(!p->first) {
        DASSERT(p->numConnections == 0);
        // There are no setters connected
        return 0;
    }

    // If the connection list exists than there must be at least one
    // setter.
    DASSERT(p->first->next);

    for(struct QsParameter *s = p->first->next; s; s = s->next) {
        DASSERT(s->kind == QsSetter);
        fprintf(ps->file,
            "  \"%d:%s:%s:%s\" -> \"%d:%s:%s:%s\" [color=blue];\n",
            ps->graphNum, GETTER, p->block->block.name, p->name,
            ps->graphNum, SETTER, s->block->block.name, s->name);
    }

    return 0;
}


static inline
void PrintGetterToSetterConnections(const struct QsGraph *g, FILE *file,
        int graphNum) {

    struct QsBlock *b = g->firstBlock;
    for(; b; b = b->next)
        if(!b->isSuperBlock &&
                !qsDictionaryIsEmpty(((struct QsSimpleBlock *)b)->getters))
            break;

    if(!b)
        // nothing to print
        return;

    // We will have at least one line in this dot file.
    // TODO: Well shit, maybe not if there is no getter parameters.
    fprintf(file, "\n");

    struct PrintParameterConnectionStruct ps;
    ps.file = file;
    ps.graphNum = graphNum;

    for(; b; b = b->next) {
        if(b->isSuperBlock) continue;
        struct QsSimpleBlock *smB = (struct QsSimpleBlock *) b;
        qsDictionaryForEach(smB->getters,
            // Butt ugly function cast
            (int (*) (const char *key, void *value,
                void *userData)) PrintGetterConnectionDotContent, &ps);
     }
}


static
int PrintParameterConnectionDotContent(const char *key,
        const struct QsParameter *p,
        struct PrintParameterConnectionStruct *ps) {

    if(p->kind == QsGetter) return 0;

    if(!p->first) {
        DASSERT(p->numConnections == 0);
        // There are no setters connected
        return 0;
    }

    if(p->first != p) 
        // We do this printing only is we have the first Constant
        // parameter.
        //
        // TODO: This is a little wasteful that we need to look at
        // all the blocks parameters, when all we need are the connected
        // first constant parameters that are in this block.
        return 0;


    DASSERT(p->first->kind == QsConstant);
    // If the connection list exists than there must be at least two
    // parameters in the list:
    DASSERT(p->first->next);

    for(struct QsParameter *s = p->first->next; s; s = s->next) {
        const char *dir = (s->kind == QsSetter)?"[":"[dir=none,";
        fprintf(ps->file,
            "  \"%d:%s:%s:%s\" -> \"%d:%s:%s:%s\" %scolor=green]\n",
            ps->graphNum, GetKindString(p), p->block->block.name, p->name,
            ps->graphNum, GetKindString(s), s->block->block.name, s->name,
            dir);
    }

    return 0;
}


static inline
void PrintConstantAndSetterConnections(const struct QsGraph *g, FILE *file,
        int graphNum) {

    struct QsBlock *b = g->firstBlock;
    for(; b; b = b->next)
        if(!b->isSuperBlock &&
                !qsDictionaryIsEmpty(((struct QsSimpleBlock *)b)->getters))
            break;

    if(!b)
        // Nothing to print.
        return;


    // We will have at least one line in this dot file.
    // TODO: Well shit, maybe not if there is no constant parameters.
    fprintf(file, "\n");

    struct PrintParameterConnectionStruct ps;
    ps.file = file;
    ps.graphNum = graphNum;


    for(; b; b = b->next) {
        if(b->isSuperBlock) continue;
        struct QsSimpleBlock *smB = (struct QsSimpleBlock *) b;
        qsDictionaryForEach(smB->getters,
            // Butt ugly function cast
            (int (*) (const char *key, void *value,
                void *userData)) PrintParameterConnectionDotContent, &ps);
     }
}


static inline
void PrintStreamConnections(const struct QsGraph *graph, FILE *file,
        int graphNum) {


    for(struct QsBlock *b = graph->firstBlock; b; b = b->next) {
        if(b->isSuperBlock) continue;

        struct QsSimpleBlock *smB = (struct QsSimpleBlock *) b;
        if(smB->inputs == 0) {
            DASSERT(smB->numInputs == 0);
            continue;
        }

        for(uint32_t i = 0; i < smB->numInputs; ++i) {
            struct QsInput *input = smB->inputs + i;
            DASSERT(input);
            if(!input->feederBlock) continue;
            DASSERT(input->inputPortNum == i);
            DASSERT(input->feederBlock->outputs);
            DASSERT(input->feederBlock->numOutputs);
            fprintf(file, "  \"%d:%s:%" PRIu32
                    ":floW\" -> \"%d:%s:Input%" PRIu32 "\""
                    " [color=red];\n",
                    graphNum, input->feederBlock->block.name,
                    input->outputPortNum,

                    graphNum, b->name, i);
        }
    }
}


int qsGraphPrintDot(const struct QsGraph *g, FILE *f) {

    ASSERT(mainThread == pthread_self(), "Not graph main thread");
    DASSERT(g->flowState == QsGraphPaused);

    // TODO: make this be able to draw many graphs
    int graphNum = 0;

    int clusterNum = 0;

    fprintf(f,
            "digraph {\n"
            "  label=\"quickstream graph\";\n"
            "\n");

    for(struct QsBlock *b = g->firstBlock; b; b = b->next) {

        if(b->isSuperBlock) continue;

        struct QsSimpleBlock *smB = (struct QsSimpleBlock *) b;

        fprintf(f,
            "  subgraph cluster_%d {\n"
            "    label=\"%s\";\n",
                    clusterNum++, b->name);
    

        if(smB->numInputs || smB->numOutputs) {

            // Make a cluster for all output ports for this block
            //
            // BUG: We don't get smB->flush set until run is called.
            fprintf(f,
            "    subgraph cluster_%d {\n"
            "      label=\"flow()%s\";\n",
                    clusterNum++, (smB->flush)?" flush()":"");
            for(uint32_t i = smB->numInputs - 1; i != -1; --i)
                fprintf(f,
            "      \"%d:%s:Input%" PRIu32 "\" "
                    "[label=\"%" PRIu32 "\",shape=circle];\n",
                    graphNum, b->name, i, i);
            for(uint32_t i = 0; i<smB->numOutputs; ++i)
                // Print the output ports, connected or not.
                fprintf(f,
            "      \"%d:%s:%" PRIu32 ":floW\" "
                  "[label=\"%" PRIu32 "\",shape=diamond];\n",
                    graphNum, b->name, i, i);

            if(smB->numInputs == 0 && smB->numOutputs == 0)
                fprintf(f,
            "      \"%d:%s:%" PRIu32 ":floW\" "
                  "[label=\"no stream\nconnections\"];\n",
                    graphNum, b->name, 0);
            else if(smB->numInputs && smB->numOutputs) { 
                // Draw the pass-through buffer connections
                for(uint32_t i = 0; i < smB->numPassThroughs; ++i)
                    if(smB->passThroughs[i].inputPortNum <
                            smB->numInputs &&
                        smB->passThroughs[i].outputPortNum <
                            smB->numOutputs)
                            fprintf(f,
            "      \"%d:%s:Input%" PRIu32 "\" -> "
                    "\"%d:%s:%" PRIu32 ":floW\" "
                    "[color=blue];\n",
                    graphNum, b->name, smB->passThroughs[i].inputPortNum,
                    graphNum, b->name, smB->passThroughs[i].outputPortNum);
            }
            fprintf(f,
            "    }\n");
        }

        PrintParameters(CONSTANT, smB->getters, QsConstant,
                b->name, f, &clusterNum, graphNum);
        PrintParameters(GETTER, smB->getters, QsGetter,
                b->name, f, &clusterNum, graphNum);
        PrintParameters(SETTER, smB->setters, QsSetter,
                b->name, f, &clusterNum, graphNum);

        fprintf(f,
            "  }\n");

    }

    PrintGetterToSetterConnections(g, f, graphNum);
    PrintConstantAndSetterConnections(g, f, graphNum);
    PrintStreamConnections(g, f, graphNum);


    fprintf(f,
            "}\n");

    return 0;
}


int qsGraphPrintDotDisplay(const struct QsGraph *g, bool waitForDisplay) {

    ASSERT(mainThread == pthread_self(), "Not graph main thread");
    DASSERT(g->flowState == QsGraphPaused);

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

        if(qsGraphPrintDot(g, f))
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
        DASSERT(pid == 0, "I'm I a child process or not? WTF");
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
        exit(1);
    }

    return ret; // success ret=0


}
