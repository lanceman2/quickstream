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

    CT_GET,
    CT_SET,
    CT_INPUT,
    CT_OUTPUT
};


// A data structure that is used while a connection is being
// drawn, and the connection is not finished yet.
struct ConnectionDraw {

    struct Block *from;
    struct Block *to;

    enum ConnectionType fromType;
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


 ***************************************
 *                get                  *
 *                                     *
 * input                        output *
 *                                     *
 *                set                  *
 ***************************************


  The user will act on one of these 4 block areas.

*/
//
//
struct Connector {

    /* Connector is something that represents a block and the
     * it's connections to input, output, get, or set that can
     * be passed as a pointer to GTK event callbacks. */
    //
    struct Block *block;
    // The GTK widget that will represent this connection area that is a
    // descendent (child or child's child) of the block widget.
    GtkWidget *widget;
    enum ConnectionType type;
};


struct Page;

struct Block {

    GtkWidget *container;
    GtkLayout *layout;
    struct Page *page;

    uint32_t numInputs, numOutputs;

    // Array of output ports.
    struct OutputPort *outputs;

    // Array of inputs
    struct InputPort *inputs;

    struct Connector get, set, input, output;

    gint x, y; // current position in layout widget.
};


struct Page {
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
    cairo_surface_t *newLine;
    cairo_surface_t *oldLines;
    //
    // width and height of the above surfaces.
    gint w, h;
};
