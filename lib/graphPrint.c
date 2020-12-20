#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <setjmp.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <pthread.h>

#include "../include/quickstream/app.h" // public interfaces
#include "../include/quickstream/builder.h" // public interfaces
#include "../include/quickstream/block.h" // public interfaces


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
            "      label=\"%s %ss\";\n"
            "\n",
            *ps->clusterNum, ps->blockName, ps->desc);
        ps->gotOne = true;
        ++(*ps->clusterNum);
    }

    fprintf(ps->file,
            "      \"%s:%s:%s\" [label=\"%s\",color=%s];\n",
            ps->desc, ps->blockName, qsParameterGetName(p),
            qsParameterGetName(p), ParameterFillColor(p));

    return 0;
}


static inline
void
PrintParameters(const char *desc, struct QsDictionary *d,
    enum QsParameterKind kind, const char *blockName,
    FILE *file, int *clusterNum) {

    if(qsDictionaryIsEmpty(d))
        // No parameters to print
        return;

    struct PrintParameterStruct ps;
    ps.file = file;
    ps.kind = kind;
    ps.desc = desc;
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
            "  \"%s:%s:%s\" -> \"%s:%s:%s\" [color=blue];\n",
            GETTER, p->block->block.name, p->name,
            SETTER, s->block->block.name, s->name);
    }

    return 0;
}


static inline
void PrintGetterToSetterConnections(const struct QsGraph *g, FILE *file) {

     for(struct QsBlock *b = g->firstBlock; b; b = b->next) {
        if(b->isSuperBlock) continue;
        struct QsSimpleBlock *smB = (struct QsSimpleBlock *) b;

        struct PrintParameterConnectionStruct ps;
        ps.file = file;

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
            "  \"%s:%s:%s\" -> \"%s:%s:%s\" %scolor=green]\n",
            GetKindString(p), p->block->block.name, p->name,
            GetKindString(s), s->block->block.name, s->name,
            dir);
    }

    return 0;
}


static inline
void PrintConstantAndSetterConnections(const struct QsGraph *g, FILE *file) {

     for(struct QsBlock *b = g->firstBlock; b; b = b->next) {
        if(b->isSuperBlock) continue;
        struct QsSimpleBlock *smB = (struct QsSimpleBlock *) b;

        struct PrintParameterConnectionStruct ps;
        ps.file = file;

        qsDictionaryForEach(smB->getters,
            // Butt ugly function cast
            (int (*) (const char *key, void *value,
                void *userData)) PrintParameterConnectionDotContent, &ps);
     }

}


int qsGraphPrintDot(const struct QsGraph *g, FILE *f) {

    ASSERT(mainThread == pthread_self(), "Not graph main thread");
    DASSERT(g->flowState == QsGraphPaused);

    int blockNum = 0;
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
            "    label=\"%s\";\n"
            "\n", clusterNum++, b->name);
        if(smB->flow)
            fprintf(f,
            "    \"%s_floW_%d\" [label=\"flow()\"];\n",
                    b->name, blockNum);
        if(smB->flush)
            fprintf(f,
            "    \"%s_flusH_%d\" [label=\"flush()\"];\n",
                    b->name, blockNum);

        PrintParameters(CONSTANT, smB->getters, QsConstant,
                b->name, f, &clusterNum);
        PrintParameters(GETTER, smB->getters, QsGetter,
                b->name, f, &clusterNum);
        PrintParameters(SETTER, smB->setters, QsSetter,
                b->name, f, &clusterNum);


        fprintf(f,
            "  }\n");

        ++blockNum;
    }


    PrintGetterToSetterConnections(g, f);
    PrintConstantAndSetterConnections(g, f);

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
