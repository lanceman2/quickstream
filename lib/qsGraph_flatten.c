#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
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
#include "config.h"
#include "port.h"



static
int RenameBlock(const char *oldName, struct QsBlock *b,
        struct QsDictionary *newDict) {

    DASSERT(oldName);
    DASSERT(b->name);
    DASSERT(0 == strcmp(oldName, b->name));

    // Remove the prefix "superBlockName:"
    //
    while(*oldName && *oldName != ':')
        ++oldName;
    DASSERT(*oldName == ':');
    ++oldName;
    DASSERT(*oldName);

    char *newName = strdup(oldName);

    DZMEM(b->name, strlen(b->name));
    free(b->name);
    b->name = newName;

    ASSERT(qsDictionaryInsert(newDict, newName, b, 0) == 0);

    return 0;
}


static int
FixPortParent(const char *name, struct QsPort *p,
        struct QsParentBlock *oldParent) {

    if(p->parentBlock == oldParent)
        // We make it like the graph called the configure of this
        // attribute, in place of the super block that we are flattening
        // out of existence.
        p->parentBlock = (void *) oldParent->block.graph;

    return 0;
}


static int
FixAttributeParent(const char *name, struct QsAttribute *a,
        struct QsParentBlock *oldParent) {

    if(a->parentBlock == oldParent)
        // We make it like the graph called the configure of this
        // attribute, in place of the super block that we are flattening
        // out of existence.
        a->parentBlock = (void *) oldParent->block.graph;

    return 0;
}


// This is a little wonky.
//
// We must change lots of the data in the block data structures.  One
// hopes that we keep this consistent with qsGraph_createBlock().  It
// maybe we can mash/share some of this code with relevant pieces of
// qsGraph_createBlock().
//
// TODO: What about super block's undeclare()?  Maybe not have undeclare()
// for super blocks.
//
void qsGraph_flatten(struct QsGraph *g) {

    DASSERT(g);
    DASSERT(g->parentBlock.block.type == QsBlockType_top);
    struct QsParentBlock *p = (void *) g;
    // There should be just one child
    ASSERT(p->firstChild == p->lastChild);
    struct QsBlock *b = p->firstChild;
    if(b->type != QsBlockType_super)
        return;

    struct QsPortDicts *ports = &g->ports;

    struct QsSuperBlock *sb = (void *) p->firstChild;
    DASSERT(sb);
    DASSERT(sb->parentBlock.block.nextSibling == 0);
    struct QsModule *m = &sb->module;

    // Replace the graph's port aliases with the super blocks ports
    // aliases.  The graph's port aliases will become super block
    // aliases again if qsGraph_saveSuperBlock() is called.
    //
    DASSERT(ports->inputs);
    DASSERT(ports->outputs);
    DASSERT(ports->setters);
    DASSERT(ports->getters);
    //
    DASSERT(qsDictionaryIsEmpty(ports->inputs));
    DASSERT(qsDictionaryIsEmpty(ports->outputs));
    DASSERT(qsDictionaryIsEmpty(ports->setters));
    DASSERT(qsDictionaryIsEmpty(ports->getters));
    //
    qsDictionaryDestroy(ports->inputs);
    qsDictionaryDestroy(ports->outputs);
    qsDictionaryDestroy(ports->setters);
    qsDictionaryDestroy(ports->getters);
    //
    // Make the super block aliases be the graph aliases.
    ports->inputs = m->ports.inputs;
    ports->outputs = m->ports.outputs;
    ports->setters = m->ports.setters;
    ports->getters = m->ports.getters;
    //
    DASSERT(ports->inputs);
    DASSERT(ports->outputs);
    DASSERT(ports->setters);
    DASSERT(ports->getters);

    p->firstChild = sb->parentBlock.firstChild;
    p->lastChild = sb->parentBlock.lastChild;

    for(b = sb->parentBlock.firstChild; b; b = b->nextSibling) {
        DASSERT(b->graph == g);
        DASSERT(b->parentBlock == &sb->parentBlock);
        // Re-parent this block, b.
        b->parentBlock = p;

        struct QsModule *m = Get_Module(b);

        // Re-parent attributes
        if(m->attributes)
            ASSERT(qsDictionaryForEach(m->attributes,
                        (int (*) (const char *key, void *value,
                        void *userData)) FixAttributeParent, sb) >= 0);

        struct QsPortDicts *ports = &m->ports;

        // Re-parent stream input ports
        if(ports->inputs)
            ASSERT(qsDictionaryForEach(ports->inputs,
                        (int (*) (const char *key, void *value,
                        void *userData)) FixPortParent, sb) >= 0);

        // Re-parent control parameter setter ports
        if(ports->setters)
            ASSERT(qsDictionaryForEach(ports->setters,
                        (int (*) (const char *key, void *value,
                        void *userData)) FixPortParent, sb) >= 0);

        // Re-parent control parameter getter ports
        if(ports->getters)
            ASSERT(qsDictionaryForEach(ports->getters,
                        (int (*) (const char *key, void *value,
                        void *userData)) FixPortParent, sb) >= 0);

        // We do not need to re-parent output ports.
        //
        //
        // See source to qsGraph_saveSuperBlock() in
        // qsGraph_saveSuperBlock.c.  We get what connections are made
        // from just the port->parentBlock pointers in the input, setter,
        // and getter ports; because inputs always just connect to one
        // output.
    }


    // Remake g->blocks:
    //
    // We need to change the names of all the blocks in the graph, so
    // we make a new dictionary.
    struct QsDictionary *newBlocks = qsDictionaryCreate();
    //
    ASSERT(0 == qsDictionaryInsert(newBlocks, g->name, g, 0));

    ASSERT(0 == qsDictionaryRemove(g->blocks,
                sb->parentBlock.block.name));
    ASSERT(0 == qsDictionaryRemove(g->blocks, g->name));

    if(!qsDictionaryIsEmpty(g->blocks))
        ASSERT(qsDictionaryForEach(g->blocks,
                (int (*) (const char *, void *,
                    void *)) RenameBlock, newBlocks) > 0);

    qsDictionaryDestroy(g->blocks);
    g->blocks = newBlocks;


    if(m->attributes)
        qsDictionaryDestroy(m->attributes);


    if(m->dlhandle) {
        dlclose(m->dlhandle);
        m->dlhandle = 0;
    }

    DASSERT(m->fileName);
    FreeString(&m->fileName);
    FreeString(&m->fullPath);

    DZMEM(sb->parentBlock.block.name,
            strlen(sb->parentBlock.block.name));
    free(sb->parentBlock.block.name);

    DZMEM(sb, sizeof(*sb));
    free(sb);
}
