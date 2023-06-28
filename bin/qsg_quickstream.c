// This code looks into private header files in libquickstream.so code.
//
// This is the only source file to quickstreamGUI that does that.

#include <stdlib.h>
#include <stddef.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include <dlfcn.h>
#include <stdatomic.h>
#include <signal.h>
#include <pthread.h>
#include <gtk/gtk.h>

#include "../include/quickstream.h"
#include "../lib/Dictionary.h"
#include "../lib/debug.h"

#include "../lib/c-rbtree.h"
#include "../lib/name.h"
#include "../lib/threadPool.h"
#include "../lib/block.h"
#include "../lib/graph.h"
#include "../lib/job.h"
#include "../lib/port.h"
#include "../lib/stream.h"
#include "../lib/parameter.h"
#include "../lib/config.h"

#include "quickstreamGUI.h"


static const struct Port *gotISWarn = false;


bool CanConnect(const struct Port *fromPort,
        const struct Port *toPort) {

    DASSERT(fromPort);
    DASSERT(toPort);
    DASSERT(fromPort->qsPort);
    DASSERT(toPort->qsPort);
    DASSERT(fromPort->terminal);
    DASSERT(toPort->terminal);
    // We should not let a user try to start a connection from
    // a stream input port with a graph port alias.
    DASSERT(!(fromPort->terminal->type == In && fromPort->graphPortAlias));


    if(toPort->terminal->type == In && toPort->graphPortAlias) {
        if(gotISWarn != toPort) {
            // Print one warning per aliased to port.
            fprintf(stderr, "You cannot connect to a stream "
                "input port that is a graph port alias\n");
            gotISWarn = toPort;
        }
        return false;
    } else if(gotISWarn)
        // Reset the warning spew
        gotISWarn = 0;


    if(toPort == fromPort)
        return false;

    return qsGraph_canConnect(fromPort->qsPort, toPort->qsPort);
}


// Set data in each array element of t->ports[] 
static int
MakeParameter(const char *name, struct QsPort *qsPort,
        struct Terminal *t) {

    struct Port *p = t->ports + t->numPorts;
    p->num = t->numPorts;

    DASSERT(!p->name);
    p->name = strdup(name);
    ASSERT(name, "strdup() failed");

    // We should never get one more than once.
    DASSERT(!p->qsPort);

    // Set the quickstream port in the GUI port.
    p->qsPort = qsPort;
    p->terminal = t;

    ++t->numPorts;

    return 0;
}


// Set data in each array element of t->ports[] 
static int
MakeOutput(const char *name, struct QsPort *qsPort,
        struct Terminal *t) {

    DASSERT(name);
    DASSERT(name[0]);
    DASSERT(qsPort);
    DASSERT(t);

    struct QsOutput *out = (void *) qsPort;
    struct Port *p;

    // The actual port must be to a simple block
    DASSERT(qsPort->block->type == QsBlockType_simple);

    if(t->block->qsBlock->type == QsBlockType_simple) {
        // In this case 
        DASSERT(((struct QsSimpleBlock *) qsPort->block)->streamJob);
        // We order the outputs in the output array order.
        // p points to the real quickstream port at the index of
        // out->portNum.
        p = t->ports + out->portNum;
        p->num = out->portNum;
        // The order we get the ports from the qsDictionaryForEach()
        // is not any particular order.
    } else {
        DASSERT(t->block->qsBlock->type == QsBlockType_super);
        // In this case the block it gets to it from is a super block so
        // the order of the GUI ports will not be anything in particular.
        //
        // TODO: Maybe we need to order them somehow.

        // In this super block case we use the numPorts as we increment it
        // in this function below:
        p = t->ports + t->numPorts;
        p->num = t->numPorts;
    }

    DASSERT(!p->name);
    p->name = strdup(name);
    ASSERT(name, "strdup() failed");

    // We should never get one more than once.
    DASSERT(!p->qsPort);

    // Set the quickstream port in the GUI port.
    p->qsPort = qsPort;
    p->terminal = t;

    struct QsStreamJob *j =
            ((struct QsSimpleBlock *) qsPort->block)->streamJob;
    DASSERT(j);

    if(out->portNum < j->minOutputs)
        p->isRequired = true;

    ++t->numPorts;

    return 0;
}


