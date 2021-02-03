/* quickstreamBuilder (qsb) is a GUI program that knows little about
 * quickstream.   quickstreamBuilder builds lists of blocks and
 * input and output connections between them in addition to connections
 * between parameter setters and getters.  The block names can be set.
 *
 * We use a minimalistic approach, and not add data structures when the
 * GTK widget API has most of the structure that we need.  For the most
 * part we just keep a list of block connections on each tab page.
 * Connections being input/output flow data and get/set control
 * parameters.
 *
 * TODO: Super block modules that are flow graphs in encapsulated in a
 * single block.
 */

struct Block;


// All blocks without any input ports.
//
struct Block *sources = 0;


enum ConnectionType {

    // There will be only 4 types of block
    // connection thingys.
    CT_SET = 0,
    CT_OUTPUT = 1,
    CT_GET = 2,
    CT_INPUT = 3
};


// Data structures for a directed graph of outputs to inputs.
//
struct OutputPort {

    // Each output port can connect to any number of block inputs.
    uint32_t numInputs;

    // From a given input connection say i where 0 <= i < numInputs,
    // to[i]->inputs[inputPortNums[i]] is the Inputport.

    uint32_t *inputPortNums;
    struct Block **to;
};
//
//
struct InputPort {

    // from->outputs[outputPortNum]->inputs[inputNum] is the block
    // that has the input for a given InputPort.
    //
    uint32_t outputPortNum;
    uint32_t inputNum;

    struct Block *from; // This input comes from "from".
};


/*
      Block and the 4 Connectors

      Their default positions are:


 ***************************************
 *                set                  *
 *                                     *
 * input                        output *
 *                                     *
 *                get                  *
 ***************************************

  
 
  But the blocks can be rotated and flipped.

  The user will act on one of these 4 block areas.

*/
//
//
struct Connector {

    /* Connector is something that represents a block and
     * it's connections to input, output, get, or set that can
     * be passed as a pointer to GTK event callbacks. */
    //
    struct Block *block;

    // Surface used to draw the GTK image widget.  This image will change
    // when the block is rotated, and we redraw on this surface.
    cairo_surface_t *surface;

    // The GTK widget that will represent this connection area that is a
    // descendent (child or child's child) of the block widget.
    GtkWidget *widget;
    
    enum ConnectionType type;
};


struct Page;

struct Block {


    // For the Page list of blocks as a stack.
    struct Block *next;

    GtkWidget *container;
    GtkWidget *grid;
    struct Page *page;

    uint32_t numInputs, numOutputs;

    // Array of output ports.
    struct OutputPort *outputs;

    // Array of inputs
    struct InputPort *inputs;

    // rotation in units of 90 degrees, so
    // rotation goes from 0, 1 = 90 deg, 2 = 180 deg
    // 3 = 270, then back to 0 using mod 4.
    uint32_t rotation;

    struct Connector get, set, input, output;

    gint x, y; // current position in layout widget.
};


struct Connection {

    // By convention "from" is a "output" or "get" and
    // to is a "input" or "set".
    struct Connector *from, *to;
};



struct Page {

    // List of all blocks as a stack.
    struct Block *blocks;

    // The main work/drawing area widget.
    GtkLayout *layout;

    GTree *selectedBlocks;

    // This struct is created for each GtkLayout widget that is the widget
    // that we draw our blocks and connections on.  We get one of these
    // per notebook tab and layout widget.
    
    // Where is the pointer relative to the layout widget.
    gint pointer_x, pointer_y;

    // point at which we draw from for from connector.
    // We calculate this at mouse press.
    gint x0, y0;
    
    // Set if this is for drawing a particular connection from.
    // Unset otherwise.
    struct Connector *from;

    // Connection lines that are drawn are saved in these surfaces.
    cairo_surface_t *newDrawSurface;
    cairo_surface_t *oldLines;
    //
    // width and height of the above surfaces.
    gint w, h;

    size_t numConnections;
    struct Connection *connections;
};


// This adds a Gtk tree view to use as the block selecting thingy.
extern
void AddBlockSelector(GtkWidget *tree);
