#include <gtk/gtk.h>

#include "../include/quickstream.h"

#include "../lib/debug.h"

#include "quickstreamGUI.h"


// points to all selected blocks, layout->selectedBlocks, when it is
// set.
struct Block *movingBlocks = 0;

// If the blocks are moved via the left mouse button we need to know
// so that we can tell that the left mouse button press was a block
// move and not a double left button press.
bool blocksMoved = false;

// For moving the blocks.
gdouble x_root, y_root;


// Makes sure that the block is selected.
void SelectBlock(struct Block *block) {

    DASSERT(block);
    struct Layout *l = block->layout;
    DASSERT(l);

    if(block->isSelected) {
        // It's already selected

        // There should be a list.
        DASSERT(l->selectedBlocks);
        return;
    }

    DASSERT(block->nextSelected == 0);

    // Add block to the list at the top.
    block->nextSelected = l->selectedBlocks;
    l->selectedBlocks = block;

    block->isSelected = true;
    gtk_widget_set_name(block->grid, "block_selected");
}


// Makes sure that the block is not selected.
void UnselectBlock(struct Block *block) {

    DASSERT(block);
    struct Layout *l = block->layout;
    DASSERT(l);

    if(!block->isSelected) {
        // It's already NOT selected

        // There should be no next.
        DASSERT(!block->nextSelected);
        return;
    }

    struct Block *prev = 0;
    struct Block *b = l->selectedBlocks;
    for(; b; b = b->nextSelected) {
        DASSERT(b->layout == l);

        if(b == block) {
            // remove from list
            if(prev)
                prev->nextSelected = b->nextSelected;
            else
                l->selectedBlocks = b->nextSelected;
            DASSERT(b->isSelected);
            b->isSelected = false;
            b->nextSelected = 0;
            if(b->grid)
                // In this case we are unselecting a block that is
                // not in the process of being destroyed.
                gtk_widget_set_name(b->grid, "block_regular");
            break;
        }
        prev = b;
    }
    DASSERT(b == block);
}


// Remove all the blocks from being selected, accept for sb.
//
// If sb, make sure it is selected.
//
void UnselectAllBlocks(struct Layout *l, struct Block *sb) {

    DASSERT(l);

    struct Block *b = l->selectedBlocks;
    while(b) {
        // Edit list while we go through it so get next ahead of removing.
        struct Block *next = b->nextSelected;
        if(b != sb) {
            DASSERT(b->isSelected);
            UnselectBlock(b);
        }
        b = next;
    }

    // There should be no other block selected.
    DASSERT(!sb || sb->nextSelected == 0);

    if(sb)
        SelectBlock(sb);
}


static inline
void CheckForValuePopover(struct Terminal *t) {

    if(setValuePopover && t->drawingArea ==
                    gtk_popover_get_relative_to(
                            GTK_POPOVER(setValuePopover)))
        //
        // Not doing this was a BUG that left setValuePopover set to
        // an invalid pointer.
        //
        // This is the case where the setValuePopover widget is a child of
        // the terminal drawing area; so when we destroy the drawing area
        // (with the block stuff) the setValuePopover will get destroyed
        // too.  We could re-parent the setValuePopover, but then, to what
        // block and terminal drawing area?; so we'll just let it get
        // re-made and mark this setValuePopover pointer as needing to be
        // recreated like so:
        setValuePopover = 0;
}


static inline void FreePorts(struct Terminal *t) {

    if(t->numPorts) {

        DASSERT(t->ports);

        for(uint32_t i = t->numPorts - 1; i != -1; --i) {

            struct Port *p = t->ports + i;

            if(p->graphPortAlias) {

                DASSERT(p->terminal);
                DASSERT(p->terminal->block);
                DASSERT(p->terminal->block->layout);
                DASSERT(p->terminal->block->layout->qsGraph);
                DASSERT(p->qsPort);

                qsGraph_removePortAlias(
                    p->terminal->block->layout->qsGraph,
                    GetQsPortType(p),
                    p->graphPortAlias);
#ifdef DEBUG
                memset(p->graphPortAlias, 0, strlen(p->graphPortAlias));
#endif
                free(p->graphPortAlias);
                p->graphPortAlias = 0;
            }
            DASSERT(p->name);
#ifdef DEBUG
            memset(p->name, 0, strlen(p->name));
#endif
            free(p->name);
       }

#ifdef DEBUG
        memset(t->ports, 0, t->numPorts*sizeof(*t->ports));
#endif
        free(t->ports);
    }
#ifdef DEBUG
    else
        ASSERT(!t->ports);
#endif
}