// Set data in each the array element t->ports[] 
static int
MakeInput(const char *name, struct QsPort *qsPort,
        struct Terminal *t) {

    DASSERT(name);
    DASSERT(name[0]);
    DASSERT(qsPort);
    DASSERT(t);
    DASSERT(t->type == In);

    struct QsInput *in = (void *) qsPort;
    struct Port *p;

    // The actual port must be to a simple block
    DASSERT(qsPort->block->type == QsBlockType_simple);

    if(t->block->qsBlock->type == QsBlockType_simple) {
        // In this case 
        DASSERT(((struct QsSimpleBlock *) qsPort->block)->streamJob);
        // We order the inputs in the input array order.
        // p points to the real quickstream port at the index of
        // in->portNum.
        p = t->ports + in->portNum;
        p->num = in->portNum;
        // The order we get the ports from the qsDictionaryForEach()
        // is not any particular order.
    } else {
        DASSERT(t->block->qsBlock->type == QsBlockType_super);

        // In this case the block it gets to it from is a super block so
        // the order of the GUI ports will not be anything in particular.
        //
        // TODO: Maybe we need to order them somehow.

        // In this super block case we use the numPorts as we increment it
        // in this function below:
        p = t->ports + t->numPorts;
        p->num = t->numPorts;
    }

    DASSERT(!p->name);
    p->name = strdup(name);
    ASSERT(name, "strdup() failed");

    // We should never get one more than once.
    DASSERT(!p->qsPort);

    // Set the quickstream port in the GUI port.
    p->qsPort = qsPort;
    p->terminal = t;

    struct QsStreamJob *j =
            ((struct QsSimpleBlock *) qsPort->block)->streamJob;
    DASSERT(j);

    if(in->portNum < j->minInputs)
        p->isRequired = true;

    ++t->numPorts;

    return 0;
}


// Counting ports in a port terminal GUI thingy.
static int
Count(const char *name, void *p, void *userData) {

    return 0;
}


// This allocates and sets the array of ports in the terminal.  The Block
// will free this memory in qsg_block.c, in the "block GUI" cleanup.  So
// struct Block owns the ports[] array memory in it.
void CreatePorts(struct Terminal *t) {

    DASSERT(t);
    DASSERT(t->block);
    DASSERT(t->block->qsBlock);
    DASSERT(t->numPorts == 0);
    DASSERT(t->ports == 0);

    //DSPEW("Block from: \"%s\"", qsBlock_getName(t->block->qsBlock));

    struct QsModule *m = Get_Module(t->block->qsBlock);
    DASSERT(m);
    struct QsPortDicts *ports = &m->ports;


    switch(t->type) {

        case In:
            if(qsDictionaryIsEmpty(ports->inputs))
                return;
            {
                uint32_t numPorts =
                        qsDictionaryForEach(ports->inputs, Count, 0);
                t->ports = calloc(numPorts, sizeof(*t->ports));
                ASSERT(t->ports, "calloc(%" PRIu32 ",%zu) failed",
                        numPorts,
                        sizeof(*t->ports));

                for(uint32_t i = 0; i < numPorts; ++i)
                    t->ports[i].terminal = t;
                ASSERT(numPorts == qsDictionaryForEach(ports->inputs,
                    (int (*) (const char *, void *,
                        void *)) MakeInput, t));
                DASSERT(t->numPorts == numPorts);
            }
            return;
        case Out:
            if(qsDictionaryIsEmpty(ports->outputs))
                return;
            {
                uint32_t numPorts =
                        qsDictionaryForEach(ports->outputs, Count, 0);
                DASSERT(numPorts);
                t->ports = calloc(numPorts, sizeof(*t->ports));
                ASSERT(t->ports, "calloc(%" PRIu32 ",%zu) failed",
                        numPorts,
                        sizeof(*t->ports));

                for(uint32_t i = 0; i < numPorts; ++i)
                    t->ports[i].terminal = t;
                ASSERT(numPorts == qsDictionaryForEach(ports->outputs,
                    (int (*) (const char *, void *,
                        void *)) MakeOutput, t));
                DASSERT(t->numPorts == numPorts);
            }
            return;
        case Set:
            if(qsDictionaryIsEmpty(ports->setters))
                return;
            {
                struct QsModule *m = Get_Module(t->block->qsBlock);
                DASSERT(m);
                struct QsPortDicts *ports = &m->ports;
                uint32_t numPorts =
                        qsDictionaryForEach(ports->setters, Count, 0);
                DASSERT(numPorts);
                t->ports = calloc(numPorts, sizeof(*t->ports));
                ASSERT(t->ports, "calloc(%" PRIu32 ",%zu) failed",
                        numPorts,
                        sizeof(*t->ports));

                for(uint32_t i = 0; i < numPorts; ++i)
                    t->ports[i].terminal = t;
                ASSERT(numPorts == qsDictionaryForEach(ports->setters,
                    (int (*) (const char *, void *,
                        void *)) MakeParameter, t));
                DASSERT(t->numPorts == numPorts);
            }
            return;
        case Get:
            if(qsDictionaryIsEmpty(ports->getters))
                return;
            {
                struct QsModule *m = Get_Module(t->block->qsBlock);
                DASSERT(m);
                struct QsPortDicts *ports = &m->ports;
                uint32_t numPorts =
                        qsDictionaryForEach(ports->getters, Count, 0);
                DASSERT(numPorts);
                t->ports = calloc(numPorts, sizeof(*t->ports));
                ASSERT(t->ports, "calloc(%" PRIu32 ",%zu) failed",
                        numPorts,
                        sizeof(*t->ports));

                for(uint32_t i = 0; i < numPorts; ++i)
                    t->ports[i].terminal = t;
                ASSERT(numPorts == qsDictionaryForEach(ports->getters,
                    (int (*) (const char *, void *,
                        void *)) MakeParameter, t));
                DASSERT(t->numPorts == numPorts);
            }
            return;
        default:
            ASSERT(0, "WTF");
            break;
    }
}


