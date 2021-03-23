#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdbool.h>

#include <gtk/gtk.h>

#include "../include/quickstream/app.h"
#include "../include/quickstream/block.h"
#include "../include/quickstream/builder.h"

#include "../lib/debug.h"

#include "qsb.h"

/*
   Block with geo: ICOSG (Input,Constant,Output,SetterGetter)
   rotating round clock-wise.

                   ICOSG

 *******************************************
 *       |          const         |        |
 *       |************************|        |
 *       |        path.so         |        |
 * input |                        | output |
 *       |         name           |        |
 *       |************************|        |
 *       |    set    |     get    |        |
 *******************************************


    Block with geo: OCISG (Output,Constant,Input,SetterGetter)


                    OCISG

 *******************************************
 *        |         const          |       |
 *        |************************|       |
 *        |        path.so         |       |
 * output |                        | input |
 *        |         name           |       |
 *        |************************|       |
 *        |    set    |    get     |       |
 *******************************************



                   COSGI

 ******************************************
 *       |        output          |       |
 *       |************************|  set  |
 *       |        path.so         |       |
 * const |                        |*******|
 *       |         name           |       |
 *       |************************|  get  |
 *       |         input          |       |
 ******************************************



              etc .....


 and there are 5 more permutations that we allow.  We keep the input and
 output on opposing sides and the parameters constant opposing setter and
 getter; with setter always before getter.  That gives 8 total orientation
 permutations that we allow.

 */


// For toning, the number of pixels for a minimum reasonable size of a
// connector pin.
//
// TODO: Tone the multiplier.
static const double _pmin = CONNECTOR_THICKNESS*1.0001;
//static const double _pmin = CONNECTOR_THICKNESS*0.6;
// Compilers some time do bad conversions from double to unsigned int, so
// we burn an extra double so as to not get bad things.
static const guint PMIN = (guint) (_pmin);


static inline
void SetHorizontal1ConnectorSize(struct Connector *c, gint *w) {

    gint connectorWidth = CONNECTOR_THICKNESS;
    if(connectorWidth < c->numPins*PMIN)
        connectorWidth = c->numPins*PMIN;

    gtk_widget_set_size_request(c->widget,
            connectorWidth, CONNECTOR_THICKNESS);

    if(*w < connectorWidth)
        *w = connectorWidth;
}


static inline
void SetHorizontal2ConnectorSizes(struct Connector *c1,
        struct Connector *c2, gint *w) {

    gint connectorWidth1 = CONNECTOR_THICKNESS;
    if(connectorWidth1 < c1->numPins*PMIN)
        connectorWidth1 = c1->numPins*PMIN;

    gtk_widget_set_size_request(c1->widget,
            connectorWidth1, CONNECTOR_THICKNESS);
    

    gint connectorWidth2 = CONNECTOR_THICKNESS;
    if(connectorWidth2 < c2->numPins*PMIN)
        connectorWidth2 = c2->numPins*PMIN;

    gtk_widget_set_size_request(c2->widget,
            connectorWidth2, CONNECTOR_THICKNESS);

    // The block name label will be the size of these two connectors
    // if it is not already larger.
    if(*w < connectorWidth1 + connectorWidth2)
        *w = connectorWidth1 + connectorWidth2;
}


static inline
void SetVertical1ConnectorSize(struct Connector *c, gint *h) {

    gint connectorHeight = CONNECTOR_THICKNESS;
    if(connectorHeight < c->numPins*PMIN)
        connectorHeight = c->numPins*PMIN;

    gtk_widget_set_size_request(c->widget,
            CONNECTOR_THICKNESS, connectorHeight);

    if(*h < connectorHeight - 2*CONNECTOR_THICKNESS)
        *h = connectorHeight - 2*CONNECTOR_THICKNESS;
}


static inline
void SetVertical2ConnectorSizes(struct Connector *c1,
        struct Connector *c2,  gint *h) {

    gint connectorHeight1 = CONNECTOR_THICKNESS;
    if(connectorHeight1 < c1->numPins*PMIN)
        connectorHeight1 = c1->numPins*PMIN;

    gtk_widget_set_size_request(c1->widget,
            CONNECTOR_THICKNESS, connectorHeight1);


    gint connectorHeight2 = CONNECTOR_THICKNESS;
    if(connectorHeight2 < c2->numPins*PMIN)
        connectorHeight2 = c2->numPins*PMIN;

    gtk_widget_set_size_request(c2->widget,
            CONNECTOR_THICKNESS, connectorHeight2);


    if(*h < connectorHeight1 + connectorHeight2 - 2*CONNECTOR_THICKNESS)
        *h = connectorHeight1 + connectorHeight2 - 2*CONNECTOR_THICKNESS;
}


