#include <gtk/gtk.h>

#include "../include/quickstream.h"

#include "../lib/debug.h"

#include "quickstreamGUI.h"


uint32_t numLayouts = 0;

// Layout background color:
static const double
    bgR = 1.0,
    bgG = 1.0,
    bgB = 1.0,
    bgA = 1.0;


// Used to draw the selection box.
struct LayoutSelection layoutSelection = { 0 };


static inline void StopConnectionDrag(GtkWidget *layout) {

    if(!fromPort) return;

    fromPort = 0;
    gtk_widget_queue_draw(layout);
}


void HandleConnectingDragEvent(GdkEventMotion *e, struct Layout *l,
        double x, double y, enum Pos toPos) {

    DASSERT(l);
    DASSERT(l->layout);

//ERROR("          e->state=0%o", e->state);
    if(!(e->state & GDK_BUTTON1_MASK) ||
            (e->state & (
                         GDK_BUTTON2_MASK |
                         GDK_BUTTON3_MASK |
                         GDK_BUTTON4_MASK |
                         GDK_BUTTON5_MASK))) {
        // The user is doing some shit that we do not do.
        // We are done trying to make a connection.
        StopConnectionDrag(l->layout);
        return;
    }
    DrawLayoutNewSurface(l, x, y, toPos);
    gtk_widget_queue_draw(l->layout);
}


static void DestroyLayout_cb(struct Layout *l) {

    DASSERT(l);
    DASSERT(l->qsGraph);
    qsGraph_destroy(l->qsGraph);

#ifdef DEBUG
    memset(l, 0, sizeof(*l));
#endif
    free(l);
    DSPEW();

    if(--numLayouts == 0)
        gtk_main_quit();
}


static
void
Scrolled_cb(GtkAdjustment *a, int *scrollValue) {

    DASSERT(a);
    DASSERT(scrollValue);

    *scrollValue = gtk_adjustment_get_value(a);

    //DSPEW("*scrollValue=*%p=%d\n", scrollValue, *scrollValue);
}