// Used in MakeConfigWidget()
//
struct ConfigAttribute {
    GtkWidget *grid;
    struct Block *block;
    gint gridCount;
};


struct ConfigBlockAttribute  {

    struct Block *block;
    const char *name; // attribute name
    GtkWidget *checkButton;
    GtkEntry *entry;
    struct QsAttribute *attribute;
    // We needed the check button to ignore event callbacks when we where
    // just reflecting state from the text entry widget.
    bool justReflecting;
};


static void DestroyWidget_cb(GtkWidget *ent,
        struct ConfigBlockAttribute *c) {

    //DSPEW("Cleaning up: %s:%s", c->block->qsBlock->name, c->name);

#ifdef DEBUG
    memset(c, 0, sizeof(*c));
#endif
    free(c);
}


// Is c a Separator character
//
static inline bool IsSep(char c) {

    return (c == ' ');
}


static void Config_cb(GtkEntry *ent,
        struct ConfigBlockAttribute *c) {

    DASSERT(c);
    DASSERT(c->attribute);

    DSPEW("Configuring: %s:%s",
            c->block->qsBlock->name,
            c->name);

    // Do not write to or free this returned string, str.
    const char *sent = gtk_entry_get_text(ent);

    const size_t nameL = strlen(c->name);

    // The entry string must start with the "attribute name":
    if(strncmp(c->name, sent, nameL + 1) != 0 && !IsSep(sent[nameL])) {
        fprintf(stderr, "Attribute name \"%s\" is not in entry: \"%s\"\n",
            c->name, sent);
        return;
    }

    // Copy the entry:
    const size_t len = strlen(sent) + 2; // 2==> '\0' and extra null
    DASSERT(len >= nameL);
    char *str = calloc(1, len);
    ASSERT(str, "calloc(1,%zu) failed", len);
    strcpy(str, sent);

    int argc = 0;
    for(char *s = str; *s;) {

        while(IsSep(*s)) ++s;

        if(*s)
            ++argc;

        char terminator = ' ';

        if(*s == '"') {
            terminator = '"';
            ++s;
        }   

        while(*s && *s != terminator)
            ++s;
        if(!*s) break;

        ++s;
    }
    DASSERT(argc >= 1);

    char **argv = malloc((argc + 1)*sizeof(*argv));
    ASSERT(argv, "malloc(%zu) failed", (argc + 1)*sizeof(*argv));

    // Same loop again, but mark the string segments.
    size_t i = 0;
    for(char *s = str; *s;) {

        while(IsSep(*s)) ++s;

        char terminator = ' ';
        if(*s == '"') {
            terminator = '"';
            // Skip to the next char
            ++s;
        }

        // set i-th arg
        argv[i++] = s;

        while(*s && *s != terminator)
            ++s;

        if(!*s) break;

        // Terminate argv[] string
        *s = '\0';

        ++s;
    }
    DASSERT(i == argc);

    // argv terminator (since malloc() does not zero it):
    argv[i] = 0;

#if 0
    {
        fprintf(stderr, "\n\nCONFIGURE ARGS[%d]=", argc);
        for(char **s = argv; *s; ++s)
            fprintf(stderr, " %s", *s);
        fprintf(stderr, "\n\n");
    }
#endif

    int ret = qsBlock_config(c->block->qsBlock,
            argc, (const char * const *) argv);

    fprintf(stderr, "CONFIGURING block \"%s\":\"%s\" returned %d\n",
            c->block->qsBlock->name, sent, ret);


    // Make the check button reflect the save state of the attribute.
    //
    // In this callback the check button state can only flip from unset to
    // set.
    //
    if(!ret || c->attribute->lastArgv) {
        // The attribute data should have been saved if ret == 0.
        DASSERT(!(!ret && !c->attribute->lastArgv));
        if(!gtk_toggle_button_get_active(
                    GTK_TOGGLE_BUTTON(c->checkButton))) {
            // Flip the check button, but do not let it resubmited the
            // configure command to the block.
            c->justReflecting = true;
            gtk_toggle_button_set_active(
                    GTK_TOGGLE_BUTTON(c->checkButton), true);
        }

        DASSERT(c->attribute->currentArgs);

        gtk_entry_set_text(GTK_ENTRY(c->entry), c->attribute->currentArgs);
    }
    if(ret) {
        if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
                        c->checkButton)))
            gtk_toggle_button_set_active(
                    GTK_TOGGLE_BUTTON(c->checkButton), false);
    }


#ifdef DEBUG
    memset(argv, 0, (argc + 1)*sizeof(*argv));
#endif
    free(argv);

