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
//#include "../lib/Dictionary.h"

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


static inline void GetConnectionColor(enum ConnectorKind ckind,
        double *r, double *g, double *b, double *a) {

    switch(ckind) {
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
   rotating round clock-wise.

 *******************************************
 *       |          const         |        |
 *       |************************|        |
 *       |        path.so         |        |
 * input |                        | output |
 *       |         name           |        |
 *       |************************|        |
 *       |    set    |     get    |        |
 *******************************************

 
 and there are 7 more permutations that we allow.  We keep the input and
 output on opposing sides and the parameters constant opposing setter and
 getter; with setter always before getter.  That gives 8 total orientation
 permutations that we allow.

 */


static gboolean ConnectorDraw_CB(GtkWidget *widget,
        cairo_t *cr,
        struct Connector *c) {

    guint width, height;
    GtkStyleContext *context;

    context = gtk_widget_get_style_context (widget);

    width = gtk_widget_get_allocated_width(widget);
    height = gtk_widget_get_allocated_height(widget);

    gtk_render_background(context, cr, 0, 0, width, height);

    DASSERT(c);

    if(!c->active) return FALSE;

    ASSERT(!c->block->block->isSuperBlock,
            "Write this code to work with super blocks");

    // We will need a width or height of each strip.
    double delta;
    // The drawing fucks up without converting this to a double.
    // Dividing double by and int is a problem.
    double numPins = c->numPins;

    // Draw strips that brake up the connectors into sections.
    // The sections are the pins in the connectors.
    //
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.2);
    //
    switch(c->block->geo) {
        case ICOSG:
        case OCISG:
        case ISGOC:
        case OSGIC:
            switch(c->kind) {
                case Setter:
                case Getter:
                case Constant:
                    // Connector is horizontal.
                    delta = width/numPins;
                    for(uint32_t i=1; i<c->numPins; i += 2)
                        cairo_rectangle(cr, i*delta, 0, delta, height);
                    break;
                case Input:
                case Output:
                    // Connector is vertical.
                    delta = height/numPins;
                    for(uint32_t i=1; i<c->numPins; i += 2)
                        cairo_rectangle(cr, 0, i*delta, width, delta);
                    break;
            }
            break;
        case COSGI:
        case CISGO:
        case SGOCI:
        case SGICO:
            switch(c->kind) {
                case Input:
                case Output:
                    // Connector is horizontal.
                    delta = width/numPins;
                    for(uint32_t i=1; i<c->numPins; i += 2)
                        cairo_rectangle(cr, i*delta, 0, delta, height);
                    break;
                case Setter:
                case Getter:
                case Constant:
                    // Connector is vertical.
                    delta = height/numPins;
                    for(uint32_t i=1; i<c->numPins; i += 2)
                        cairo_rectangle(cr, 0, i*delta, width, delta);
                    break;
            }
            break;
    }
    //
    cairo_fill(cr);


    const double radius= 2.3;
    const double delta2 = delta/2.0;
    double h2 = height;
    h2 /= 2.0;
    double w2 = width;
    w2 /= 2.0;

    // Draw pin circles in the center of the strips that brake up the
    // connectors into sections.  The sections are the pins in the
    // connectors.
    //
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.3);
    //
    switch(c->block->geo) {
        case ICOSG:
        case OCISG:
        case ISGOC:
        case OSGIC:
            switch(c->kind) {
                case Setter:
                case Getter:
                case Constant:
                    // Connector is horizontal.
                    for(uint32_t i=0; i<c->numPins; ++i)
                        cairo_arc(cr, i*delta + delta2, h2,
                                radius, 0, 2.0*M_PI);
                    break;
                case Input:
                case Output:
                    // Connector is vertical.
                    for(uint32_t i=0; i<c->numPins; ++i)
                        cairo_arc(cr, w2, i*delta + delta2,
                                radius, 0, 2.0*M_PI);
                    break;
            }
            break;
        case COSGI:
        case CISGO:
        case SGOCI:
        case SGICO:
            switch(c->kind) {
                case Input:
                case Output:
                    // Connector is horizontal.
                    for(uint32_t i=0; i<c->numPins; ++i)
                          cairo_arc(cr, i*delta + delta2, h2,
                                radius, 0, 2.0*M_PI);
                    break;
                case Setter:
                case Getter:
                case Constant:
                    // Connector is vertical.
                    for(uint32_t i=0; i<c->numPins; ++i)
                        cairo_arc(cr, w2, i*delta + delta2,
                                radius, 0, 2.0*M_PI);
                    break;
            }
            break;
    }
    //
    cairo_fill(cr);


    /////////////////////////////////////////////////////////
    //     Now draw text like "input", "output", "set",
    //     "get", and "const"
    //
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
            switch(c->kind) {
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
            switch(c->kind) {
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


    return FALSE;
}


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


#if 0
// Here is the complete list of possible kinds of parameters connections:
//
//   1. Getter    to  Setter
//   2. Constant  to  Setter
//   3. Constant  to  Constant
//
static
int CanConnectParametersCB(struct QsParameter *p,
            enum QsParameterKind kind,
            enum QsParameterType type,
            size_t size,
            const char *pName, struct QsParameter **connectTo) {

    DASSERT(p);
    DASSERT(p->kind == kind);
    DASSERT(p->type == type);
    DASSERT(p->size == size);
    DASSERT(*connectTo);

    struct QsParameter *P = *connectTo;

    DASSERT(P->type == type);
    DASSERT(P->size == size);

    if(kind == QsSetter && p->first)
        // A setter cannot be connected more than once.
        return 0;
    if(P->kind == QsSetter && P->first)
        // A setter cannot be connected more than once.
        return 0;


    if(
        (P->kind == QsGetter   &&    kind == QsSetter) ||
        (   kind == QsGetter   && P->kind == QsSetter) ||
        (P->kind == QsConstant &&    kind == QsSetter) ||
        (   kind == QsConstant && P->kind == QsSetter) ||
        (P->kind == QsConstant &&    kind == QsConstant)) {
        // We can connect these parameters?  We use *connectTo = 0
        // as a flag that answers yes to that question.
        *connectTo = 0;
        return 1; // Stop calling the this function.
    }

    // else we cannot connect these parameters.
    return 0;
}


// Return the Answer the Question: Is a Connection Possible?
//
// The from connector, "from", is already selected, but this is deciding
// whither or not we can connect from "from" to connector "to".  If not
// the user will not see it as an option.
//
static
bool CheckConnectionPossible(struct Connector *from,
        struct Connector *to) {

    DASSERT(from);
    DASSERT(to);

    DASSERT(!from->block->block->isSuperBlock);
    DASSERT(!to->block->block->isSuperBlock);


    switch(from->kind) {
        case Input:
            if(to->kind != Output)
                return false;
            // Connecting from a Input to an Output.
            //
            // Since each output port may have any number of
            // connections from input ports, we will show all output
            // ports in the connector popup menu.
            return true;
        case Output:
        {
            if(to->kind != Input)
                return false;
            // Connecting from a Output to an Input.
            //
            // There must be an empty input port or we can't connect.
            struct QsSimpleBlock *smB =
                (struct QsSimpleBlock *) to->block->block;
            if(smB->numInputs < smB->block.maxNumInputs)
                // We are not using all possible input ports, so we go
                // to the next step.
                return true;
            // But we could have unconnected port before the highest
            // port, so we check for that now.
            uint32_t i=smB->numInputs-1;
            for(;i!=-1; --i)
                if(!smB->inputs[i].block)
                    // this input port i is not connected.
                    return true;
            if(i == -1)
                // We searched all ports and all are in use.
                return false;
            // There is an input port not in use.
            return true;
        }
        case Setter:
        case Getter:
        case Constant:
        {
            // Now we need to see if there are any parameters that
            // this "from" blocks' parameter can connect to.
            struct QsParameter *fromParameter = from->parameter;

            qsParameterForEach(0, to->block->block,
                QsAny/*kind*/,
                from->parameter->type,
                from->parameter->size,
                0/*name 0=any*/,
                (int (*)(struct QsParameter *p,
                    enum QsParameterKind kind,
                    enum QsParameterType type,
                    size_t size,
                    const char *pName, void *))
                    CanConnectParametersCB/*callback*/,
                    &fromParameter/*userData*/, 0/*flags*/);
            return fromParameter?false:true;
        }
    }

    DASSERT(0, "We should not get here");
    return true;
}


// This just answers the question: Is a connection possible "from" this
// connector?  We do not know the other, "to", connector yet.
//
// TODO: This does not bother looking at all the other blocks, to make
// sure that there is a compatible "to" connector available.  Doing so
// may make the user interface a little confusing.
bool
CheckConnectionFromPossible(struct Connector *c) {

    struct QsBlock *b = c->block->block;
    ASSERT(!b->isSuperBlock);
    struct QsSimpleBlock *smB = (struct QsSimpleBlock *) b;

    switch(c->kind) {
        case Input:
        {
            // Inputs can only be connected to once, so if there
            // is an unused input port available, then we may be
            // able to connect to it.
            //
            for(uint32_t i = b->maxNumInputs; i != -1; --i) {
                if(i >= smB->numInputs)
                    // There is an unused input port possible.
                    return true;
                if(smB->inputs[i].block == 0)
                    // There is an unused input port possible.
                    return true;
                // This input port, i, is in use.
            }
        }
        break;
        case Output:
        {
            // MORE CODE HERE...
            return false;
        }
        break;
        case Getter:
        case Setter:
        case Constant:
        {
            // MORE CODE HERE...
            return false;
        }
        break;
    }

    ASSERT(0, "We should not get here");
    return false;
}
#endif


static gboolean ConnectorMotion_CB(GtkWidget *draw,
        GdkEventButton *e, struct Connector *c) {


    return FALSE; // TRUE = eat event
}


static gboolean ConnectorEnter_CB(GtkWidget *draw,
        GdkEventButton *e, struct Connector *c) {


    return TRUE; // TRUE = eat event
}



static gboolean ConnectorLeave_CB(GtkWidget *draw,
        GdkEventButton *e, struct Connector *c) {


    return TRUE; // TRUE = eat event
}



static gboolean ConnectorRelease_CB(GtkWidget *draw,
        GdkEventButton *e, struct Connector *c) {


    return FALSE; // TRUE = eat event
}


// The connector is a GTK drawingArea widget, "draw".
static gboolean ConnectorPress_CB(GtkWidget *draw,
        GdkEventButton *e, struct Connector *c) {

    DASSERT(c);
    DASSERT(c->active);
    ASSERT(c->block->block->isSuperBlock == 0, "Write this code");


    if(e->button == CONNECT_BUTTON) {
        // Event goes to next parent widget.

        return FALSE; // pass the event to the parent block widget.
    }


    return FALSE; // TRUE = eat this event
}


static void MakeBlockConnector(GtkWidget *grid,
        const char *className/*for CSS*/,
        enum ConnectorKind ckind,
        struct Block *block) {

    GtkWidget *drawArea = gtk_drawing_area_new();

    gtk_widget_set_can_focus(drawArea, TRUE);
    gtk_widget_add_events(drawArea,
            GDK_ENTER_NOTIFY_MASK|
            GDK_LEAVE_NOTIFY_MASK |
            GDK_SCROLL_MASK |
            GDK_POINTER_MOTION_MASK |
            GDK_BUTTON_RELEASE_MASK |
            GDK_BUTTON_PRESS_MASK|
            GDK_STRUCTURE_MASK);

    ASSERT(!block->block->isSuperBlock, "Write more code HERE");
    struct QsSimpleBlock *smB = (struct QsSimpleBlock *) block->block;
    
    gtk_widget_show(drawArea);
    gtk_widget_set_size_request(drawArea, MIN_BLOCK_LEN, MIN_BLOCK_LEN);
    gtk_widget_set_name(drawArea, className);

    struct Connector *c;

    switch(ckind) {
        case Constant:
            c = &block->constants;
            c->numPins = smB->numConstants;
            break;
        case Getter:
            c = &block->getters;
            c->numPins = smB->numGetters;
            break;
        case Setter:
            c = &block->setters;
            c->numPins = smB->numSetters;
            break;
        case Input:
            c = &block->input;
            c->numPins = block->block->maxNumInputs;
            break;
        case Output:
            c = &block->output;
            c->numPins = block->block->maxNumOutputs;
            break;
    }

    c->kind = ckind;
    c->widget = drawArea;
    c->block = block;
    snprintf(c->name, CONNECTOR_CLASSNAME_LEN, "%s", className);

    g_signal_connect(G_OBJECT(drawArea), "draw",
            G_CALLBACK(ConnectorDraw_CB), c/*userData*/);

    c->active = (c->numPins)?true:false;

    if(!c->active)
        // If the connector is not able to make connections we do not need
        // the next few callbacks setup.
        return;

    g_signal_connect(GTK_WIDGET(drawArea), "motion-notify-event",
            G_CALLBACK(ConnectorMotion_CB), c/*userData*/);
    g_signal_connect(GTK_WIDGET(drawArea), "enter-notify-event",
            G_CALLBACK(ConnectorEnter_CB), c/*userData*/);
    g_signal_connect(GTK_WIDGET(drawArea), "leave-notify-event",
            G_CALLBACK(ConnectorLeave_CB), c/*userData*/);
    g_signal_connect(GTK_WIDGET(drawArea), "button-release-event",
            G_CALLBACK(ConnectorRelease_CB), c/*userData*/);
    g_signal_connect(GTK_WIDGET(drawArea), "button-press-event",
            G_CALLBACK(ConnectorPress_CB), c/*userData*/);
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