static void Destroy_cb(GtkWidget *parent,
               struct Block *b) {

    CheckForValuePopover(b->terminals + In);
    CheckForValuePopover(b->terminals + Out);
    CheckForValuePopover(b->terminals + Set);
    CheckForValuePopover(b->terminals + Get);
}


static void DestroyBlock_cb(struct Block *b) {

    DASSERT(b);
    DASSERT(b->layout);
    DASSERT(b->layout->blocks);
    DASSERT(b->qsBlock);

    // Remove it from the selection list.
    b->parent = 0;
    b->grid = 0;
    UnselectBlock(b);

    // Remove the block from the layout->blocks list.
    struct Block *prev = 0;
    struct Block *bl = b->layout->blocks;
    for(; bl; bl = bl->next) {
        if(bl == b) break;
        else prev = bl;
    }
    // It must be in the list:
    DASSERT(bl == b);
    if(prev)
        prev->next = b->next;
    else
        b->layout->blocks = b->next;

    for(enum Type i = 0; i < NumTypes; ++i) {
        struct Terminal *t = b->terminals + i;
        for(uint32_t j = t->numPorts - 1; j != -1; --j) {
            struct Port *p = t->ports + j;
            if(p->cons.numConnections)
                Disconnect(p);
        }
    }

    DASSERT(b->cons.numConnections == 0);

    FreePorts(b->terminals + In);
    FreePorts(b->terminals + Out);
    FreePorts(b->terminals + Set);
    FreePorts(b->terminals + Get);


    qsBlock_destroy(b->qsBlock);

#ifdef DEBUG
    memset(b, 0, sizeof(*b));
#endif
    free(b);

    DSPEW();
}


static inline
void SetOrientation(struct Block *b, uint8_t orientation) {

    DASSERT(b);
    struct Terminal *t = b->terminals;

    t[In].pos = orientation & 03;
    t[Out].pos = (orientation & (03 << 2)) >> 2;
    t[Set].pos = (orientation & (03 << 4)) >> 4;
    t[Get].pos = (orientation & (03 << 6)) >> 6;
}


void PopUpBlock(struct Block *block) {

    DASSERT(block);
    DASSERT(block->layout);
    DASSERT(block->layout->layout);
    DASSERT(block->parent);

    g_object_ref(block->parent);
    gtk_container_remove(GTK_CONTAINER(block->layout->layout),
            block->parent);
    gtk_layout_put(GTK_LAYOUT(block->layout->layout),
            block->parent, block->x, block->y);
    g_object_unref(block->parent);
}


#if 0
static gboolean
Enter_cb(GtkWidget *w, GdkEventCrossing *e, struct Block *b) {

    DASSERT(w);
    DASSERT(e);
    DASSERT(b);
    DASSERT(b->layout);


    DSPEW();
    return FALSE; // FALSE => call more handlers.
}


static gboolean
Leave_cb(GtkWidget *w, GdkEventCrossing *e, struct Block *b) {

    DASSERT(w);
    DASSERT(e);
    DASSERT(b);
    DASSERT(b->layout);

    DSPEW();
    return FALSE; // FALSE => call more handlers.
}
#endif


