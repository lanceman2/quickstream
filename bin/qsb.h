/* quickstreamBuilder (qsb) is a GUI program that knows a little about
 * quickstream.   quickstreamBuilder builds lists of blocks and input,
 * output, and parameter connections between them.
 */
struct Block;


struct Page {
    // This is a g tree list of the blocks that are selected, i.e.
    // highlighted and shit.  We can move all of them together with the
    // mouse pointer.
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
    GtkWidget *connectorsPopover;

    // width and height of layout drawing area and the above
    // surfaces.
    gint w, h;
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

    // The connector will not have connections if there are no inputs,
    // outputs, or parameters to connect to.  active = true if there is
    // something to connect to or from associated with this this widget,
    // else there is no associated inputs, outputs, or parameters to
    // connect to and the widget just has GTK events go to the bulk block
    // widget.
    bool active;

    // Flag that says that the user has selected a parameter or stream
    // port number to connect.  Having selectionMade = true means that
    // the union{} below has a value set.
    bool selectionMade;

    union {
      // We are connecting to a parameter or a stream port number.
      // This is used when the mouse is moving.
      struct QsParameter *parameter;
      uint32_t portNum; // input or output
    };
};


struct Block {

    GtkWidget *ebox; // block container widget.
    GtkWidget *grid; // block ebox child container widget.
    GtkWidget *pathLabel; // block path label widget.
    GtkWidget *nameLabel; // block name label widget.
    struct Page *page; // tab page that has this block in it.
    struct QsBlock *block;
    struct Connector constants, getters, setters, input, output;
    struct Block *next; // for singly linked list of blocks in page.
    double x, y; // current position in layout widget.

    enum ConnectorGeo geo;
    bool isSelected;
};



extern
struct QsGraph *graph;


extern GtkNotebook *noteBook;


extern
struct Block *movingBlock;


// This is set if we have a "from" connector selected by the user.  For
// connections in the process of being made.
extern
struct Connector *fromConnector;

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



// This is uses in gtk_widget_set_size_request() for some of the block
// widgets.
#define MIN_BLOCK_LEN     (24)



#define CREATE_BLOCK_BUTTON   (1) // 1 = left mouse
#define MOVE_BLOCK_BUTTON     (1) // 1 = left mouse
#define CONNECT_BUTTON        (1)

#define BLOCK_POPUP_BUTTON    (3) // 3 = right mouse
