#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
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
#include "parameter.h"
#include "stream.h"

#include "graphConnect.h"



static
int PrintPort(const char *key, struct QsPort *port, FILE *file) {

    // For a simple blocks' port dictionary the port->name is the
    // same as the key, but this is from a super block port dictionary
    // the key is not necessarily the same as the port->name and
    // the port is a port from a simple block.

    fprintf(file, "%s\n", key);
    return 0;
}

static
int PrintAliasPort(const char *key, struct QsPort *port, FILE *file) {

    // For a simple blocks' port dictionary the port->name is the
    // same as the key, but this is from a super block port dictionary
    // the key is not necessarily the same as the port->name and
    // the port is a port from a simple block.

    fprintf(file, "%s --> %s %s\n", key,
            port->block->name,
            port->name);
    return 0;
}


int qsBlock_printPorts(struct QsBlock *b, FILE *file) {

    NotWorkerThread();
    ASSERT(b);
    struct QsGraph *g = b->graph;
    DASSERT(g);
    CHECK(pthread_mutex_lock(&g->mutex));

    int (*printPort)(const char *key, struct QsPort *port, FILE *file);

    if(b->type == QsBlockType_simple) {
        fprintf(file,
"\n"
" Ports for simple block: \"%s\"\n", b->name);
        printPort = PrintPort;
    } else if(b->type == QsBlockType_super) {
        fprintf(file,
"\n"
" Port aliases for super block: \"%s\"\n"
" alias --> block_name port_name\n",
            b->name);
        printPort = PrintAliasPort;
    } else
        ASSERT(0, "Bad block type");


    fprintf(file,
"************* setter ****************\n"
"*************************************\n");
    qsDictionaryForEach(GetDictionary(b, QsPortType_setter),
            (int (*) (const char *, void *, void *)) printPort, file);

    fprintf(file,
"************* getter ****************\n"
"*************************************\n");
    qsDictionaryForEach(GetDictionary(b, QsPortType_getter),
            (int (*) (const char *, void *, void *)) printPort, file);

    fprintf(file,
"************* input *****************\n"
"*************************************\n");
    qsDictionaryForEach(GetDictionary(b, QsPortType_input),
            (int (*) (const char *, void *, void *)) printPort, file);

    fprintf(file,
"************* output ****************\n"
"*************************************\n");
    qsDictionaryForEach(GetDictionary(b, QsPortType_output),
            (int (*) (const char *, void *, void *)) printPort, file);

    fprintf(file, "\n");

    CHECK(pthread_mutex_unlock(&g->mutex));
    return 0;
}