static gboolean
Motion_cb(GtkWidget *w, GdkEventMotion *e, struct Block *b) {

    DASSERT(w);
    DASSERT(e);
    DASSERT(b);
    DASSERT(b->layout);

    //DSPEW();

    if(e->type != GDK_MOTION_NOTIFY)
        // WTF
        return FALSE;


    if(fromPort) {

        // We cannot let the event get passed down to the layout
        // since the e x,y coordinates are relative to the block.
        // GTK3 is not good about making event coordinates relative
        // to a widget, when the widget gets the event after another
        // widget passes it down to it.
        HandleConnectingDragEvent(e, b->layout, e->x + b->x, 
                e->y + b->y, NumPos);

        return TRUE; // TRUE => eat the event
    }

    if(!movingBlocks)
        return TRUE;

    // We have a block we are moving
 
    if(!(e->state & GDK_BUTTON1_MASK)) {
        // This really should not happen, but it does.  Sometimes the
        // button is not pressed any more without getting a button release
        // event.  WTF.
        PopUpBlock(movingBlocks);
        SetWidgetCursor(movingBlocks->layout->layout, "default");
        movingBlocks = 0;
        return TRUE;
    }


    return FALSE; // FALSE => call more handlers.
}

static gboolean
Press_cb(GtkWidget *w, GdkEventButton *e, struct Block *b) {

    DASSERT(w);
    DASSERT(e);
    DASSERT(b);
    DASSERT(b->layout);

    //DSPEW("e->type=%d  GDK_DOUBLE_BUTTON_PRESS=%d GDK_BUTTON_PRESS=%d",
    //          e->type, GDK_DOUBLE_BUTTON_PRESS, GDK_BUTTON_PRESS);

    if(e->type != GDK_BUTTON_PRESS && e->type != GDK_DOUBLE_BUTTON_PRESS)
        return FALSE;

    // GDK has mouse buttons numbered from 1 to 5.

    if(e->button == 1/*left mouse*/) {

        if(e->type == GDK_DOUBLE_BUTTON_PRESS) {

            if(movingBlocks) {
                PopUpBlock(movingBlocks);
                movingBlocks = 0;
            }
            SetWidgetCursor(b->layout->layout, "default");

            ShowBlockConfigWindow(b);
            return TRUE;
        }

        // We cannot pop up the block until after the user finishes moving
        // it.  To pop up the block we must detach the GTK widget from its
        // parent which causes this event to lose the grab on the pointer
        // (mouse) device, which fucks up the whole click and grab
        // thing.  Man, it was hard to find that bug...

        if(e->state & GDK_CONTROL_MASK || e->state & GDK_SHIFT_MASK)
            SelectBlock(b);
        else if(!b->isSelected)
            UnselectAllBlocks(b->layout, b);

        movingBlocks = b->layout->selectedBlocks;
        blocksMoved = false;
        DASSERT(movingBlocks);

        for(struct Block *bl = movingBlocks; bl; bl = bl->nextSelected) {
            bl->x0 = bl->x;
            bl->y0 = bl->y;
        }
        x_root = e->x_root;
        y_root = e->y_root;

        // We will do this when the blocks start to move and
        // not before:ShowBlockPopupMenu
        //SetWidgetCursor(b->layout->layout, "grabbing");

        return TRUE; // TRUE => eat the event.
    }

    if(e->button == 3/*right mouse*/) {
        movingBlocks = 0;
        ShowBlockPopupMenu(b);
        // We cannot pop up the block now but
        // we will after the popup menu is done.
        return TRUE;
    }

    DSPEW();
    return FALSE; // FALSE => call more handlers.
}


static gboolean
Release_cb(GtkWidget *w, GdkEventButton *e, struct Block *b) {
    DASSERT(b);
    DASSERT(b->layout);

    if(e->type != GDK_BUTTON_RELEASE)
        return FALSE;

    // GDK has buttons numbered from 1 to 5.

    if(e->button == 1/*left mouse*/) {
        if(movingBlocks) {
            // If we do this we lose the ability to catch a
            // double left mouse click; so we added the
            // blocksMoved flag:
            if(blocksMoved) {
                blocksMoved = false;
                PopUpBlock(movingBlocks);
            }
            movingBlocks = 0;
        }
        SetWidgetCursor(b->layout->layout, "default");
        return TRUE;
    }


    DSPEW();
    return FALSE; // FALSE => call more handlers.
}


