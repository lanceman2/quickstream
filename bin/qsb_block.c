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
     Block with 0 degrees rotation:

 ***************************************
 *                set                  *
 *                                     *
 * input                        output *
 *                                     *
 *                get                  *
 ***************************************
 */

// TODO: Save and Cleanup cairo_surface_t *s.
//
static void DrawConnectImageSurface(cairo_surface_t *s,
        enum ConnectorType ctype,
        uint32_t rot) {

    DASSERT(s);

    cairo_t *cr = cairo_create(s);
    //                          r, g, b, a
    cairo_set_source_rgba(cr, 0.9, 0, 0, 0.1);
    cairo_paint(cr);

    double r, g, b, a;
    GetConnectionColor(ctype, &r, &g, &b, &a);
    // override the alpha.
    //a = 0.4;

    cairo_set_source_rgba(cr, r, g, b, a);
    cairo_set_line_width(cr, 6);

    cairo_translate(cr, 16, 16);

    if(ctype == Input || ctype == Output)
        cairo_rotate(cr, (rot + 2) * M_PI/2.0);
    else // ctype ==  Constant || ctype == Getter || ctype == Setter
        cairo_rotate(cr, (rot + 3) * M_PI/2.0);

    cairo_rotate(cr, M_PI);

    cairo_translate(cr, -16, -16);

    cairo_move_to(cr, 0, 0);
    cairo_line_to(cr, 31, 15);
    cairo_line_to(cr, 0, 31);

    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_stroke(cr);
    cairo_destroy(cr);
}


#define CONNECT_IMAGE_LEN   (32)


// TODO: Save and Cleanup cairo_surface_t *s.
//
static inline cairo_surface_t *
CreateConnectImageSurface(enum ConnectorType ctype, uint32_t rot) {

    cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
            CONNECT_IMAGE_LEN, CONNECT_IMAGE_LEN);
    DASSERT(s);
    DrawConnectImageSurface(s, ctype, rot);

    return s;
}


static inline void
MakeBlockLabel(GtkWidget *grid,
        const char *text,
        const char *className,
        gint x, gint y, gint w, gint h) {

    GtkWidget *l = gtk_label_new(text);
    gtk_widget_set_name(l, className);
    gtk_widget_show(l);
    gtk_grid_attach(GTK_GRID(grid), l, x, y, w, h);
}


static void MakeBlockConnector(GtkWidget *grid,
        const char *className/*for CSS shit*/,
        enum ConnectorType ctype,
        gint x, gint y, gint w, gint h) {

    GtkWidget *ebox = gtk_event_box_new();

    gtk_widget_set_can_focus(ebox, TRUE);
    gtk_widget_set_events(ebox,
            GDK_STRUCTURE_MASK |
            GDK_SCROLL_MASK |
            GDK_POINTER_MOTION_MASK |
            GDK_BUTTON_RELEASE_MASK |
            GDK_BUTTON_PRESS_MASK|
            GDK_STRUCTURE_MASK);

    gtk_widget_show(ebox);
    gtk_widget_set_name(ebox, className);
    gtk_grid_attach(GTK_GRID(grid), ebox, x, y, w, h);

    cairo_surface_t *surface = CreateConnectImageSurface(ctype, 0);

    GtkWidget *img = gtk_image_new_from_surface(surface);
    // Decreases the reference count on surface by one.  The GTK image will
    // add its' own reference.
    cairo_surface_destroy(surface);

    gtk_container_add(GTK_CONTAINER(ebox), img);
    //gtk_widget_set_name(img, className); // ebox takes care of this.
    gtk_widget_show(img);
}


// The top GTK widget of a block is a gtk_event_box.
//
// Return true if the block is successfully added, else return false.
//
// layout - is the widget we add the block to.
//
// The name of the block can be changed later.
//
bool AddBlock(GtkLayout *layout, const char *blockFile,
    double x, double y) {

    DASSERT(blockFile);

    if(!graph) graph = qsGraphCreate();
    ASSERT(graph);

    struct QsBlock *block = qsGraphBlockLoad(graph, blockFile, 0);



    // As of GTK3 version 3.24.20; gtk widget name is more like a CSS
    // class name.  Name is not a unique ID.  It's more like a CSS class.
    // That's not likely to change, given all the shit that would break.

    // Widget sandwich like so (bottom to top user facing top):
    //
    //   event_box : grid :
    //                       event_box : img  (connections)
    //                       label            (inner labels)
    //
    //
    // Events that fall back to the root event_box are for the block as a
    // whole.  Events that are eaten by the img are connections to stream
    // Input, Output, and parameter Setter, Getter, and Constant
    // connections.

    GtkWidget *ebox = gtk_event_box_new();
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
        //                                             x, y, w, h
        MakeBlockConnector(grid, "constant", Constant, 1, 0, 4, 1);
        MakeBlockConnector(grid, "input", Input, 0, 0, 1, 5);
        MakeBlockConnector(grid, "output", Output, 5, 0, 1, 5);
        MakeBlockConnector(grid, "setter", Setter, 1, 4, 2, 1);
        MakeBlockConnector(grid, "getter", Getter, 3, 4, 2, 1);
    }

    gtk_layout_put(layout, ebox, x, y);

    return true;
}