#ifdef DEBUG
    memset(str, 0, len);
#endif
    free(str);
}


bool BlockHasConfigAttrubutes(const struct Block *b) {

    DASSERT(b);
    DASSERT(b->qsBlock);
    struct QsModule *m = Get_Module(b->qsBlock);
    ASSERT(m);

    if(!m->attributes ||
            qsDictionaryIsEmpty(m->attributes))
        return false;
    return true;
}


static
void SaveConfig_checkButton(GtkToggleButton *b,
        struct ConfigBlockAttribute *c) {

    DASSERT(b);
    DASSERT(c);
    DASSERT(GTK_TOGGLE_BUTTON(c->checkButton) == b);
    DASSERT(c->attribute);
    DASSERT(c->block);
    DASSERT(c->block->qsBlock);

    if(c->justReflecting) {
        // We just configured the block in the text entry callback so we
        // do not need to do it again.
        c->justReflecting = false;
        return;
    }

    // The user just clicked (on) or un-clicked (off) the check button.
    if(gtk_toggle_button_get_active(b)) {
        // The user clicked it on, so do the text entry callback.
        Config_cb(c->entry, c);
    } else if(c->attribute->lastArgv) {

        DSPEW("Freeing block \"%s\" saved attribute \"%s\"",
                c->block->qsBlock->name, c->name);
        // The user clicked it off, so remove the saved attribute from the
        // block, for when the graph is saved as a super block.
        qsGraph_removeConfigAttribute(c->block->qsBlock, c->name);
        // It should be gone now:
        DASSERT(!c->attribute->lastArgv);
    }
}


static int
MakeConfigWidget(const char *name, struct QsAttribute *a,
            struct ConfigAttribute *c) {

    DASSERT(a);
    DASSERT(c);
    DASSERT(c->grid);
    DASSERT(c->block);

    
    // Lots of widget-ness, but very straight forward.


    GtkWidget *w = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
    gtk_widget_set_name(w, "config");
    gtk_grid_attach(GTK_GRID(c->grid), w,
            0/*left*/, c->gridCount++/*top*/,
            2/*width*/, 1/*height*/);
    gtk_widget_show(w);


    GtkWidget *vbox =  gtk_box_new(GTK_ORIENTATION_VERTICAL,
            2/*spacing*/);

    w = gtk_text_view_new();
    gtk_widget_set_name(w, "configName");
    gtk_text_view_set_editable(GTK_TEXT_VIEW(w), FALSE);
    GtkTextBuffer *tb = gtk_text_view_get_buffer(GTK_TEXT_VIEW(w));
    gtk_text_buffer_set_text(GTK_TEXT_BUFFER(tb), name, -1);
    gtk_box_pack_start(GTK_BOX(vbox), w,
                FALSE/*expand*/, FALSE/*fill*/,
                3/*padding*/);
    gtk_widget_show(w);

    w = gtk_text_view_new();
    gtk_widget_set_name(w, "configDesc");
    gtk_text_view_set_editable(GTK_TEXT_VIEW(w), FALSE);
    tb = gtk_text_view_get_buffer(GTK_TEXT_VIEW(w));
    gtk_text_buffer_set_text(GTK_TEXT_BUFFER(tb), a->desc, -1);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(w), GTK_WRAP_WORD);
    gtk_widget_show(w);
    gtk_box_pack_start(GTK_BOX(vbox), w,
                FALSE/*expand*/, FALSE/*fill*/,
                3/*padding*/);

    gtk_grid_attach(GTK_GRID(c->grid), vbox,
            0/*left*/, c->gridCount/*top*/,
            1/*width*/, 1/*height*/);

    gtk_widget_show(vbox);


    vbox =  gtk_box_new(GTK_ORIENTATION_VERTICAL,
            2/*spacing*/);

    w = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
    gtk_widget_set_name(w, "config2");
    gtk_box_pack_start(GTK_BOX(vbox), w,
                TRUE/*expand*/, TRUE/*fill*/,
                3/*padding*/);
    gtk_widget_show(w);

    w = gtk_text_view_new();
    gtk_widget_set_name(w, "configDesc");
    gtk_text_view_set_editable(GTK_TEXT_VIEW(w), FALSE);
    tb = gtk_text_view_get_buffer(GTK_TEXT_VIEW(w));
    gtk_text_buffer_set_text(GTK_TEXT_BUFFER(tb), a->argSyntax, -1);
    gtk_box_pack_start(GTK_BOX(vbox), w,
            FALSE/*expand*/, FALSE/*fill*/,
            3/*padding*/);
    gtk_widget_show(w);


    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2/*spacing*/);

    // This allocation will get free()ed when the entry widget is
    // destroyed.
    struct ConfigBlockAttribute *cba = calloc(1, sizeof(*cba));
    ASSERT(cba, "calloc(1,%zu) failed", sizeof(*cba));
    cba->block = c->block;
    cba->name = name;
    cba->attribute = a;

    w = gtk_check_button_new_with_mnemonic("Do Save");
    cba->checkButton = w;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),
            (a->lastArgv)?TRUE:FALSE);
    g_signal_connect(cba->checkButton, "toggled",
            G_CALLBACK(SaveConfig_checkButton), cba);
    gtk_box_pack_start(GTK_BOX(hbox), w,
                FALSE/*expand*/, FALSE/*fill*/,
                3/*padding*/);
    gtk_widget_show(w);


    w = gtk_entry_new();
    cba->entry = GTK_ENTRY(w);
    g_signal_connect(w, "activate",
                G_CALLBACK(Config_cb), cba);
    g_signal_connect(w, "destroy",
                G_CALLBACK(DestroyWidget_cb), cba);
    gtk_entry_set_text(GTK_ENTRY(w), a->currentArgs);
    gtk_widget_set_name(w, "configEntry");
    gtk_box_pack_start(GTK_BOX(hbox), w,
                TRUE/*expand*/, TRUE/*fill*/,
                3/*padding*/);
    gtk_widget_show(w);

    gtk_box_pack_start(GTK_BOX(vbox), hbox,
                FALSE/*expand*/, FALSE/*fill*/,
                3/*padding*/);

    gtk_grid_attach(GTK_GRID(c->grid), vbox,
            1/*left*/, c->gridCount/*top*/,
            1/*width*/, 1/*height*/);

    gtk_widget_show(hbox);
    gtk_widget_show(vbox);


    ++c->gridCount;

    return 0; // continue
}

