/* quickstreamBuilder (qsb) is a GUI program that knows a little about
 * quickstream.   quickstreamBuilder builds lists of blocks and input,
 * output, and parameter connections between them.
 */
struct Block;


// We make our own popover widget because we could not get GTK+3 to play
// nice.
struct Popover {

    GtkWidget *container;
    GtkWidget *label;
};


struct Page {
    // This is a g tree list of the blocks that are selected, i.e.
    // highlighted.  We can move all of them together with the mouse
    // pointer.
    GTree *selectedBlocks;

    // The widget that is in the page.  We also call it the work area.
    GtkWidget *layout;

    struct Block *blocks;// singly linked list of all blocks in the page.

    // For drawing new lines or the pointer selecting box.
    cairo_surface_t *newDrawSurface;
    //
    // Old lines that we drew that are not moving currently.
    cairo_surface_t *oldLines;

    // Widget that pops up help balloons for the connector pins.
    struct Popover connectorsPopover;

    // width and height of layout drawing area and the above
    // surfaces.
    gint w, h;

    // When we rotate a block we need to set this flag so that when the
    // page layout (workArea) is redrawn we require that the oldLines
    // surface get redrawn too.
    bool redrawOldLines;
};


enum ConnectorKind {
    Input,
    Constant,
    Output,
    Getter,
    Setter
};


// enum of geometric orientations of connector types - ConnectorGeo
//
// I - Input, O - Output, S - Setter, G - Getter, C - Constant
//
// The order in clock-wise rotation from the left side of the block.
// Note: the Setter is always to the left of the Getter or above it, but
// we always call them together SG, Setter than Getter.
//
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


#define DESC_LEN (128)

// A connector pin.
struct Pin {

    struct Connector *connector;

    // We keep a singly-linked list of pins for all parameter connections.
    // The "first" pin is the start of the list.  This list has a
    // corresponding and independent list in the parameters (struct
    // QsParameter).  This gives us a parameter connection list that
    // without which we would need to search through all the pins to find
    // connections each time we draw the connections.  There is a
    // connection list in the struct OsParameter, but we do not mirror
    // that, we just keep a list of all that are in the connection group.
    // The pins in the group have the same value for "first", which is the
    // first in the list.
    struct Pin *first, *next;

    uint32_t index; // This is also the port number for input and output.

    // The popover description, that we put in the popover label.
    char desc[DESC_LEN];

    union {
      // We are connecting to a parameter or a stream port number.
      struct QsParameter *parameter;
      // This is redundant for input and output since we have index.
      uint32_t portNum; // input or output
    };
};


// We need the length to include the '\0' string terminator.
#define CONNECTOR_CLASSNAME_LEN  (7) // Largest string is "output"


struct Connector {

    enum ConnectorKind kind; // See enum for list of kinds.
    char name[CONNECTOR_CLASSNAME_LEN]; // Largest string is "output"
    GtkWidget *widget; // GTK drawingArea
    struct Block *block;

    // Number of things we can connect to in this connector.  Or put
    // another way: the number of pins in this connector.
    uint32_t numPins;

    // This is an array that is allocated.  The array size is numPins.
    // If numPins is zero this is zero too, and is not allocated.
    struct Pin *pins;

    // The connector will not have connections if there are no inputs,
    // outputs, or parameters to connect to.  active = true if there is
    // something to connect to or from associated with this this widget,
    // else there is no associated inputs, outputs, or parameters to
    // connect to and the widget just has GTK events go to the bulk block
    // widget.
    bool active;

    // When the fromPin is selected we have a mouse button pressed and the
    // user is dragging/drawing a line from it.
    //
    // x, y, dx, dy are for connection line drawing.
    //
    //uint32_t selectedPin;
    //
    double x, y; // point to draw the line from for the connection.
    //
    // dx,dy marks the direction of the line from the connection pin,
    // "fromPin.  A cubic Bézier spline is what we draw from the fromPin.
    // dx, dy help use calculate the 2nd of the 4 control points that are
    // needed to draw a cubic Bézier spline.  dx,dy = 1, 0 is to the
    // right,  -1, 0 is to the left,  0, 1 is down, and 0, -1 is up; and
    // that's all the values dx, dy can have; right, left, down, and up.
    // We get these at the mouse press and as the mouse moves we do not
    // need to calculate them again until the next mouse press with a
    // fromPin.
    double dx, dy;

    // Is it oriented horizontally? Otherwise it's oriented vertically.
    bool isHorizontal;
    // If it is on the right or on the bottom of the block.
    bool isSouthWestOfBlock;
};


struct Block {

    GtkWidget *ebox; // block container widget.
    GtkWidget *grid; // block ebox child container widget.
    GtkWidget *pathLabel; // block path label widget.
    GtkWidget *nameLabel; // block name label widget.
    struct Page *page; // tab page that has this block in it.
    struct QsBlock *block;
    // All blocks have 5 connectors, by they active or not.
    struct Connector constants, getters, setters, input, output;
    struct Block *next; // for singly linked list of blocks in page.
    double x, y; // current position in layout widget.

    // Array of pointers to these block structs that this block
    // has connections to inputs from their outputs.
    //
    // If the value is 0 then there is no connection to the input port
    // with the index being the input port number.
    //
    // We use input connections to track both input and output connections
    // since each input port can only have one output connected to it.
    //
    // There is no mapping from QsBlock to this block (other than this
    // array), so without this array to track input and output connections
    // we'd have to search all the (struct Block) blocks to find and draw
    // each input/output connection.
    //
    // inputConnections array size is block->maxNumInputs.
    struct Pin **inputConnections;

