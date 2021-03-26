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
#include "../lib/parameter.h"

#include "qsb.h"


// Draw a connection line on the page->oldLines surface.
//
// Queuing the draw event can be done later, so we can loop over all
// connections and call this.
void DrawConnection(struct Page *page,
        struct Pin *pin1, struct Pin *pin2) {

/////// LEFT OFF HEREEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE

}


void DrawAllConnections(struct Page *page) {


    if(!graph) return;




}


// x_root, y_root are the position of the mouse pointer in the root
// window.   The only sure reference frame is the root window frame, all
// other frames are not reliable, things move and event positions are
// relative to the first widget that may be letting the event get passed
// to a descendent widget.
//
// This will be used to set either the "from" pin or the "to" pin.  The
// "from" pin is just the first pin to be selected.  When the "to" pin is
// being selected the "from" pin must be already set.
//
struct Pin *GetConnectorPinAndPosition(GtkWidget *draw, double x_root,
        double y_root, struct Connector *c) {

    struct Pin *pin;

    // I can't believe this takes this many variables.
    uint32_t pinNum;
    double pos[2];
    double layoutPos[2];
    double p;
    double delta;
    double numPins = c->numPins;
    GetWidgetRootXY(draw, pos, pos+1);
    GetWidgetRootXY(c->block->page->layout, layoutPos, layoutPos+1);
    double width = gtk_widget_get_allocated_width(draw);
    double height = gtk_widget_get_allocated_height(draw);

    if(c->isHorizontal) {
        // The connector is oriented horizontally; so the pin number
        // depends on the x position of the pointer.
        delta = x_root - pos[0];
        p = numPins*delta/width;
        if(p < 0.0) pinNum = 0;
        else if(p >= numPins) pinNum = numPins - 1;
        else pinNum = p;
        pin = c->pins + pinNum;
        pin->connector->x = pos[0] + (pinNum+0.5)*(width/numPins) -
                layoutPos[0];
        pin->connector->y = (pos[1] + height/2.0) - layoutPos[1];
        if(pin->connector->isSouthWestOfBlock) {
            pin->connector->dx = 0.0;
            pin->connector->dy = 1.0;
        } else {
            pin->connector->dx = 0.0;
            pin->connector->dy = -1.0;
        }
    } else {
        // The connector is oriented vertically, so the pin number depends
        // on the y position of the pointer.
        delta = y_root - pos[1];
        p = numPins*delta/height;
        if(p < 0.0) pinNum = 0;
        else if(p >= numPins) pinNum = numPins - 1;
        else pinNum = p;
        pin = c->pins + pinNum;
        pin->connector->x = pos[0] + width/2.0 - layoutPos[0];
        pin->connector->y = pos[1] + (pinNum + 0.5)*(height/numPins) -
                layoutPos[1];
        if(pin->connector->isSouthWestOfBlock) {
            pin->connector->dx = 1.0;
            pin->connector->dy = 0.0;
        } else {
            pin->connector->dx = -1.0;
            pin->connector->dy = 0.0;
        }
    }

    if(!CanConnectFromPin(pin))
        // It could be trying to get either the fromPin or the toPin, it
        // does not matter, it's not a pin that we can connect to.
        return 0;
    if(!fromPin) {
        DASSERT(!toPin);
        // This is returning the fromPin
        return pin;
    }

    DASSERT(fromPin);

    // Okay.  This is returning the toPin; but we need to check that the
    // fromPin and this (toPin) pin are compatible.

    if(CanConnect2Pins(fromPin, pin))
        return pin;

    return 0; // not a valid pin to pin connection.
}


struct SetPinParameterDesc_st {

    struct Connector *connector;
    uint32_t pinIndex;
    const char *preName;
};


static
int SetPinParameterDesc(struct QsParameter *p,
            enum QsParameterKind kind,
            enum QsParameterType type,
            size_t size,
            const char *pName, struct SetPinParameterDesc_st *st) {

    DASSERT(st);
    DASSERT(st->connector);
    DASSERT(st->connector);
    DASSERT(st->pinIndex < st->connector->numPins);
    DASSERT(st->preName);

    struct Pin *pin = st->connector->pins + st->pinIndex;
    // Next call gets the next index in the connector->pins array.
    ++st->pinIndex;

    DASSERT(st->connector == pin->connector);

    pin->parameter = p;

    int len = snprintf(pin->desc, DESC_LEN, "%s \"%s\"",
        st->preName, pName);

    if(len > 0 && len < DESC_LEN)
        switch(type) {
            case QsDouble:
                snprintf(pin->desc+len, DESC_LEN-len,
                        " Double [%zu]", size/sizeof(double));
                break;
            case QsUint64:
                snprintf(pin->desc+len, DESC_LEN-len,
                        " Uint64 [%zu]", size/sizeof(uint64_t));
                break;
            case QsString:
                snprintf(pin->desc+len, DESC_LEN-len,
                        " String [%zu]", size);
                break;
            case QsNone:
                ASSERT(0, "Got parameter of type none");
                break;
    }
    return 0;
}


static inline void
SetPinParameterDescs(struct Connector *c, enum QsParameterKind kind,
        const char *preName) {

    struct SetPinParameterDesc_st st;
    st.connector = c;
    st.pinIndex = 0;
    st.preName = preName;

    qsParameterForEach(0, c->block->block, kind, 0, 0, 0,
        (int (*)(struct QsParameter *p,
            enum QsParameterKind kind,
            enum QsParameterType type,
            size_t size,
            const char *pName, void *userData)) SetPinParameterDesc,
        &st, 0);

}


static inline void SetPinPortDesc(struct Connector *c, const char *pre) {

    for(uint32_t i=0; i<c->numPins; ++i) {
        DASSERT(c == c->pins[i].connector);
        snprintf(c->pins[i].desc, DESC_LEN,
                        "%s port %" PRIu32, pre, i);
        c->pins[i].portNum = i;
    }
}


void MakeBlockConnector(GtkWidget *grid,
        const char *className/*for CSS*/,
        enum ConnectorKind ckind,
        struct Block *block) {

    GtkWidget *drawArea = gtk_drawing_area_new();

    //gtk_widget_set_can_focus(drawArea, FALSE);
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
    gtk_widget_set_size_request(drawArea, CONNECTOR_THICKNESS,
            CONNECTOR_THICKNESS);
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

    c->pins = calloc(c->numPins, sizeof(*c->pins));

    for(uint32_t i=0; i<c->numPins; ++i)
        c->pins[i].connector = c;

    switch(c->kind) {

        case Input:
            SetPinPortDesc(c, "Input");
            break;
        case Output:
            SetPinPortDesc(c, "Output");
            break;
        case Constant:
            SetPinParameterDescs(c, QsConstant, "Constant");
            break;
        case Getter:
            SetPinParameterDescs(c, QsGetter, "Getter");
            break;
        case Setter:
            SetPinParameterDescs(c, QsSetter, "Setter");
            break;
    }


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