void MakeConfigWidgets(struct Block *b, GtkWidget *grid) {

    struct QsModule *m = Get_Module(b->qsBlock);
    ASSERT(m);

    DASSERT(!m->attributes || !qsDictionaryIsEmpty(m->attributes));

    struct ConfigAttribute ca;
    ca.grid = grid;
    ca.block = b;
    ca.gridCount = 0;

    ASSERT(qsDictionaryForEach(m->attributes,
            (int (*) (const char *key, void *value,
            void *userData)) MakeConfigWidget, &ca));
}


bool IsSuperBlock(const struct Block *b) {

    DASSERT(b);
    DASSERT(b->qsBlock);

    return (b->qsBlock->type == QsBlockType_super);
}


const char *GetThreadPoolName(const struct Block *b) {

    DASSERT(b);
    DASSERT(b->qsBlock);
    ASSERT(b->qsBlock->type == QsBlockType_simple);

    struct QsSimpleBlock *sb = (void *) b->qsBlock;

    DASSERT(sb->jobsBlock.threadPool);
    DASSERT(sb->jobsBlock.threadPool->name);
    DASSERT(sb->jobsBlock.threadPool->name[0]);

    return sb->jobsBlock.threadPool->name;
}


static
int GetBlockEntry(const char *name, struct QsBlock *b,
        GtkListStore *blockListModel) {

    DASSERT(name);
    DASSERT(b);
    DASSERT(blockListModel);
    if(b->type != QsBlockType_simple)
        return 0;

    struct QsSimpleBlock *sb = (void *) b;
    DASSERT(sb->jobsBlock.threadPool->name);

    GtkTreeIter iter;
    gtk_list_store_append(blockListModel, &iter);
    gtk_list_store_set(blockListModel, &iter,
                0,
                name,
                1,
                sb->jobsBlock.threadPool->name,
                2,
                b,
                -1);
    return 0;
}


static
int CountSimpleBlocks(const char *name, struct QsBlock *b,
        uint32_t *count) {

    DASSERT(name);
    DASSERT(b);
    DASSERT(count);

    if(b->type != QsBlockType_simple)
        return 0;

    ++(*count);
    return 0;
}


GtkListStore *GetBlockListModel(struct Layout *l) {

    struct QsGraph *g = l->qsGraph;
    DASSERT(g);

    if(qsDictionaryIsEmpty(g->blocks))
        return 0;

    uint32_t numSimpleBlocks = 0;

    ASSERT(qsDictionaryForEach(g->blocks,
                (int (*)(const char *, void *, void *))
                CountSimpleBlocks, &numSimpleBlocks) > 0);

    if(!numSimpleBlocks)
        return 0;

    GtkListStore *blockListModel = gtk_list_store_new(3,
            G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER);

    ASSERT(qsDictionaryForEach(g->blocks,
                (int (*)(const char *, void *, void *))
                    GetBlockEntry, blockListModel) > 0);

    return blockListModel;
}

static char *_GetThreadPoolName(const struct QsThreadPool *tp) {

    DASSERT(tp);
    DASSERT(tp->name);
    DASSERT(tp->name[0]);

    return tp->name;
}


static
int MakeThreadPoolCell_cb(const char *name, struct QsThreadPool *tp,
        GtkListStore *model) {

    GtkTreeIter iter;
    gtk_list_store_append(model, &iter);
    gtk_list_store_set(model, &iter, 0, _GetThreadPoolName(tp), -1);

    return 0; // continue looping
}


