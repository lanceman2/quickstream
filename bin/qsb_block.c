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
#include "../lib/block.h"

#include "qsb.h"



static inline void GetConnectionColor(enum ConnectorType ctype,
        double *r, double *g, double *b, double *a) {

    switch(ctype) {
        case Input:
            *r = 1.0;
            *g = 0.0;
            *b = 0.0;
            *a = 0.1;
            break;
        case Output:
            *r = 1.0;
            *g = 0.0;
            *b = 0.0;
            *a = 0.1;
            break;
        case Getter:
            *r = 0.0;
            *g = 0.0;
            *b = 1.0;
            *a = 0.1;
            break;
        case Setter:
            *r = 0.0;
            *g = 0.0;
            *b = 1.0;
            *a = 0.1;
        case Constant:
            *r = 0.0;
            *g = 1.0;
            *b = 1.0;
            *a = 0.1;
    }
}


/*
     Block with geo: ICOSG (Input,Constant,Output,SetterGetter)

 *******************************************
 *       |          const         |        |
 *       |************************|        |
 *       |        path.so         |        |
 * input |                        | output |
 *       |         name           |        |
 *       |************************|        |
 *       |    set    |     get    |        |
 *******************************************


    Block with geo: OCISG (Ouput,Constant,Input,SetterGetter)

 *******************************************
 *        |         const          |       |
 *        |************************|       |
 *        |        path.so         |       |
 * output |                        | input |
 *        |         name           |       |
 *        |************************|       |
 *        |    set    |    get     |       |
 *******************************************


 and there are 6 more permutations that we allow.  We keep the input and
 output on opposing sides and the parameters constant opposing setter and
 getter; with setter always before getter.  That gives 8 total
 permutations that we allow.

 */