    enum ConnectorGeo geo;
    bool isSelected;
};


// We only have one stream graph.
extern
struct QsGraph *graph;


// Current selected note book tab that is being acted on.
extern GtkNotebook *noteBook;


// More than one block can be moved at a time, so take care in using this
// variable.
extern
struct Block *movingBlock;


// This is set if we have a "from" connector and pin selected by the user.
// For connections in the process of being made.
extern
struct Pin *fromPin;
// As the user moves the pointer over possible "to" connection pins in
// a connector that is in a different connector from the fromPin
// connector.  This only gets set is the fromPin is compatible with the
// toPin.  When the connection is made the fromPin and toPin are reset to
// 0.  The graph keep the record of connections between blocks.
extern
struct Pin *toPin;


// The block that the popup menu is changing now.
extern
struct Block *popupBlock;


extern
void InitCSS(void);


// This adds a Gtk tree view to use as the block selecting thingy
// on the right side of the main window.
//
// tree is a passed in and is empty to start with.
//
extern
void AddBlockSelector(GtkWidget *tree, GtkEntry *entry);


extern
void CleanupBlockSelector(void);


// Returns the current selected block file from the text entry, which is
// also fed by the selector tree view widget.
extern
char *GetSelectedBlockFile(void);


// Return pointer if the block is successfully added, else return 0.
extern
struct Block *AddBlock(struct Page *page,
        GtkLayout *layout, const char *blockFile,
        double x, double y);

extern
void SelectBlock(struct Block *b);


extern
void UnselectBlock(struct Block *b);

extern
void UnselectAllBlocks(struct Page *page);


static inline void GetWidgetRootXY(GtkWidget *w, double *x, double *y) {

    gint ix, iy;
    gdk_window_get_origin(gtk_widget_get_window(w), &ix, &iy);
    *x = ix;
    *y = iy;
}


static inline void
Connect(GtkBuilder *builder, const char *id, const char *action,
        void *callback, void *userData) {
    g_signal_connect(gtk_builder_get_object(builder, id),
            action, G_CALLBACK(callback), userData);
}

extern
void MakeBlockConnector(GtkWidget *grid,
        const char *className/*for CSS*/,
        enum ConnectorKind ckind,
        struct Block *block);

extern
void StopDragingConnection(struct Page *page);


extern
void StartDragingConnection(struct Page *page);


// declared in qsb_OrientConnectors.c
extern
void OrientConnectors(struct Block *b,
        enum ConnectorGeo geo, bool detachFirst);
extern
void RotateCCWCB(GtkWidget *widget, gpointer data);
extern
void RotateCWCB(GtkWidget *widget, gpointer data);
extern
void FlipCB(GtkWidget *widget, gpointer data);
extern
void FlopCB(GtkWidget *widget, gpointer data);



// Block Connector GTK3+ widget event callback functions:
extern
gboolean ConnectorDraw_CB(GtkWidget *widget, cairo_t *cr,
        struct Connector *c);
//
extern
gboolean ConnectorMotion_CB(GtkWidget *draw,
        GdkEventButton *e, struct Connector *c);
//
extern
gboolean ConnectorEnter_CB(GtkWidget *draw,
        GdkEventButton *e, struct Connector *c);
//
extern
gboolean ConnectorLeave_CB(GtkWidget *draw,
        GdkEventButton *e, struct Connector *c);
//
extern
gboolean ConnectorRelease_CB(GtkWidget *draw,
        GdkEventButton *e, struct Connector *c);
//
extern
gboolean ConnectorPress_CB(GtkWidget *draw,
        GdkEventButton *e, struct Connector *c);



extern
bool CanConnectFromPin(struct Pin *pin);
extern
bool CanConnect2Pins(struct Pin *pin1, struct Pin *pin2);
extern
void Connect2Pins(struct Pin *pin1, struct Pin *pin2);


extern
void DrawAllConnections(struct Page *page);
extern
void DrawConnection(struct Page *page,
        struct Pin *pin1, struct Pin *pin2);
extern
void ClearAndDrawAllConnections(struct Page *page);

extern
struct Pin *GetConnectorPinAndPosition(GtkWidget *draw, double x_root,
        double y_root, struct Connector *c);


extern
bool GetLayoutSurfaces(struct Page *page, GtkWidget *widget);


// Connections line width.
extern
const double lineWidth;


extern
void GetConnectionLineColor(struct Connector *c,
        double *r, double *g, double *b, double *a);

// This, CONNECTOR_THICKNESS, is used in gtk_widget_set_size_request() for
// some of the inner block widgets.  This is a toning parameter.  It is
// the thickness of a connector widget, as they are like border that wrap
// the block.  Lots of the derived dimensions come from this number, so if
// you change it, do some testing to confirm that blocks and connectors
// draw nicely.
//
//#define CONNECTOR_THICKNESS     (100) // bigger
#define CONNECTOR_THICKNESS     (24) // normal

#define MIN_POPOVER_WIDTH   (160)
#define MIN_POPOVER_HEIGHT  CONNECTOR_THICKNESS




#define CREATE_BLOCK_BUTTON   (1) // 1 = left mouse
#define MOVE_BLOCK_BUTTON     (1) // 1 = left mouse
#define CONNECT_BUTTON        (1)

#define BLOCK_POPUP_BUTTON    (3) // 3 = right mouse
