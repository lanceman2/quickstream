#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdbool.h>
#include <math.h>

#include <X11/Xlib.h>

#include <gtk/gtk.h>
#include <gdk/gdkevents.h>
#include <gdk/gdkx.h>

#include "../include/quickstream/app.h"
#include "../include/quickstream/block.h"
#include "../include/quickstream/builder.h"

#include "../lib/debug.h"
// TODO: Big todo.  This code reaches into the guts of libquickstream;
// we could instead make a more complete quickstream builder API in
// libquickstream.  Maybe later, maybe not.  Whatever.  It's just not
// so clear what a quickstream builder API should look like.
#include "../lib/block.h"
#include "../lib/parameter.h"

#include "qsb.h"



// Popup menu for block button click
static GtkMenu *blockPopupMenu = 0;
// This is the block that the user clicked on.
struct Block *popupBlock = 0;


// The whole point of this program is to make blocks and connect the
// connectors (in these blocks) that are part of the blocks.  This is the
// current "FROM" connector that we started making a connection at.   This
// is set to zero after we have the "TO" connector or the user gives up
// trying to finish this connection that the user initiated.  The user
// only can make one connection at a time, with the mouse, hence this is
// just one variable, "fromConnector".
struct Connector *fromConnector = 0;




static inline GtkWidget *
MakeBlockLabel(GtkWidget *grid,
        const char *text,
        const char *className,
        gint x, gint y, gint w, gint h) {

    GtkWidget *l = gtk_label_new(text);
    gtk_widget_set_name(l, className);
    gtk_widget_show(l);
    gtk_grid_attach(GTK_GRID(grid), l, x, y, w, h);

    return l;
}


static gboolean
Block_buttonReleaseCB(GtkWidget *ebox,
        GdkEventButton *e, struct Block *block) {

    if(movingBlock)
        return FALSE; // FALSE = event to next widget

    return TRUE;
}


static gboolean
Block_buttonMotionCB(GtkWidget *ebox,
        GdkEventButton *e, struct Block *block) {

    // The moving of the block is done in the layout event catchers in
    // qsb.c.

    if(movingBlock)
        return FALSE; // FALSE = event to next widget

    return FALSE;
}


static gboolean UnselectCB(struct Block *key, struct Block *val,
                  gpointer data) {
    UnselectBlock(key);
    return FALSE; // Keep traversing.
}


void UnselectAllBlocks(struct Page *page) {

    DASSERT(page);
    g_tree_foreach(page->selectedBlocks, (GTraverseFunc) UnselectCB, 0);
}


static gboolean
Block_buttonPressCB(GtkWidget *ebox,
        GdkEventButton *e, struct Block *block) {


    if(fromConnector)
        // If we clicked on a block and got to here, then the making of a
        // connection was aborted by the user, as we define it.
        StopDragingConnection(block->page);


    switch(e->button) {

        case MOVE_BLOCK_BUTTON:
            movingBlock = block;
            return FALSE; // FALSE = event to next widget

        case BLOCK_POPUP_BUTTON:
            popupBlock = block;
            UnselectAllBlocks(block->page);
            SelectBlock(block);
            gtk_menu_popup_at_pointer(blockPopupMenu, (GdkEvent *) e);
            return TRUE; // TRUE Event stops here.
    }

    return TRUE; // TRUE Event stops here.
}


void SelectBlock(struct Block *b) {

    if(b->isSelected) return;

    gtk_widget_set_name(b->grid, "selectedBlock");
    g_tree_insert(b->page->selectedBlocks, b, b);
    b->isSelected = true;
}


void UnselectBlock(struct Block *b) {

    if(!b->isSelected) return;

    gtk_widget_set_name(b->grid, "block");
    g_tree_remove(b->page->selectedBlocks, b);
    b->isSelected = false;
}


static void DestroyBlock(struct Block *b) {

    DASSERT(b);
    DASSERT(b->page);

    UnselectBlock(b);

    // Remove block from the page blocks list
    struct Block *prev = 0;
    struct Block *bl = b->page->blocks;
    DASSERT(bl);
    while(bl && bl != b) {
        prev = bl;
        bl = bl->next;
    }
    DASSERT(bl == b);
    if(prev)
        prev->next = b->next;
    else
        b->page->blocks = b->next;

    qsBlockUnload(b->block);

    gtk_widget_destroy(b->ebox);

#ifdef DEBUG
    memset(b, 0, sizeof(*b));
#endif
    free(b);
}


static void RemovePopupBlockCB(GtkWidget *widget, gpointer data) {
    DASSERT(popupBlock);
    DestroyBlock(popupBlock);
    popupBlock = 0;
}