GtkTreeModel *MakeThreadPoolListStore(const struct Layout *l) {

    DASSERT(l);
    DASSERT(l->blocks);
    DASSERT(l->qsGraph);

    GtkListStore *model = gtk_list_store_new(1, G_TYPE_STRING);

    ASSERT(qsDictionaryForEach(l->qsGraph->threadPools,
            (int (*) (const char *key, void *tp,
            void *userData)) MakeThreadPoolCell_cb, model));

    return GTK_TREE_MODEL(model);
}



struct ModelCount {
    GtkListStore *model;
    uint32_t count;
};


static
int MakeThreadPoolCells_cb(const char *name, struct QsThreadPool *tp,
        struct ModelCount *mc) {

    GtkTreeIter iter;
    gtk_list_store_append(mc->model, &iter);
    gtk_list_store_set(mc->model, &iter,
            0, name,
            1, tp->maxThreads,
            2, tp, // This one is not rendered to the user GUI.
            -1);

    ++mc->count;

    return 0; // continue looping
}


GtkTreeModel *MakeThreadPoolsEditStore(const struct Layout *l,
        uint32_t *numThreadPools) {

    struct ModelCount mc;

    mc.model = gtk_list_store_new(3,
            G_TYPE_STRING, G_TYPE_UINT, G_TYPE_POINTER);
            // This seems to work too, but assumes that G_TYPE_UINT64 is a
            // pointer, which may not be good for a 32-bit port.
            //G_TYPE_STRING, G_TYPE_UINT, G_TYPE_UINT64);

    mc.count = 0;

    ASSERT(qsDictionaryForEach(l->qsGraph->threadPools,
            (int (*) (const char *key, void *tp,
            void *userData)) MakeThreadPoolCells_cb, &mc));

    if(numThreadPools)
        *numThreadPools = mc.count;

    return GTK_TREE_MODEL(mc.model);
}


struct FBHelper {
    struct QsPort *qsPort;
    bool gotIt;
};

int FindBlock(const char *name, struct QsPort *p,
        struct FBHelper *h) {

    if(p == h->qsPort) {
        h->gotIt = true;
        return 1;
    }
    return 0; // keep looking
}


// There is a GUI block and port in this layout, l
// that is connected in libquickstream.so to qsPort.
//
// The port we need may be connected by a super block port alias
// to a simple block qsPort.
//
static
struct Port *FindPort(struct Layout *l,
        struct QsPort *qsPort,
        enum Type type) {

    DASSERT(l);
    DASSERT(qsPort);
    DASSERT(type < NumTypes);
    DASSERT(type >= In);

    struct Block *b = l->blocks;
    struct FBHelper h = { .qsPort = qsPort, .gotIt = false };

    for(; b; b = b->next) {
        struct QsModule *m = Get_Module(b->qsBlock);
        struct QsDictionary *d;
        switch(type) {
            case In:
                d = m->ports.inputs;
                break;
            case Out:
                d = m->ports.outputs;
                break;
            case Set:
                d = m->ports.setters;
                break;
            case Get:
                d = m->ports.getters;
                break;
            default:
                ASSERT(0);
                break;
        }
        if(d) {
            qsDictionaryForEach(d,
                    (int (*)(const char *, void *, void *)) FindBlock,
                    &h);
            if(h.gotIt)
                break;
        }
    }

    ASSERT(b, "Failed to find GUI block");

    struct Terminal *t = b->terminals + type;

    for(uint32_t i = t->numPorts - 1; i != -1; --i) {
        struct Port *p = t->ports + i;
        if(p->qsPort == qsPort)
            return p;
    }

    ASSERT(0, "Failed to find port in GUI");
    return 0;
}


static struct QsParameter *GetLeaderParameter(struct QsGroup *g) {

    DASSERT(g);
    DASSERT(g->sharedPeers);
    DASSERT(*g->sharedPeers);

    if(g->getter) return ((void *) g->getter);

    return  (void *)
        ((*g->sharedPeers) - offsetof(struct QsSetter, job));
}

struct Helper {

    const char *name;
    void *ptr;
};

static int
FindPtr(const char *name, const void *ptr, struct Helper *h) {

    DASSERT(name);
    DASSERT(name[0]);

    if(ptr == h->ptr) {
        h->name = name;
        return 1; // finish
    }
    return 0; // continue
}

static inline
const char *DictionaryFindName(struct QsDictionary *d, void *ptr) {

    struct Helper h = { .name = 0, .ptr = ptr };

    qsDictionaryForEach(d,
            (int (*)(const char *, void *, void *)) FindPtr,
            &h);

    return h.name;
}