static gboolean DrawConnectImage_CB(GtkWidget *widget,
        cairo_t *cr,
        struct Connector *c) {

    guint width, height;
    GtkStyleContext *context;

    context = gtk_widget_get_style_context (widget);

    width = gtk_widget_get_allocated_width(widget);
    height = gtk_widget_get_allocated_height(widget);

    gtk_render_background(context, cr, 0, 0, width, height);

    DASSERT(c);

#if 1
    GdkRGBA color;
    gtk_style_context_get_color(context,
                    gtk_style_context_get_state(context),
                    &color);
    gdk_cairo_set_source_rgba(cr, &color);

    cairo_fill(cr);

    cairo_text_extents_t te;
    cairo_select_font_face (cr, "Georgia",
        CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size (cr, 14);

    switch(c->block->geo) {
        case ICOSG:
        case OCISG:
        case ISGOC:
        case OSGIC:
            switch(c->type) {
                case Setter:
                case Getter:
                case Constant:
                    cairo_text_extents(cr, c->name, &te);
                    cairo_move_to(cr, width/2 - te.width/2,
                            height/2 + te.height/2);
                    break;
                case Input:
                case Output:
                    cairo_text_extents(cr, c->name, &te);
                    cairo_rotate(cr, M_PI/2.0);
                    cairo_move_to(cr, height/2 - te.width/2,
                            - width/2 - (te.y_bearing + te.height) +
                            te.height/2);
                    break;
            }
            break;
        case COSGI:
        case CISGO:
        case SGOCI:
        case SGICO:
            switch(c->type) {
                case Input:
                case Output:
                    cairo_text_extents(cr, c->name, &te);
                    cairo_move_to(cr, width/2 - te.width/2,
                            height/2 + te.height/2);
                    break;
                case Setter:
                case Getter:
                case Constant:
                    cairo_text_extents(cr, c->name, &te);
                    cairo_rotate(cr, M_PI/2.0);
                    cairo_move_to(cr, height/2 - te.width/2,
                            - width/2 - (te.y_bearing + te.height) +
                            te.height/2);
                    break;
            }
            break;
    }

    cairo_show_text (cr, c->name);
#endif
    return FALSE;
}


static inline void
MakeBlockLabel(GtkWidget *grid,
        const char *text,
        const char *className,
        gint x, gint y, gint w, gint h) {

    GtkWidget *l = gtk_label_new(text);
    gtk_widget_set_name(l, className);
    gtk_widget_set_size_request(l, 120, MIN_BLOCK_LEN);
    gtk_widget_show(l);
    gtk_grid_attach(GTK_GRID(grid), l, x, y, w, h);
}


static void MakeBlockConnector(GtkWidget *grid,
        const char *className/*for CSS*/,
        enum ConnectorType ctype,
        struct Block *block,
        gint x, gint y, gint w, gint h) {

    GtkWidget *drawArea = gtk_drawing_area_new();

    gtk_widget_set_can_focus(drawArea, TRUE);
    gtk_widget_add_events(drawArea,
            GDK_STRUCTURE_MASK |
            GDK_SCROLL_MASK |
            GDK_POINTER_MOTION_MASK |
            GDK_BUTTON_RELEASE_MASK |
            GDK_BUTTON_PRESS_MASK|
            GDK_STRUCTURE_MASK);

    gtk_widget_show(drawArea);
    gtk_widget_set_size_request(drawArea, MIN_BLOCK_LEN, MIN_BLOCK_LEN);
    gtk_widget_set_name(drawArea, className);
    gtk_grid_attach(GTK_GRID(grid), drawArea, x, y, w, h);

    struct Connector *c;
    switch(ctype) {
        case Constant:
            c = &block->constants;
            break;
        case Getter:
            c = &block->getters;
            break;
        case Setter:
            c = &block->setters;
            break;
        case Input:
            c = &block->input;
            break;
        case Output:
            c = &block->output;
            break;
    }

    c->type = ctype;
    c->widget = drawArea;
    c->block = block;
    c->name = strdup(className);
    // TODO: free c->name.
    ASSERT(c->name, "strdup() failed");
    // TODO: free c

    g_signal_connect(G_OBJECT(drawArea), "draw",
            G_CALLBACK(DrawConnectImage_CB), c/*userData*/);
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

    if(movingBlock)
        return FALSE; // FALSE = event to next widget

    return TRUE;
}


// Popup menu for block button click
static GtkMenu *popupMenu = 0;
// This is the block that the user clicked on.
static struct Block *popupBlock = 0;


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

    switch(e->button) {

        case MOVE_BLOCK_BUTTON:
            movingBlock = block;
            return FALSE; // FALSE = event to next widget

        case BLOCK_POPUP_BUTTON:
            popupBlock = block;
            UnselectAllBlocks(block->page);
            SelectBlock(block);
            gtk_menu_popup_at_pointer(popupMenu, (GdkEvent *) e);
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


// Moves all the connectors to positions given by "geo".
//
static void MoveConnectors(struct Block *b, enum ConnectorGeo geo) {

    DASSERT(b);

    // Increase the reference count of widget object, so that when we
    // remove the widget from the grid it will not destroy it.
    g_object_ref(b->constants.widget);
    g_object_ref(b->getters.widget);
    g_object_ref(b->setters.widget);
    g_object_ref(b->input.widget);
    g_object_ref(b->output.widget);

    gtk_container_remove(GTK_CONTAINER(b->grid), b->constants.widget);
    gtk_container_remove(GTK_CONTAINER(b->grid), b->getters.widget);
    gtk_container_remove(GTK_CONTAINER(b->grid), b->setters.widget);
    gtk_container_remove(GTK_CONTAINER(b->grid), b->input.widget);
    gtk_container_remove(GTK_CONTAINER(b->grid), b->output.widget);

    // First place constants, setters and getters.
    switch(geo) {
        case OCISG:
        case ICOSG:
            gtk_grid_attach(GTK_GRID(b->grid),
                    b->constants.widget, 1, 0, 4, 1);
            gtk_grid_attach(GTK_GRID(b->grid),
                    b->getters.widget, 3, 4, 2, 1);
            gtk_grid_attach(GTK_GRID(b->grid),
                    b->setters.widget, 1, 4, 2, 1);
            break;
        case ISGOC:
        case OSGIC:
            gtk_grid_attach(GTK_GRID(b->grid),
                    b->constants.widget, 1, 4, 4, 1);
            gtk_grid_attach(GTK_GRID(b->grid),
                    b->getters.widget, 3, 0, 2, 1);
            gtk_grid_attach(GTK_GRID(b->grid),
                    b->setters.widget, 1, 0, 2, 1);
            break;
        case COSGI:
        case CISGO:
            gtk_grid_attach(GTK_GRID(b->grid),
                    b->constants.widget, 0, 0, 1, 5);
            gtk_grid_attach(GTK_GRID(b->grid),
                    b->getters.widget, 5, 2, 1, 3);
            gtk_grid_attach(GTK_GRID(b->grid),
                    b->setters.widget, 5, 0, 1, 2);
            break;
        case SGOCI:
        case SGICO:
            gtk_grid_attach(GTK_GRID(b->grid),
                    b->constants.widget, 5, 0, 1, 5);
            gtk_grid_attach(GTK_GRID(b->grid),
                    b->getters.widget, 0, 2, 1, 3);
            gtk_grid_attach(GTK_GRID(b->grid),
                    b->setters.widget, 0, 0, 1, 2);
            break;
    }

    // Now place input and output.
    switch(geo) {
        case ICOSG:
        case ISGOC:
            gtk_grid_attach(GTK_GRID(b->grid),
                    b->input.widget, 0, 0, 1, 5);
            gtk_grid_attach(GTK_GRID(b->grid),
                    b->output.widget, 5, 0, 1, 5);
            break;
        case OCISG:
        case OSGIC:
            gtk_grid_attach(GTK_GRID(b->grid),
                    b->input.widget, 5, 0, 1, 5);
            gtk_grid_attach(GTK_GRID(b->grid),
                    b->output.widget, 0, 0, 1, 5);
            break;
        case COSGI:
        case SGOCI:
            gtk_grid_attach(GTK_GRID(b->grid),
                    b->input.widget, 1, 4, 4, 1);
            gtk_grid_attach(GTK_GRID(b->grid),
                    b->output.widget, 1, 0, 4, 1);
            break;
        case CISGO:
        case SGICO:
            gtk_grid_attach(GTK_GRID(b->grid),
                    b->input.widget, 1, 0, 4, 1);
            gtk_grid_attach(GTK_GRID(b->grid),
                    b->output.widget, 1, 4, 4, 1);
            break;
    }

    gint w = gtk_widget_get_allocated_width(b->grid);
    gint h = gtk_widget_get_allocated_height(b->grid);
    gtk_widget_queue_draw_area(b->grid, 0, 0, w, h);
    b->geo = geo;
}


static void RotateCCWCB(GtkWidget *widget,
        GdkEvent *e, gpointer data) {
    DASSERT(popupBlock);

    switch(popupBlock->geo) {
        case ICOSG:
            MoveConnectors(popupBlock, COSGI);
            break;
        case OCISG:
            MoveConnectors(popupBlock, CISGO);
            break;
        case ISGOC:
            MoveConnectors(popupBlock, SGOCI);
            break;
        case OSGIC:
            MoveConnectors(popupBlock, SGICO);
            break;
        case COSGI:
            MoveConnectors(popupBlock, OSGIC);
            break;
        case CISGO:
            MoveConnectors(popupBlock, ISGOC);
            break;
        case SGOCI:
            MoveConnectors(popupBlock, OCISG);
            break;
        case SGICO:
            MoveConnectors(popupBlock, ICOSG);
            break;
    }
}


static void RotateCWCB(GtkWidget *widget,
        GdkEvent *e, gpointer data) {
    DASSERT(popupBlock);

    switch(popupBlock->geo) {
        case ICOSG:
            MoveConnectors(popupBlock, SGICO);
            break;
        case OCISG:
            MoveConnectors(popupBlock, SGOCI);
            break;
        case ISGOC:
            MoveConnectors(popupBlock, CISGO);
            break;
        case OSGIC:
            MoveConnectors(popupBlock, COSGI);
            break;
        case COSGI:
            MoveConnectors(popupBlock, ICOSG);
            break;
        case CISGO:
            MoveConnectors(popupBlock, OCISG);
            break;
        case SGOCI:
            MoveConnectors(popupBlock, ISGOC);
            break;
        case SGICO:
            MoveConnectors(popupBlock, OSGIC);
            break;
    }
}


static void FlipCB(GtkWidget *widget,
        GdkEvent *e, gpointer data) {
    DASSERT(popupBlock);

    switch(popupBlock->geo) {
        case ICOSG:
            MoveConnectors(popupBlock, ISGOC);
            break;
        case OCISG:
            MoveConnectors(popupBlock, OSGIC);
            break;
        case ISGOC:
            MoveConnectors(popupBlock, ICOSG);
            break;
        case OSGIC:
            MoveConnectors(popupBlock, OCISG);
            break;
        case COSGI:
            MoveConnectors(popupBlock, CISGO);
            break;
        case CISGO:
            MoveConnectors(popupBlock, COSGI);
            break;
        case SGOCI:
            MoveConnectors(popupBlock, SGICO);
            break;
        case SGICO:
            MoveConnectors(popupBlock, SGOCI);
            break;
    }
}


static void FlopCB(GtkWidget *widget,
        GdkEvent *e, gpointer data) {
    DASSERT(popupBlock);

    switch(popupBlock->geo) {
        case ICOSG:
            MoveConnectors(popupBlock, OCISG);
            break;
        case OCISG:
            MoveConnectors(popupBlock, ICOSG);
            break;
        case ISGOC:
            MoveConnectors(popupBlock, OSGIC);
            break;
        case OSGIC:
            MoveConnectors(popupBlock, ISGOC);
            break;
        case COSGI:
            MoveConnectors(popupBlock, SGOCI);
            break;
        case CISGO:
            MoveConnectors(popupBlock, SGICO);
            break;
        case SGOCI:
            MoveConnectors(popupBlock, COSGI);
            break;
        case SGICO:
            MoveConnectors(popupBlock, CISGO);
            break;
    }
}



static void RemovePopupBlockCB(GtkWidget *widget,
        GdkEvent *e, gpointer data) {
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


    if(popupMenu == 0) {

        GtkBuilder *popupBuilder = gtk_builder_new_from_resource(
                "/quickstreamBuilder/qsb_popup_res.ui");
        popupMenu = GTK_MENU(gtk_builder_get_object(popupBuilder,
                    "popupMenu"));
        Connect(popupBuilder,"remove", "activate", RemovePopupBlockCB, 0);
        Connect(popupBuilder,"rotateCW", "activate", RotateCWCB, 0);
        Connect(popupBuilder,"rotateCCW", "activate", RotateCCWCB, 0);
        Connect(popupBuilder,"flip", "activate", FlipCB, 0);
        Connect(popupBuilder,"flop", "activate", FlopCB, 0);
       }


    DASSERT(blockFile);

    if(!graph) graph = qsGraphCreate();
    ASSERT(graph);

    struct QsBlock *block = qsGraphBlockLoad(graph, blockFile, 0);

    if(!block)
        // Failed to load block.
        return 0;

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
            GDK_BUTTON_PRESS_MASK);

    {
        /* The Grid of a block is 6x5

            0,0  1,0  2,0  3,0  4,0  5,0

            0,1  1,1  2,1  3,1  4,1  5,1

            0,2  1,2  2,2  3,2  4,2  5,2

            0,3  1,3  2,3  3,3  4,3  5,3

            0,4  1,4  2,4  3,4  4,4  5,4

        */

        GtkWidget *grid = gtk_grid_new();
        b->grid = grid;
        gtk_widget_show(grid);
        gtk_widget_set_name(grid, "block");
        gtk_widget_set_visible(grid, TRUE);
        gtk_container_add(GTK_CONTAINER(ebox), grid);

        //                              x, y, w, h
        MakeBlockLabel(grid, blockFile, "path", 1, 1, 4, 1);
        MakeBlockLabel(grid, block->name, "name", 1, 3, 4, 1);
        //                                                     x, y, w, h
        MakeBlockConnector(grid, "const", Constant, b, 1, 0, 4, 1);
        MakeBlockConnector(grid, "get", Getter, b, 3, 4, 2, 1);
        MakeBlockConnector(grid, "set", Setter, b, 1, 4, 2, 1);
        MakeBlockConnector(grid, "input", Input, b, 0, 0, 1, 5);
        MakeBlockConnector(grid, "output", Output, b, 5, 0, 1, 5);

        g_signal_connect(GTK_WIDGET(ebox), "button-press-event",
            G_CALLBACK(Block_buttonPressCB), b/*userData*/);
        g_signal_connect(GTK_WIDGET(ebox), "button-release-event",
            G_CALLBACK(Block_buttonReleaseCB), b/*userData*/);
        g_signal_connect(GTK_WIDGET(ebox), "motion-notify-event",
            G_CALLBACK(Block_buttonMotionCB), b/*userData*/);
      }
    gtk_layout_put(layout, ebox, b->x, b->y);

    return b;
}
