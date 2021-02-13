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


enum ConnectorType {
    Input,
    Constant,
    Output,
    Getter,
    Setter
};


enum ConnectorGeo {
    ICOSG,
    OCISG,
    ISGOC,
    OSGIC,

    COSGI,
    CISGO,
    SGOCI,
    SGICO
};


struct Block;


struct Connector {
    enum ConnectorType type;
    enum ConnectorGeo geo; // orientation and flip/flop
    const char *name;
    struct Block *block;
};


struct Block {

    GtkWidget *ebox; // block container widget.
    struct Page *page; // tab page that has this block in it.
    struct QsBlock *block;
    struct Connector constants, getters, setters, input, output;
};


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
 *       |        constant        |        |
 *       |************************|        |
 *       |        path.so         |        |
 * input |                        | output |
 *       |         name           |        |
 *       |************************|        |
 *       |  setter   |   getter   |        |
 *******************************************


    Block with geo: OCISG (Ouput,Constant,Input,SetterGetter)

 *******************************************
 *        |        constant        |       |
 *        |************************|       |
 *        |        path.so         |       |
 * output |                        | input |
 *        |         name           |       |
 *        |************************|       |
 *        |  setter   |   getter   |       |
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

    width = gtk_widget_get_allocated_width (widget);
    height = gtk_widget_get_allocated_height (widget);

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

    switch(c->geo) {
        case ICOSG:
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
        case OCISG:
        case ISGOC:
        case OSGIC:
        case COSGI:
        case CISGO:
        case SGOCI:
        case SGICO:
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
    c->geo = ICOSG;
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

    if(movingBlockWidget)
        return FALSE; // FALSE = event to next widget

    return TRUE;
}


static gboolean
Block_buttonMotionCB(GtkWidget *ebox,
        GdkEventButton *e, struct Block *block) {

    if(movingBlockWidget)
        return FALSE; // FALSE = event to next widget

    return TRUE;
}


static gboolean
Block_buttonPressCB(GtkWidget *ebox,
        GdkEventButton *e, struct Block *block) {

    if(e->button == MOVE_BLOCK_BUTTON) {

        movingBlockWidget = ebox;
        return FALSE; // FALSE = event to next widget
    }

    return TRUE;
}


// The top GTK widget of a block is a gtk_event_box.
//
// Return widget if the block is successfully added, else return 0.
//
// layout - is the widget we add the block to.
//
// The name of the block can be changed later.
//
GtkWidget *AddBlock(struct Page *page,
        GtkLayout *layout, const char *blockFile,
        double x, double y) {

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
        gtk_widget_show(grid);
        gtk_widget_set_name(grid, "block");
        gtk_widget_set_visible(grid, TRUE);
        gtk_container_add(GTK_CONTAINER(ebox), grid);

        //                              x, y, w, h
        MakeBlockLabel(grid, blockFile, "path", 1, 1, 4, 1);
        MakeBlockLabel(grid, block->name, "name", 1, 3, 4, 1);
        //                                                     x, y, w, h
        MakeBlockConnector(grid, "constants", Constant, b, 1, 0, 4, 1);
        MakeBlockConnector(grid, "input", Input, b, 0, 0, 1, 5);
        MakeBlockConnector(grid, "output", Output, b, 5, 0, 1, 5);
        MakeBlockConnector(grid, "setters", Setter, b, 1, 4, 2, 1);
        MakeBlockConnector(grid, "getters", Getter, b, 3, 4, 2, 1);

        g_signal_connect(GTK_WIDGET(ebox), "button-press-event",
            G_CALLBACK(Block_buttonPressCB), b/*userData*/);
        g_signal_connect(GTK_WIDGET(ebox), "button-release-event",
            G_CALLBACK(Block_buttonReleaseCB), b/*userData*/);
        g_signal_connect(GTK_WIDGET(ebox), "motion-notify-event",
            G_CALLBACK(Block_buttonMotionCB), b/*userData*/);
      }

    gtk_layout_put(layout, ebox, x, y);

    return ebox;
}