void DisplayBlocksAndConnections(struct Layout *l) {

    DASSERT(l);
    DASSERT(l->qsGraph);

    // 1. Get the GUI super block metadata "quickstreamGUI", if there is
    // any.
    //
    size_t mSize = 0;
    // The block position dictionary
    struct QsDictionary *posDict = 0;
    char *mem = qsGraph_getMetaData(l->qsGraph, "quickstreamGUI", &mSize);
    if(mem) {
    
        uint32_t numBlocks = 0;
        DASSERT(mSize);

        // Count the number of block names.
        char *s = (char *) mem;
        size_t len = 0;
        // Look through the strings that are block names.
        for(;len < mSize && *s;) {
            size_t l = strlen(s) + 1;
            len += l;
            s += l;
            ++numBlocks;
        }
        // One more 0 terminator.
        ++len;

        uint32_t padding = 0;
        if(len % 8)
            padding = len % 8;

        if(numBlocks && (len + padding +
                    numBlocks*sizeof(struct GUILayout)) <= mSize) {

            struct GUILayout *gl =
                (struct GUILayout *) (mem + len + padding);
            posDict = qsDictionaryCreate();

            // We'll looks through the strings (block names) again.
            s = (char *) mem;
            for(uint32_t i = 0; i < numBlocks; ++i) {
                // qsDictionaryInsert() could fail if a block name
                // repeats.  So we just try.
                qsDictionaryInsert(posDict, s, gl + i, 0);
                s += strlen(s) + 1;
            }
        } else
            WARN("Bad quickstreamGUI metadata found in "
                    "loaded super block");
    }

    // 2. First make the block widgets.  We can't make connections until
    //    the block widgets are made.
    //
    for(struct QsBlock *b = l->qsGraph->parentBlock.firstChild;
            b; b = b->nextSibling) {
        // Defaults:
        double x = -1.0, y = -1.0;
        uint8_t orientation = 0;
   
        if(posDict) {
            struct GUILayout *gl = (struct GUILayout *)
                qsDictionaryFind(posDict, qsBlock_getName(b));

            if(gl) {
                x = gl->x;
                y = gl->y;
                orientation = gl->orientation;
            }
        }
        struct QsModule *m = Get_Module(b);
        DASSERT(m);
        DASSERT(m->fileName);
        CreateBlock(l, m->fileName, b, x, y, orientation,
                2 /*0 unselectAllButThis,
                   1 add this block to selected
                   2 no selection change */);
    }

    // 3.  Now connect the block terminal widgets, the ones that already
    //     have libquickstream.so connections, but not GUI connections.
    //
    for(struct Block *b = l->blocks; b; b = b->next) {

        DASSERT(b->qsBlock);

        // Terminal Types

        ////////////////////////////////////////////////////////////////
        // In     stream input  connects to output
        ////////////////////////////////////////////////////////////////
        struct Terminal *t = b->terminals + In;
        DASSERT(t->block == b);
        DASSERT(t->type == In);
        if(t->ports) {
            DASSERT(t->numPorts);
            for(uint32_t i = 0; i < t->numPorts; ++i) {
                struct Port *inP = t->ports + i;
                // Since we are connecting by way of stream inputs and not
                // outputs there should be no In GUI connection yet for
                // this port.
                DASSERT(!inP->cons.numConnections);
                DASSERT(!inP->cons.connections);
                struct QsInput *input = (void *) inP->qsPort;
                if(input->output) {
                    // We have this input connected.
                    //
                    // Make a GUI connection without making the
                    // libquickstream connection.
                    AddConnection(inP,
                            FindPort(l, &input->output->port, Out),
                            false/*qsConnect ?*/);
                }
            }
        }

        ////////////////////////////////////////////////////////////////
        // Set     control parameters  connects Getter too   
        ////////////////////////////////////////////////////////////////
        // There can be only one getter in a parameter connection group
        // and that getter will be the leader of the group.
        t = b->terminals + Set;
        DASSERT(t->block == b);
        DASSERT(t->type == Set);
        if(t->ports) {
            DASSERT(t->numPorts); // FUCK FUCK FUCK xxxxxxxxxx---------
            for(uint32_t i = 0; i < t->numPorts; ++i) {
                struct Port *p = t->ports + i;
                DASSERT(p->qsPort);
                DASSERT(p->qsPort->portType == QsPortType_setter);
                struct QsParameter *qsP = (void *) p->qsPort;
                if(!qsP->group)
                    // Not connected.
                    continue;
                // This parameter is connected.
                struct QsParameter *leadParameter =
                    GetLeaderParameter(qsP->group);
                if(leadParameter == qsP) continue;
                // This parameter is not the lead parameter.
                enum Type ltype =
                    (leadParameter->port.portType == QsPortType_getter)?
                    Get:Set;
                AddConnection(FindPort(l, &leadParameter->port, ltype),
                        p, false);
            }
        }
    }

    // 4. Now add the graphPortAliases to the Ports.
    //    We mark only Ports with graph aliases.
    //
    for(struct Block *b = l->blocks; b; b = b->next) {
        for(enum Type type = 0; type < NumTypes; ++type) {
            struct Terminal *t = b->terminals + type;
            struct QsDictionary *d;
            // TODO: Kind of stupid code here:
            switch(t->type) {
                case In:
                    d = l->qsGraph->ports.inputs;
                    break;
                case Out:
                    d = l->qsGraph->ports.outputs;
                    break;
                case Set:
                    d = l->qsGraph->ports.setters;
                    break;
                case Get:
                    d = l->qsGraph->ports.getters;
                    break;
                default:
                    ASSERT(0);
                    break;
            }
            if(!d) continue;

            for(uint32_t i = t->numPorts-1; i != -1; --i) {
                struct Port *p = t->ports + i;
                DASSERT(!p->graphPortAlias);
                // 0 if not found.
                // the port alias name if found.
                const char *n = DictionaryFindName(d, p->qsPort);
                if(n) {
                    p->graphPortAlias = strdup(n);
                    ASSERT(p->graphPortAlias, "strdup() failed");
                }
            }
        }
    }

    if(mem) {
#ifdef DEBUG
        memset(mem, 0, mSize);
#endif
        free(mem);
    }

    if(posDict)
        qsDictionaryDestroy(posDict);
}