// Draw the background and all the connection lines that are in the
// Graph and we can see them in the current view of the layout.
static inline void DrawLayoutSurface(struct Layout *l) {

    cairo_t *cr = cairo_create(l->surface);
    DASSERT(cr);
    cairo_identity_matrix(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_set_source_rgba(cr,bgR, bgG, bgB, bgA);
    cairo_paint(cr);

    for(struct Connection *c = l->firstConnection; c; c = c->next)
        DrawConnectionLineToSurface(cr, c->port1, c->port2);

    cairo_destroy(cr);
}


static inline void
RemakeSurfaces(struct Layout *l) {

    DASSERT(l);
    DASSERT(l->layout);
    DASSERT(l->w);
    DASSERT(l->h);

    if(l->surface) {
        DASSERT(l->newSurface);
        cairo_surface_destroy(l->surface);
        cairo_surface_destroy(l->newSurface);
    }

    // This needs to be called from the "draw" callback because the
    // gtk_widget_get_window() fails when called from in the resize
    // events.
    l->surface = gdk_window_create_similar_surface(
            gtk_widget_get_window(l->layout),
            CAIRO_CONTENT_COLOR_ALPHA,
            l->w, l->h);

    l->newSurface = gdk_window_create_similar_surface(
            gtk_widget_get_window(l->layout),
            CAIRO_CONTENT_COLOR_ALPHA,
            l->w, l->h);

    DrawLayoutSurface(l);
}



static gboolean
Draw_cb(GtkWidget *layout,
        cairo_t *cr, struct Layout *l) {

    DASSERT(l);
    DASSERT(l->layout == layout);
    GtkAllocation a;
    gtk_widget_get_allocation(layout, &a);

    if(l->w < a.width || l->h < a.height) {
        // Make it larger if we need to.
        if(l->w < a.width)
            l->w = a.width;
        if(l->h < a.height)
            l->h = a.height;

        gtk_layout_set_size(GTK_LAYOUT(layout), l->w, l->h);

        RemakeSurfaces(l);
    } else if(!l->surface)
        RemakeSurfaces(l);


    //DSPEW("width,height=%d,%d", a.width, a.height);

    DASSERT(l->w >= a.width && l->h >= a.height);

    if(l->surfaceNeedRedraw)
        DrawLayoutSurface(l);


    // Note the coordinates are relative to the viewable part of the
    // scroll window.  Where-as the layout as lots of hidden area in the
    // scroll window.  Hence the layout has 2 coordinate systems with the
    // smaller one displaced by x=scrollH, y=scrollV.
    //
    cairo_set_source_surface(cr, l->surface, -l->scrollH, -l->scrollV);
    cairo_paint(cr);


    if(fromPort || layoutSelection.active) {
        // Add the new, not finished connection line, or selection box,
        // on top.
        cairo_set_source_surface(cr, l->newSurface,
                -l->scrollH, -l->scrollV);
        cairo_paint(cr);
    }

    // TODO: Tests show we get a lot of draw events for no good reason.


    // This needs to return FALSE so that the layout can draw widgets on
    // top of this background.  This background has the block terminal
    // connection lines on it.
    //
    return FALSE; // FALSE to propagate the event further
}


static gboolean
Press_cb(GtkWidget *layout, GdkEventButton *e, struct Layout *l) {

    DASSERT(layout);
    DASSERT(e);
    DASSERT(l);

    if(e->type != GDK_BUTTON_PRESS)
        return FALSE;

    if(e->button == 2/*middle mouse*/) {

        DASSERT(l->window);
        DASSERT(l == l->window->layout);
        DASSERT(l->window->treeView);

        char *path = GetBlockPath(l->window->treeView, 0);

        if(!path)
            // A block file is not selected.  It could be a directory is
            // selected.
            return FALSE;

        // Try to open a block with path.
        //
        // AddBlock will own the path memory.
        CreateBlock(l, path, 0,
                e->x - PORT_ICON_WIDTH - BW - 6.0,
                e->y - PORT_ICON_WIDTH - BW - 6.0,
                0/*orientation: 0==default*/,
                0/*0 unselectAllButThis,
                   1 add this block to selected
                   2 no selection change */);

        // We will have a mouse release event grab, but we'll not use it
        // in Release_cb().

        return TRUE;
    }

    if(e->button == 3/*right mouse*/) {

        ShowGraphTabPopupMenu(l);
        return TRUE; // TRUE to not propagate the event further
    }


    if(e->button != 1/*not left*/)
        return FALSE;

    // We will start a selection box.
    UnselectAllBlocks(l, 0);
    layoutSelection.active = true;

    // Start a new null selection
    layoutSelection.x = e->x;
    layoutSelection.y = e->y;
    layoutSelection.width = 0.0;
    layoutSelection.height = 0.0;

    // We have a pointer/mouse grab now.
    return TRUE; // TRUE to not propagate the event further
}


static gboolean
Release_cb(GtkWidget *layout, GdkEventButton *e, struct Layout *l) {

    if(e->type != GDK_BUTTON_RELEASE)
        return FALSE;

    if(e->button != 1/*not left*/)
        return FALSE;

    // We have a left button release.

    if(movingBlocks) {
        PopUpBlock(movingBlocks);
        SetWidgetCursor(layout, "default");
        movingBlocks = 0;
        return TRUE; // TRUE == eat event
    }

    if(!layoutSelection.active)
        return FALSE; // FALSE to propagate the event further

    // We finished a selection box drag.
    layoutSelection.active = false;
    DrawLayoutNewSurface(l, -3.0, -3.0, NumPos);
    gtk_widget_queue_draw(layout);

    // The blocks should be selected from the Motion_cb().

    return TRUE; // FALSE to propagate the event further
}


static gboolean
Motion_cb(GtkWidget *layout, GdkEventMotion *e, struct Layout *l) {

    DASSERT(layout);
    DASSERT(l);
    DASSERT(l->layout == layout);

    if(e->type != GDK_MOTION_NOTIFY)
        return FALSE;


    if(movingBlocks) {
        DASSERT(!fromPort);
        // Move all selected blocks.
        for(struct Block *b = movingBlocks; b; b = b->nextSelected) {
            // We use absolute positions so that we do not round off.
            // I have seen the round off in that the pointer slips off
            // the block as you drag it.
            b->x = e->x_root - x_root + b->x0;
            b->y = e->y_root - y_root + b->y0;

            gtk_layout_move(GTK_LAYOUT(layout),
                    b->parent, b->x, b->y);
        }
        return TRUE;
    }

    if(fromPort) {
        DASSERT(!movingBlocks);
        HandleConnectingDragEvent(e, l, e->x, e->y, NumPos);
        return TRUE;
    }

    if(!layoutSelection.active)
        return FALSE;


    // Note the cairo_rectangle() can have negative width and/or height.
    //
    // https://www.cairographics.org/manual/cairo-Paths.html#cairo-rectangle
    //
    layoutSelection.width = e->x - layoutSelection.x;
    layoutSelection.height = e->y - layoutSelection.y;

    // Let x1,y1, x2,y2 be the coordinates of the selection rectangle.
    gint x1 = layoutSelection.x;
    gint x2 = x1;
    if(layoutSelection.width > 0)
        x2 += layoutSelection.width;
    else
        x1 += layoutSelection.width;

    gint y1 = layoutSelection.y;
    gint y2 = y1;
    if(layoutSelection.height > 0)
        y2 += layoutSelection.height;
    else
        y1 += layoutSelection.height;

    // Find and mark the blocks that are selected.
    //
    // TODO: this is not a very efficient method.
    for(struct Block *b = l->blocks; b; b = b->next) {

        GtkAllocation a;
        DASSERT(b->grid);
        gtk_widget_get_allocation(b->grid, &a);

        int xmax = b->x + a.width;
        int ymax = b->y + a.height;

        if(     // Does the selection overlap in x
                ((b->x < x2 && xmax > x1) ||
                (xmax > x1 && b->x < x2))
                &&
                // Does the selection overlap in y
                ((b->y < y2 && ymax > y1) ||
                (ymax > y1 && b->y < y2))
          ) {
            if(!b->isSelected)
                SelectBlock(b);
        } else if(b->isSelected)
            UnselectBlock(b);
    }

    // Draw this box on the graphs newSurface.
    DrawLayoutNewSurface(l, -3.0, -3.0, NumPos);
    gtk_widget_queue_draw(layout);

    return TRUE; // FALSE to propagate the event further
}


#if 0
static gboolean
Enter_cb(GtkWidget *layout, GdkEventCrossing *e, struct Layout *l) {

    DSPEW();

    return FALSE; // FALSE to propagate the event further
}


static gboolean
Leave_cb(GtkWidget *layout, GdkEventCrossing *e, struct Layout *l) {

    DSPEW();

    return FALSE; // FALSE to propagate the event further
}
#endif


struct Layout *CreateLayout(struct Window *w, const char *path) {

    DASSERT(w);

    // If path is set, this will load the path of a block.  If the block
    // is a super block the super block will be flattened and will become
    // the graph (top block).
    struct QsGraph *g = qsGraph_create(path, 2/*maxThreads*/,
            0/*graph name*/, 0, QS_GRAPH_HALTED | QS_GRAPH_SAVE_ATTRIBUTES);
    if(!g)
        // Fail
        return 0;

    // This will create a thread that lets the graph blocks run async
    // graph commands.
    qsGraph_launchRunner(g);

    struct Layout *l = calloc(1, sizeof(*l));
    ASSERT(l, "calloc(1,%zu) failed", sizeof(*l));
    l->window = w;
    l->qsGraph = g;
    l->isHalted = true;

    qsGraph_saveConfig(g, true);

    GtkWidget *layout = gtk_layout_new(0, 0);
    DASSERT(layout);
    l->layout = layout;


    gtk_widget_set_events(layout,
            gtk_widget_get_events(layout) |
            GDK_BUTTON_PRESS_MASK |
            GDK_BUTTON_RELEASE_MASK |
#if 0
            GDK_ENTER_NOTIFY_MASK | 
            GDK_LEAVE_NOTIFY_MASK |
#endif
            GDK_POINTER_MOTION_MASK|
            GDK_STRUCTURE_MASK);
            // GDK_STRUCTURE_MASK seems to be broken for GTK layout.
    gtk_widget_set_name(layout, "layout");



    g_signal_connect(layout, "draw",
            G_CALLBACK(Draw_cb), l);
    g_signal_connect(layout, "button-press-event",
            G_CALLBACK(Press_cb), l);
    g_signal_connect(layout, "button-release-event",
            G_CALLBACK(Release_cb), l);
    g_signal_connect(layout, "motion-notify-event",
            G_CALLBACK(Motion_cb), l);
#if 0
    g_signal_connect(layout, "enter-notify-event",
            G_CALLBACK(Enter_cb), l);
    g_signal_connect(layout, "leave-notify-event",
            G_CALLBACK(Leave_cb), l);
#endif

    GtkWidget *scrolledWindow = gtk_scrolled_window_new(0, 0);

    gtk_scrolled_window_set_policy(
            GTK_SCROLLED_WINDOW(scrolledWindow),
            GTK_POLICY_ALWAYS, GTK_POLICY_ALWAYS);

    gtk_container_add(GTK_CONTAINER(scrolledWindow), layout);

    gtk_widget_set_hexpand(layout, TRUE);
    gtk_widget_set_vexpand(layout, TRUE);

    // TODO: Figure out a layout size here, maybe based on the root window
    // size.  This must be larger than the scroll window ever is.
#define L_SIZE  3000
    gtk_layout_set_size(GTK_LAYOUT(layout), l->w = L_SIZE, l->h = L_SIZE);
    //gtk_widget_set_size_request(layout, L_SIZE, L_SIZE);

    {
        // The management of the layout size and layout scroll window is a
        // total pain.  Blocks can get lost in a part of the layout that
        // is not accessible by the GUI user.  The GTK 3 interfaces
        // managing the layout scroll window is mystical.  There is no
        // easy way to enforce the rule "don't let widgets and drawings
        // get inaccessible in the layout".  It obviously should be able
        // to be automatic.  It was the first thing I thought of that was
        // needed; GTK 3 missed the mark.

        GtkAdjustment *ha = gtk_scrollable_get_hadjustment(
                GTK_SCROLLABLE(layout));
        g_signal_connect(ha, "value-changed", G_CALLBACK(Scrolled_cb),
                &l->scrollH);

        GtkAdjustment *va = gtk_scrollable_get_vadjustment(
                GTK_SCROLLABLE(layout));
        g_signal_connect(va, "value-changed", G_CALLBACK(Scrolled_cb),
                &l->scrollV);

        // Move the WorkArea scroll bars to a middle setting
        gdouble lo = gtk_adjustment_get_lower(ha);
        gdouble up = gtk_adjustment_get_upper(ha);
        gtk_adjustment_set_value(ha, lo + (up-lo)/2.0);
        l->scrollH = gtk_adjustment_get_value(ha);

        lo = gtk_adjustment_get_lower(va);
        up = gtk_adjustment_get_upper(va);
        gtk_adjustment_set_value(va, lo + (up-lo)/2.0);
        l->scrollV = gtk_adjustment_get_value(va);
    }

    ++numLayouts;


    if(path)
        // This will make this GUI data like struct Block and connections
        // stuff consistent with the quickstream graph we made in
        // qsGraph_create(path,...) above.
        //
        DisplayBlocksAndConnections(l);


    GtkWidget *overlay = gtk_overlay_new();

    gtk_container_add(GTK_CONTAINER(overlay), scrolledWindow);

    ASSERT(g_object_get_data(G_OBJECT(overlay), "Layout") == 0);
    // When the overlay is destroyed DestroyLayout_cb() is called to clean
    // up the quickstream graph that is being edited in the layout.
    // "overlay" is the widget that will be the of the notebook page
    // widget.
    g_object_set_data_full(G_OBJECT(overlay), "Layout", (void *) l,
            (void (*)(void *)) DestroyLayout_cb);
    ASSERT(g_object_get_data(G_OBJECT(overlay), "Layout") == l);
    l->overlay = overlay;

    ShowButtonBar(l, true);
    SetHaltButton(l, l->isHalted);
    SetQsFeedback(l);

    gtk_widget_show_all(overlay);

    return l;
}