//////////////////////////////////////////////////////////////////////////
//  We use a GtkGrid as the base widget container for a Block Module.
//
//
//  It's a 3 x 5 grid like so:
//
//  The middle column tends to be wider like so:
//
//    _____________________________________________
//   | __________________________________________  |
//   | |     |                             |     | |
//   | |     |        connections          |     | |
//   | |     |_____________________________|     | |
//   | |  c  |                             |  c  | |
//   | |  o  |         Block Name          |  o  | |
//   | |  n  |_____________________________|  n  | |
//   | |  n  |                             |  n  | |
//   | |  e  |          Filename           |  e  | |
//   | |  c  |_____________________________|  c  | |
//   | |  t  |                             |  t  | |
//   | |  i  |                             |  i  | |
//   | |  o  |         Block Icon          |  o  | |
//   | |  n  |                             |  n  | |
//   | |  s  |_____________________________|  s  | |
//   | |     |                             |     | |
//   | |     |        connections          |     | |
//   | |_____|_____________________________|_____| |
//   |_____________________________________________|
//
//
//  Note: the border is not part of the 3x5 grid in the GtkGrid, but
//  it adds to the size of the GtkGrid widget, BW.
//
//  The "Block Name", "Filename", and "Icon" are allowed to expand in
//  height when there are a large number of ports in the "connections"
//  (making them longer) keeping the smaller dimensions constant with
//  changing number of terminal connection ports.
//
//  There is a CSS border around the block GtkGrid that is of width CPP
//  macro, BW.  This border width, BW, is included in the width and height
//  of the block GtkGrid.  This border width is assumed to be the same on
//  all sides in the C code, so making it non-uniform will fuck up the C
//  code.
//
//  The layout of this block is pretty much hard coded.  This is not a
//  layout that can easily changed, without rewriting a lot of C code.
//  You can change CPP macros BW, CON_LEN, CON_WIDTH, PORT_ICON_WIDTH,
//  PORT_ICON_PAD, and PIN_LW; but there are constraints in the comments
//  in quickstreamGUI.h where they are defined.  We have tested changing
//  these values within the listed constraints.
//
//
//  there are 4 types of terminal connections:
//
//    stream inputs   = In
//    stream outputs  = Out
//    setters         = Set
//    getters         = Get
//


// Add a GUI representation of a quickstream block into the layout.

