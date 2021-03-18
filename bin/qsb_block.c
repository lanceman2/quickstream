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
#include "../lib/Dictionary.h"

#include "qsb.h"



// Popup menu for block button click
static GtkMenu *blockPopupMenu = 0;
// This is the block that the user clicked on.
static struct Block *popupBlock = 0;

// This gets rebuilt each time it is used.
static GtkWidget *connectorPopupMenu = 0;


// The whole point of this program is to make blocks and connect the
// connectors (in these blocks) that are part of the blocks.  This is the
// current "FROM" connector that we started making a connection at.   This
// is set to zero after we have the "TO" connector or the user gives up
// trying to finish this connection that the user initiated.  The user
// only can make one connection at a time, with the mouse, hence this is
// just one variable, "fromConnector".
struct Connector *fromConnector = 0;

// This "to" connector can only be set if there is a "from" connector
// (fromConnector).   toConnector is the connector that was chosen
// by the user after the fromConnector.  This needs to be set so that
// the user can select the particular thingy to connect to. Though the
// connector is known, it cannot be connected until the parameter or port
// to connect to is chosen.
static struct Connector *toConnector = 0;



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


    /////////////////////////////////////////////////////////
    //     Now draw text like "input"
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


// The current connector is either the "from" or the "to" connector,
// depending on this logic here.
static inline
struct Connector *GetCurrentConnector(void) {

    if(toConnector) {
        DASSERT(toConnector != fromConnector);
        return toConnector;
    }
    DASSERT(fromConnector);
    return fromConnector;
}


static
gboolean ConnectPopupMenuItem_PortNum(GtkMenuItem *menuitem,
        uintptr_t n) {

    struct Connector *c = GetCurrentConnector();
    // Set the port number for this connector, c.
   
    if(c->selectionMade)
        WARN("RESELECTING");

    c->selectionMade = true;
    c->portNum = n;

    WARN("Got port %" PRIu32 " for block %s", c->portNum,
            c->block->block->name);

    DASSERT(connectorPopupMenu);

    gtk_widget_destroy(connectorPopupMenu);
    connectorPopupMenu = 0;

    if(c == fromConnector)
        StartMakingConnection(c->block->page);

    return TRUE; // TRUE = eat event
}


// Called for "selection-done" for connectorPopupMenu; which seems to be
// when the user does not select anything from the connector popup menu.
static void
ConnectSelectionDoneCB(GtkMenuShell *m, void *userdata) {

    gtk_widget_destroy(connectorPopupMenu);
    connectorPopupMenu = 0;

    DASSERT(!toConnector);
    if(fromConnector) {
        fromConnector->selectionMade = false;
        fromConnector = 0;
    }
}