// The top GTK widget of a block is a gtk_event_box.
//
// Return widget if the block is successfully added, else return 0.
//
// layout - is the widget we add the block to.
//
// The name of the block can be changed later.
//
struct Block *AddBlock(struct Page *page,
        GtkLayout *layout, const char *blockFile,
        double x, double y) {

    DASSERT(blockFile);

    if(!graph) graph = qsGraphCreate();
    ASSERT(graph);

    struct QsBlock *block = qsGraphBlockLoad(graph, blockFile, 0);

    if(!block)
        // Failed to load block.
        return 0;

    if(blockPopupMenu == 0) {

        GtkBuilder *popupBuilder = gtk_builder_new_from_resource(
                "/quickstreamBuilder/qsb_popup_res.ui");
        blockPopupMenu = GTK_MENU(gtk_builder_get_object(popupBuilder,
                    "popupMenu"));
        Connect(popupBuilder,"remove", "activate", RemovePopupBlockCB, 0);
        Connect(popupBuilder,"rotateCW", "activate", RotateCWCB, 0);
        Connect(popupBuilder,"rotateCCW", "activate", RotateCCWCB, 0);
        Connect(popupBuilder,"flip", "activate", FlipCB, 0);
        Connect(popupBuilder,"flop", "activate", FlopCB, 0);
    }


    struct Block *b = calloc(1, sizeof(*b));
    // TODO: free(b);
    ASSERT(b, "calloc(1,%zu) failed", sizeof(*b));
    b->block = block;
    b->page = page;
    b->x = x;
    b->y = y;
    // The geometric arrangement of the connectors.
    b->geo = ICOSG;

    // Add to the end of the singly linked list of blocks in the page.
    if(page->blocks) {
        struct Block *last = page->blocks;
        while(last->next) last = last->next;
        last->next = b;
    } else {
        page->blocks = b;
    }


    // As of GTK3 version 3.24.20; gtk widget name is more like a CSS
    // class name.  Name is not a unique ID.  It's more like a CSS class.
    // That's not likely to change, given all the shit that would break.

    // Widget sandwich like so (bottom to top user facing top):
    //
    //   event_box : grid :
    //                       draw_area  (connections)
    //                       label      (inner labels)
    //
    //
    // Events that fall back to the root event_box are for the block as a
    // whole.  Events that are eaten by the img are connections to stream
    // Input, Output, and parameter Setter, Getter, and Constant
    // connections.

    GtkWidget *ebox = gtk_event_box_new();
    b->ebox = ebox;
    gtk_widget_show(ebox);
    gtk_widget_set_can_focus(ebox, TRUE);
    gtk_widget_set_events(ebox,
            GDK_STRUCTURE_MASK |
            GDK_SCROLL_MASK |
            GDK_POINTER_MOTION_MASK |
            GDK_BUTTON_RELEASE_MASK |
            GDK_BUTTON_PRESS_MASK|
            GDK_ENTER_NOTIFY_MASK|
            GDK_LEAVE_NOTIFY_MASK);

    {
        /* The Grid of a block is 6x5

            0,0  1,0  2,0  3,0  4,0  5,0  6,0

            0,1  1,1  2,1  3,1  4,1  5,1  6,1

            0,2  1,2  2,2  3,2  4,2  5,2  6,2

            0,3  1,3  2,3  3,3  4,3  5,3  6,3

            0,4  1,4  2,4  3,4  4,4  5,4  6,4

            0,5  1,5  2,5  3,5  4,5  5,5  6,5


        */

        GtkWidget *grid = gtk_grid_new();
        b->grid = grid;
        gtk_widget_show(grid);
        gtk_grid_set_row_spacing(GTK_GRID(grid), 0);
        gtk_grid_set_column_spacing(GTK_GRID(grid), 0);
        gtk_widget_set_name(grid, "block");
        gtk_widget_set_visible(grid, TRUE);
        gtk_container_add(GTK_CONTAINER(ebox), grid);



        b->pathLabel = MakeBlockLabel(grid, blockFile, "path",
        //      x, y, w, h
                1, 1, 4, 1);
        b->nameLabel = MakeBlockLabel(grid, block->name, "name",
                1, 3, 4, 1);

        MakeBlockConnector(grid, "const", Constant, b);
        MakeBlockConnector(grid, "get", Getter, b);
        MakeBlockConnector(grid, "set", Setter, b);
        MakeBlockConnector(grid, "input", Input, b);
        MakeBlockConnector(grid, "output", Output, b);

        g_signal_connect(GTK_WIDGET(ebox), "button-press-event",
            G_CALLBACK(Block_buttonPressCB), b/*userData*/);
        g_signal_connect(GTK_WIDGET(ebox), "button-release-event",
            G_CALLBACK(Block_buttonReleaseCB), b/*userData*/);
        g_signal_connect(GTK_WIDGET(ebox), "motion-notify-event",
            G_CALLBACK(Block_buttonMotionCB), b/*userData*/);

        // Attach the connectors to the grid with, geo, orientations
        // given by the default that we define here as:
        OrientConnectors(b, ICOSG, false);
      }

    gtk_layout_put(layout, ebox, b->x, b->y);

    return b;
}