void CreateBlock(struct Layout *l,
        const char *filename, struct QsBlock *qsB,
        double x, double y, uint8_t orientation,
        int slFlag /*0 unselectAllButThis,
                   1 add this block to selected
                   2 no selection change */) {

    DASSERT(l);
    DASSERT(l->qsGraph);

    if(!qsB) {
        qsB = qsGraph_createBlock(l->qsGraph, 0, filename, 0, 0);
        if(!qsB) return; // failure
    }


    if(x < 0.0 || y < 0.0) {
        // Semi-random placement:
        x = l->scrollH + 20 + 250*drand48();
        y = l->scrollV + 20 + 270*drand48();
    }

    struct Block *b = calloc(1, sizeof(*b));
    ASSERT(b, "calloc(1,%zu) fail", sizeof(*b));

    // Set terminal values.
    b->terminals[In].pos  = Left;
    b->terminals[Out].pos = Right;
    b->terminals[Set].pos = Top;
    b->terminals[Get].pos = Bottom;
    //
    b->terminals[In].type = In;
    b->terminals[Out].type = Out;
    b->terminals[Set].type = Set;
    b->terminals[Get].type = Get;
    //
    b->terminals[In].block = b;
    b->terminals[Out].block = b;
    b->terminals[Set].block = b;
    b->terminals[Get].block = b;

    // Add this block to the layout blocks singly linked list, as the
    // first one:
    b->next = l->blocks;
    l->blocks = b;


    if(orientation)
        // Override the default just set above.
        //
        // This will override block->terminals[In,Out,Set,Get].pos
        SetOrientation(b, orientation);


    // Now make the GTK widget for the block on the layout.
    //
    // We need an event box because grid does not get events without it.
    //
    GtkWidget *e = gtk_event_box_new();
    gtk_widget_set_events(e,
            gtk_widget_get_events(e)|
            GDK_BUTTON_PRESS_MASK |
            GDK_BUTTON_RELEASE_MASK |
#if 0
            GDK_ENTER_NOTIFY_MASK | 
            GDK_LEAVE_NOTIFY_MASK |
#endif
            GDK_POINTER_MOTION_MASK);

#if 0
    g_signal_connect(e, "enter-notify-event", G_CALLBACK(Enter_cb), b);
    g_signal_connect(e, "leave-notify-event", G_CALLBACK(Leave_cb), b);
#endif

    g_signal_connect(e, "motion-notify-event", G_CALLBACK(Motion_cb), b);
    g_signal_connect(e, "button-press-event", G_CALLBACK(Press_cb), b);
    g_signal_connect(e, "button-release-event", G_CALLBACK(Release_cb), b);
    g_signal_connect(e, "destroy", G_CALLBACK(Destroy_cb), b);

    GtkWidget *grid = gtk_grid_new();

    gtk_widget_set_name(grid, "block_regular");
    gtk_container_add(GTK_CONTAINER(e), grid);
    gtk_layout_put(GTK_LAYOUT(l->layout), e, x, y);
    gtk_widget_set_size_request(grid, CON_LEN, CON_LEN);
    gtk_grid_set_row_homogeneous(GTK_GRID(grid), FALSE);


    gtk_widget_set_size_request(grid, CON_LEN, CON_LEN);

    b->qsBlock = qsB;
    b->grid = grid;
    b->layout = l;
    b->parent = e;
    b->x = x;
    b->y = y;

    ASSERT(g_object_get_data(G_OBJECT(e), "Block") == 0);
    g_object_set_data_full(G_OBJECT(e), "Block", (void *) b,
            (void (*)(void *)) DestroyBlock_cb);
    ASSERT(g_object_get_data(G_OBJECT(e), "Block") == b);


    { //////////////// Block Name ////////////////////////
        const char *name = qsBlock_getName(qsB);
        DSPEW("Creating GUI block named \"%s\"", name);
        GtkWidget *label = gtk_label_new(name);
        char *tooltipText = Splice2(
                "The name of this instance of this block is: ",
                name);
        gtk_widget_set_tooltip_text(label, tooltipText);
        free(tooltipText);
        gtk_widget_set_name(label, "name");
        gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
        gtk_widget_set_vexpand(label, TRUE);
        gtk_grid_attach(GTK_GRID(grid), label, 1, 1, 1, 1);
        // We need to keep a pointer to this label so we can change
        // the text in it is the block name changes.
        b->nameLabel = label;
    }
    { ///////////////// Filename ////////////////////////
        const char *basename = filename + strlen(filename);
        // TODO: UNIX specific code.
        while(*basename != '/' && basename != filename)
            --basename;
        if(*basename == '/')
            ++basename;
        char *tooltipText = Splice2(
                "The path that this block was loaded from is: ",
                filename);
        GtkWidget *label = gtk_label_new(basename);
        gtk_widget_set_tooltip_text(label, tooltipText);
        free(tooltipText);
        gtk_widget_set_name(label, "path");
        gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
        gtk_widget_set_vexpand(label, TRUE);
        gtk_grid_attach(GTK_GRID(grid), label, 1, 2, 1, 1);
    }
    { /////////////// Block Icon ////////////////////////
        GtkWidget *label = gtk_label_new("Icon");
        gtk_widget_set_tooltip_text(label, "Add an icon for this block");
        gtk_widget_set_name(label, "icon");
        gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
        // The icon will take any extra space in the vertical
        // and so all the other widgets in this row will have a fixed
        // height of 
        gtk_widget_set_vexpand(label, TRUE);
        gtk_grid_attach(GTK_GRID(grid), label, 1, 3, 1, 1);
    }

    // Though the terminal arrays exist, in the struct Block, we still
    // need to make drawing areas, setup widget callbacks, define all the
    // ports, and so on.
    CreateTerminal(b->terminals + In);
    CreateTerminal(b->terminals + Out);
    CreateTerminal(b->terminals + Set);
    CreateTerminal(b->terminals + Get);


    switch(slFlag) {
        case 0:
            UnselectAllBlocks(l, b);
            break;
        case 1:
            SelectBlock(b);
            break;
    }

    gtk_widget_show_all(grid);
    gtk_widget_show(e);
}