// Moves all the connectors to positions given by "geo".
//
// Set the orientation of connectors on the block.
void OrientConnectors(struct Block *b, enum ConnectorGeo geo,
        bool detachFirst) {

    DASSERT(b);

    if(detachFirst) {
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
    }

    // A vertical dimension used to determine the minimum size of the
    // block widget name label.  We use the path and name label to use up
    // extra space in the block.  The connectors keep the same thickness.
    //
    // This is the total size of the block if none of the connectors get
    // larger to accommodate for having a large number of connector
    // pins.
    //
    // These values, w, and h, will grow with the connector sizes in the
    // code to follow.
    gint w = 110;
    gint h = 2*CONNECTOR_THICKNESS;


    // First place constants, setters and getters.
    switch(geo) {
        case OCISG:
        case ICOSG:
            // horizontal connectors.
            b->constants.isHorizontal = true;
            b->constants.isSouthWestOfBlock = false;
            b->getters.isHorizontal = true;
            b->setters.isHorizontal = true;
            b->getters.isSouthWestOfBlock = true;
            b->setters.isSouthWestOfBlock = true;
            SetHorizontal1ConnectorSize(&b->constants, &w);
            SetHorizontal2ConnectorSizes(&b->setters, &b->getters, &w);
            gtk_grid_attach(GTK_GRID(b->grid),
                    b->constants.widget, 1, 0, 4, 1);
            gtk_grid_attach(GTK_GRID(b->grid),
                    b->getters.widget, 3, 4, 2, 1);
            gtk_grid_attach(GTK_GRID(b->grid),
                    b->setters.widget, 1, 4, 2, 1);
            break;
        case ISGOC:
        case OSGIC:
            // horizontal connectors.
            b->constants.isHorizontal = true;
            b->constants.isSouthWestOfBlock = true;
            b->getters.isHorizontal = true;
            b->setters.isHorizontal = true;
            b->getters.isSouthWestOfBlock = false;
            b->setters.isSouthWestOfBlock = false;
            SetHorizontal1ConnectorSize(&b->constants, &w);
            SetHorizontal2ConnectorSizes(&b->setters, &b->getters, &w);
            gtk_grid_attach(GTK_GRID(b->grid),
                    b->constants.widget, 1, 4, 4, 1);
            gtk_grid_attach(GTK_GRID(b->grid),
                    b->getters.widget, 3, 0, 2, 1);
            gtk_grid_attach(GTK_GRID(b->grid),
                    b->setters.widget, 1, 0, 2, 1);
            break;
        case COSGI:
        case CISGO:
            // vertical connectors.
            b->constants.isHorizontal = false;
            b->constants.isSouthWestOfBlock = false;
            b->getters.isHorizontal = false;
            b->setters.isHorizontal = false;
            b->getters.isSouthWestOfBlock = true;
            b->setters.isSouthWestOfBlock = true;
            SetVertical1ConnectorSize(&b->constants, &h);
            SetVertical2ConnectorSizes(&b->setters, &b->getters, &h);
            gtk_grid_attach(GTK_GRID(b->grid),
                    b->constants.widget, 0, 0, 1, 5);
            gtk_grid_attach(GTK_GRID(b->grid),
                    b->getters.widget, 5, 2, 1, 3);
            gtk_grid_attach(GTK_GRID(b->grid),
                    b->setters.widget, 5, 0, 1, 2);
            break;
        case SGOCI:
        case SGICO:
            // vertical connectors.
            b->constants.isHorizontal = false;
            b->constants.isSouthWestOfBlock = true;
            b->getters.isHorizontal = false;
            b->setters.isHorizontal = false;
            b->getters.isSouthWestOfBlock = false;
            b->setters.isSouthWestOfBlock = false;
            SetVertical1ConnectorSize(&b->constants, &h);
            SetVertical2ConnectorSizes(&b->setters, &b->getters, &h);
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
            // vertical connectors.
            b->input.isHorizontal = false;
            b->input.isSouthWestOfBlock = false;
            b->output.isHorizontal = false;
            b->output.isSouthWestOfBlock = true;
            SetVertical1ConnectorSize(&b->input, &h);
            SetVertical1ConnectorSize(&b->output, &h);
            gtk_grid_attach(GTK_GRID(b->grid),
                    b->input.widget, 0, 0, 1, 5);
            gtk_grid_attach(GTK_GRID(b->grid),
                    b->output.widget, 5, 0, 1, 5);
            break;
        case OCISG:
        case OSGIC:
            // vertical connectors.
            b->input.isHorizontal = false;
            b->input.isSouthWestOfBlock = true;
            b->output.isHorizontal = false;
            b->output.isSouthWestOfBlock = false;
            SetVertical1ConnectorSize(&b->input, &h);
            SetVertical1ConnectorSize(&b->output, &h);
            gtk_grid_attach(GTK_GRID(b->grid),
                    b->input.widget, 5, 0, 1, 5);
            gtk_grid_attach(GTK_GRID(b->grid),
                    b->output.widget, 0, 0, 1, 5);
            break;
        case COSGI:
        case SGOCI:
            // horizontal connectors.
            b->input.isHorizontal = true;
            b->input.isSouthWestOfBlock = true;
            b->output.isHorizontal = true;
            b->output.isSouthWestOfBlock = false;
            SetHorizontal1ConnectorSize(&b->input, &w);
            SetHorizontal1ConnectorSize(&b->output, &w);
            gtk_grid_attach(GTK_GRID(b->grid),
                    b->input.widget, 1, 4, 4, 1);
            gtk_grid_attach(GTK_GRID(b->grid),
                    b->output.widget, 1, 0, 4, 1);
            break;
        case CISGO:
        case SGICO:
            // horizontal connectors.
            b->input.isHorizontal = true;
            b->input.isSouthWestOfBlock = false;
            b->output.isHorizontal = true;
            b->output.isSouthWestOfBlock = true;
            SetHorizontal1ConnectorSize(&b->input, &w);
            SetHorizontal1ConnectorSize(&b->output, &w);
            gtk_grid_attach(GTK_GRID(b->grid),
                    b->input.widget, 1, 0, 4, 1);
            gtk_grid_attach(GTK_GRID(b->grid),
                    b->output.widget, 1, 4, 4, 1);
            break;
    }

    gtk_widget_set_size_request(b->pathLabel, w, h/2);
    gtk_widget_set_size_request(b->nameLabel, w, h/2);



    w = gtk_widget_get_allocated_width(b->grid);
    h = gtk_widget_get_allocated_height(b->grid);
    gtk_widget_queue_draw_area(b->grid, 0, 0, w, h);
    
    b->geo = geo;
}