// Returns the number of items in the popup menu, and 0 if there is no
// popup, because there are no compatible items.
//
// When this is called it should already be determined that connections
// can be made; so there will be at least one item in the connection
// popup menu.
//
static inline uint32_t
CreateShowConnectorPopupMenu(struct Connector *c, GdkEvent *e) {

    DASSERT(c->active);


    struct QsBlock *b = c->block->block;
    struct QsSimpleBlock *smB = (struct QsSimpleBlock *) b;

WARN("block=%p  toConnector=%p", smB, toConnector);

    if(connectorPopupMenu)
         gtk_widget_destroy(connectorPopupMenu);
    connectorPopupMenu = gtk_menu_new();
    g_signal_connect(GTK_WIDGET(connectorPopupMenu), "selection-done",
            G_CALLBACK(ConnectSelectionDoneCB), 0/*userData*/);

    uint32_t numItems = 0;
    uint32_t activeItems = 0;

    switch(c->kind) {
        case Input:
        {
            // Whither c be a starting point (from) of a connection or an
            // end point (to) the popup items are the same for this
            // case.
            //
            const size_t LABEL_LEN = 64;
            char label[LABEL_LEN];
            numItems = b->maxNumInputs;
            for(uintptr_t i=0; i<numItems; ++i) {
                snprintf(label, LABEL_LEN, "Input Port %zu", i);
                GtkWidget *w = gtk_menu_item_new_with_label(label);
                gtk_widget_add_events(w, GDK_BUTTON_PRESS_MASK);
                gtk_menu_attach(GTK_MENU(connectorPopupMenu), w,
                        0, 1, i, i+1);
                if(i >= smB->numInputs || smB->inputs[i].block == 0) {
                    // This input port, i, can be connected, because it's
                    // not connected yet.
                    ++activeItems;
                    g_signal_connect(GTK_WIDGET(w), "activate",
                        G_CALLBACK(ConnectPopupMenuItem_PortNum),
                        (void *) i);
                } else
                    // This input port, i, is already connected, so we
                    // cannot connect it again.
                    gtk_widget_set_sensitive(GTK_WIDGET(w), FALSE);
                gtk_widget_show(w);
            }
            break;
        }
        case Output:

            break;
        case Constant:

            break;
        case Getter:

            break;
        case Setter:

            break;
    }

    ASSERT(numItems > 0, "Write more code HERE");
    DASSERT(numItems > 0);
    DASSERT(activeItems > 0);


    gtk_menu_popup_at_pointer(GTK_MENU(connectorPopupMenu), e);

    return numItems;
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


// The connector is a GTK drawingArea widget, "draw".
static gboolean ConnectorPress_CB(GtkWidget *draw,
        GdkEventButton *e, struct Connector *c) {

    DASSERT(c);
    DASSERT(c->active);
    ASSERT(c->block->block->isSuperBlock == 0, "Write this code");

    if(e->button != CONNECT_BUTTON)
        // Event goes to next parent widget.
        return FALSE;

WARN("fromConnector=%p  toConnector=%p", fromConnector, toConnector);

    // This is either the event where a user is first pressing on a
    // connector, or the user has a "from" connector already selected
    // and they are pressing on the next "to" connector.

    toConnector = 0;

    if(!fromConnector || fromConnector == c) {
        if(CheckConnectionFromPossible(c))
            fromConnector = c;
        else {
            DASSERT(fromConnector == 0);
            // Event goes to next parent widget.
            return FALSE;
        }
    } else {
        if(CheckConnectionPossible(fromConnector, c))
            toConnector = c;
        else {
            DSPEW("No connection possible");
            // Event goes to next parent widget.
            return FALSE;
        }
    }


    CreateShowConnectorPopupMenu(c, (GdkEvent *) e);

    return TRUE; // TRUE = eat this event
}


static void MakeBlockConnector(GtkWidget *grid,
        const char *className/*for CSS*/,
        enum ConnectorKind ckind,
        struct Block *block,
        gint x, gint y, gint w, gint h) {

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

    gtk_widget_show(drawArea);
    gtk_widget_set_size_request(drawArea, MIN_BLOCK_LEN, MIN_BLOCK_LEN);
    gtk_widget_set_name(drawArea, className);
    gtk_grid_attach(GTK_GRID(grid), drawArea, x, y, w, h);


    struct Connector *c;
    switch(ckind) {
        case Constant:
            c = &block->constants;
            block->constants.active = qsDictionaryIsEmpty(
                    ((struct QsSimpleBlock *)
                        block->block)->constants)?false:true;
            break;
        case Getter:
            c = &block->getters;
            block->getters.active = qsDictionaryIsEmpty(
                    ((struct QsSimpleBlock *)
                        block->block)->getters)?false:true;
            break;
        case Setter:
            c = &block->setters;
            block->setters.active = qsDictionaryIsEmpty(
                    ((struct QsSimpleBlock *)
                        block->block)->setters)?false:true;
            break;
        case Input:
            c = &block->input;
            block->input.active =
                (block->block->maxNumInputs)?true:false;
            break;
        case Output:
            c = &block->output;
            block->output.active =
                (block->block->maxNumOutputs)?true:false;
            break;
    }

    c->kind = ckind;
    c->widget = drawArea;
    c->block = block;
    snprintf(c->name, CONNECTOR_CLASSNAME_LEN, "%s", className);

    g_signal_connect(G_OBJECT(drawArea), "draw",
            G_CALLBACK(ConnectorDraw_CB), c/*userData*/);


    if(!c->active)
        // If the connector is not able to make connections we do not need
        // the next few callbacks setup.
        return;

#if 0
    g_signal_connect(GTK_WIDGET(drawArea), "motion-notify-event",
            G_CALLBACK(ConnectorMotion_CB), c/*userData*/);
    g_signal_connect(GTK_WIDGET(drawArea), "enter-notify-event",
            G_CALLBACK(ConnectorEnter_CB), c/*userData*/);
    g_signal_connect(GTK_WIDGET(drawArea), "leave-notify-event",
            G_CALLBACK(ConnectorLeave_CB), c/*userData*/);
    g_signal_connect(GTK_WIDGET(drawArea), "button-release-event",
            G_CALLBACK(ConnectorRelease_CB), c/*userData*/);
#endif
    g_signal_connect(GTK_WIDGET(drawArea), "button-press-event",
            G_CALLBACK(ConnectorPress_CB), c/*userData*/);
}


static gboolean
Block_buttonReleaseCB(GtkWidget *ebox,
        GdkEventButton *e, struct Block *block) {


WARN();
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
        StopMakingConnection(block->page);


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


static void RotateCCWCB(GtkWidget *widget, gpointer data) {
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


static void RotateCWCB(GtkWidget *widget, gpointer data) {
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


static void FlipCB(GtkWidget *widget, gpointer data) {
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


static void FlopCB(GtkWidget *widget, gpointer data) {
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


static void RemovePopupBlockCB(GtkWidget *widget, gpointer data) {
    DASSERT(popupBlock);
    DestroyBlock(popupBlock);
    popupBlock = 0;

WARN("data=%zu", (uintptr_t) data);
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
            GDK_BUTTON_PRESS_MASK);

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