const char *GetBlockFileName(struct Block *b) {

    DASSERT(b);
    DASSERT(b->qsBlock);

    struct QsModule *m = Get_Module(b->qsBlock);

    return m->fileName;
}


static
void QsFeedback(struct QsGraph *g, enum Feedback fb,
        struct Layout *l) {

    DASSERT(l);
    DASSERT(l->qsGraph);
    DASSERT(l->qsGraph->feedback == (void *) QsFeedback);


    switch(fb) {
        case QsStart:
            DSPEW("Got graph feedback %d = QsStart", fb);
            SetRunButton(l, true);
            break;
        case QsStop:
            DSPEW("Got graph feedback %d = QsStop", fb);
            SetRunButton(l, false);
            break;
        case QsHalt:
            DSPEW("Got graph feedback %d = QsHalt", fb);
            SetHaltButton(l, true);
            break;
        case QsUnhalt:
            DSPEW("Got graph feedback %d = QsUnhalt", fb);
            SetHaltButton(l, false);
            break;
        default:
            ASSERT(0, "WTF");
    }
}


void SetQsFeedback(struct Layout *l) {

    DASSERT(l);
    DASSERT(l->qsGraph);
    DASSERT(l->qsGraph->feedback == 0);

    l->qsGraph->feedback = (void *) QsFeedback;
    l->qsGraph->fbData = l;
}


enum QsPortType GetQsPortType(const struct Port *p) {

    DASSERT(p);
    DASSERT(p->qsPort);
    return p->qsPort->portType;
}



void RemoveGraphPortAlias(struct Port *port) {
    DASSERT(port);
    DASSERT(port->graphPortAlias);
    DASSERT(port->terminal);
    DASSERT(port->terminal->block);
    DASSERT(port->terminal->block->layout);
    DASSERT(port->terminal->block->layout->qsGraph);
    DASSERT(port->qsPort);

    qsGraph_removePortAlias(
            port->terminal->block->layout->qsGraph,
            GetQsPortType(port),
            port->graphPortAlias);

#ifdef DEBUG
    memset(port->graphPortAlias, 0, strlen(port->graphPortAlias));
#endif
    free(port->graphPortAlias);
    port->graphPortAlias = 0;
}



static inline
const char *GetQsPortTypeAsString(const struct Port *p) {

    DASSERT(p);
    DASSERT(p->qsPort);

    switch(p->qsPort->portType) {
        case QsPortType_input:
            return "i";
        case QsPortType_output:
            return "o";
        case QsPortType_setter:
            return "s";
        case QsPortType_getter:
            return "g";
        default:
            ASSERT(0);
            break;
    }

    return 0;
}

static inline
const char *GetBlockNameFromQsPort(const struct QsPort *p) {

    DASSERT(p);
    DASSERT(p->block);
    DASSERT(p->block->name);
    DASSERT(p->block->name[0]);

    return p->block->name;
}

static inline
const char *GetPortNameFromQsPort(const struct QsPort *p) {

    DASSERT(p);
    DASSERT(p->name);
    DASSERT(p->name[0]);

    return p->name;
}


// This just tries to make a graph port alias if it does
// not succeed, it does the right thing.
//
void MakeGraphPortAlias(struct Port *port, const char *name) {
    DASSERT(port);
    DASSERT(!port->graphPortAlias);
    DASSERT(port->qsPort);
    DASSERT(port->terminal);
    DASSERT(port->terminal->block);
    DASSERT(port->terminal->block->layout);
    DASSERT(port->terminal->block->layout->qsGraph);

    if(0 == qsBlock_makePortAlias(
                port->terminal->block->layout->qsGraph,
                GetBlockNameFromQsPort(port->qsPort),
                GetQsPortTypeAsString(port),
                GetPortNameFromQsPort(port->qsPort),
                name)) {
        port->graphPortAlias = strdup(name);
        ASSERT(port->graphPortAlias, "strdup() failed");
    }
}