void RotateCCWCB(GtkWidget *widget, gpointer data) {
    DASSERT(popupBlock);

    switch(popupBlock->geo) {
        case ICOSG:
            OrientConnectors(popupBlock, COSGI, true);
            break;
        case OCISG:
            OrientConnectors(popupBlock, CISGO, true);
            break;
        case ISGOC:
            OrientConnectors(popupBlock, SGOCI, true);
            break;
        case OSGIC:
            OrientConnectors(popupBlock, SGICO, true);
            break;
        case COSGI:
            OrientConnectors(popupBlock, OSGIC, true);
            break;
        case CISGO:
            OrientConnectors(popupBlock, ISGOC, true);
            break;
        case SGOCI:
            OrientConnectors(popupBlock, OCISG, true);
            break;
        case SGICO:
            OrientConnectors(popupBlock, ICOSG, true);
            break;
    }
}


void RotateCWCB(GtkWidget *widget, gpointer data) {
    DASSERT(popupBlock);

    switch(popupBlock->geo) {
        case ICOSG:
            OrientConnectors(popupBlock, SGICO, true);
            break;
        case OCISG:
            OrientConnectors(popupBlock, SGOCI, true);
            break;
        case ISGOC:
            OrientConnectors(popupBlock, CISGO, true);
            break;
        case OSGIC:
            OrientConnectors(popupBlock, COSGI, true);
            break;
        case COSGI:
            OrientConnectors(popupBlock, ICOSG, true);
            break;
        case CISGO:
            OrientConnectors(popupBlock, OCISG, true);
            break;
        case SGOCI:
            OrientConnectors(popupBlock, ISGOC, true);
            break;
        case SGICO:
            OrientConnectors(popupBlock, OSGIC, true);
            break;
    }
}


void FlipCB(GtkWidget *widget, gpointer data) {
    DASSERT(popupBlock);

    switch(popupBlock->geo) {
        case ICOSG:
            OrientConnectors(popupBlock, ISGOC, true);
            break;
        case OCISG:
            OrientConnectors(popupBlock, OSGIC, true);
            break;
        case ISGOC:
            OrientConnectors(popupBlock, ICOSG, true);
            break;
        case OSGIC:
            OrientConnectors(popupBlock, OCISG, true);
            break;
        case COSGI:
            OrientConnectors(popupBlock, CISGO, true);
            break;
        case CISGO:
            OrientConnectors(popupBlock, COSGI, true);
            break;
        case SGOCI:
            OrientConnectors(popupBlock, SGICO, true);
            break;
        case SGICO:
            OrientConnectors(popupBlock, SGOCI, true);
            break;
    }
}


void FlopCB(GtkWidget *widget, gpointer data) {
    DASSERT(popupBlock);

    switch(popupBlock->geo) {
        case ICOSG:
            OrientConnectors(popupBlock, OCISG, true);
            break;
        case OCISG:
            OrientConnectors(popupBlock, ICOSG, true);
            break;
        case ISGOC:
            OrientConnectors(popupBlock, OSGIC, true);
            break;
        case OSGIC:
            OrientConnectors(popupBlock, ISGOC, true);
            break;
        case COSGI:
            OrientConnectors(popupBlock, SGOCI, true);
            break;
        case CISGO:
            OrientConnectors(popupBlock, SGICO, true);
            break;
        case SGOCI:
            OrientConnectors(popupBlock, COSGI, true);
            break;
        case SGICO:
            OrientConnectors(popupBlock, CISGO, true);
            break;
    }
}
