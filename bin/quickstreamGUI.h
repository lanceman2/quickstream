// TODO: Maybe split up this file into more than one file.  That would
// speed up compile and edit development cycles.  With this file being
// required by most other files in the program, touching this file causes
// most objects in the program to be re-compiled with a program rebuild.
// This file is very central to the quickstreamGUI program.
// It's not so bad with a "make -j 7".
//
//
// A large portion of the data structures in this file/program are
// redundant, in that the information contained in them is already
// contained in the data structures in libquickstream.so.  For example: we
// have block connection lists for the graphically displayed quickstream
// block connections; where most of that information is in data structures
// in libquickstream.so.  In this case we could justify this redundancy by
// making the code less complex by having more straight forward access
// to the connection information.  If we tried to simplify the accessing
// to connection related information in libquickstream.so which could
// require more functions added to the libquickstream.so API, where-by
// adding more complexity to the libquickstream.so code, and we don't
// would to do that.  We did add some functions to libquickstream.so
// for quickstreamGUI, but only to some limit.  We did not completely
// decouple the quickstreamGUI C code from all the private header files in the
// libquickstream.so source files; but libquickstream.so and all it's
// source files are independent of the source files in quickstreamGUI.
//
//  quickstreamGUI source depends on libquickstream.so source
//
//   but
// 
//  libquickstream.so source is not dependent on quickstreamGUI source
//
//


// GNUradio RANT:  gnuradio-companion does not link with
// libgnuradio-runtime.so (if it does it's not using it effectively), and
// the result is that they add an intermediate computer language to store
// the state of the GNUradio GUI program.  That adds complexity to the
// problem.  It adds a lot to the problem that correspondingly does not
// exist in quickstream.  The GNUradio block builder has to write a lot
// more code.  The management and consistency of use of this "new
// language" as it is used between libgnuradio-runtime.so, the blocks, and
// the gnuradio-companion becomes a thing; where in compared to the
// quickstream framework does not even have the concept.  That is a shit
// ton of work not in existence in quickstream.  That is, in this lowest
// order approximation, infinitely times better.  Okay: that is unless the
// use of the "new language" provides a positive useful function for users
// at some level.  I argue there is no positive useful function for this
// "new language", only negative.  Negative, like making users do mundane
// work to use GNUradio.  There is no information in the "new language"
// files that is not in the blocks, and the constructed programs that use
// them.  The construct of this "new language" introduces unnecessary and
// useless protocols.
//
// I'd guess that the introduction of this "new YML based language" came
// about for two reasons:
//
//  1. to save state unique to gnuradio-companion, and
//
//  2. to separate the development of gnuradio-companion and
//     libgnuradio-runtime.so.
//
// I see that both of these reasons as not "directly" helping users.  I
// see the complexity added from this as hindering users.  This adds
// addition mundane work to use GNUradio.  Also; the separation that this
// adds makes many in-the-loop development features impossible.
// quickstreamGUI runs with the quickstream run-time library automatically
// verifying consistency, without introducing protocols that aren't there
// otherwise.
//
// I see no good in that kind of design pattern.
//
//
// Okay.  Got another point.  Because quickstreamGUI can run the stream
// graphs in it's process, it (the quickstream package) has a much higher
// standard of quality than it would otherwise.  gnuradio-companion can
// only fork and execute the stream graphs, because running them in the
// same process as gnuradio-companion is not in the design.  The fork and
// execute scheme is robust even for very low quality executables.  Yes,
// of course quickstreamGUI can also fork and execute the stream graphs
// it edits.
//

//
// I've always had a problem with configuration files.  They always, or at
// least tend to, evolve into another computer language that is unique to
// the application that spawned it.  I'd argue that it is best to put off
// configuration file development to as late as possible in the code
// development, and it is best not to have it at all.
//


// TODO: get rid of this CPP macro crap.  For now we need it to keep the
// border width of the GTK block widget thing the same in all the code.
#define STR(s)   XSTR(s)
#define XSTR(s)  #s
#define BW 3
#define BW_STR   STR(BW)


// If we have a terminal bar of ports that is tall and skinny CON_LEN is
// the minimum height and CON_THICKNESS is the width, like so:
//
//     ___
//    | . |
//    |   |
//    | . |
//    |   |
//    | . |
//    |   |
//    | . |
//    |___|
//
//
// else if a terminal bar of ports that is short and fat CON_LEN is the
// minimum larger dimension and CON_THICKNESS is the height, like so:
//
//       _________________________
//      |  .  .  .  .  .  .  .  . |
//      |_________________________|
//
//
// Minimum terminal drawing area size:
#define CON_LEN              50
#define CON_THICKNESS        20


#define PORT_ICON_WIDTH  17.0
#define PORT_ICON_PAD    10.0 
// PIN_LW must be less than PORT_ICON_WIDTH and CON_THICKNESS
#define PIN_LW    (1.3)        // pin icon line width
#define HW        (PIN_LW/2.0) // 1/2 pin icon line width



struct Color {
    double r, g, b, a;
};


// We have 4 types of ports (connection terminal pins).
enum Type {

    In = 0, // stream
    Out,// stream
    Set,// setter control parameter
    Get,// getter control parameter
    NumTypes
};


// We have 4 positions that connection terminal (ports) can be at any of
// the 4 sides of a rectangle.
enum Pos {

    Left = 0,            
    Right,
    Top,
    Bottom,
    NumPos
};


// How to place a block on the layout
struct GUILayout {

    double x, y;

    // Position of  In, Out, Set, Get
    //  2 bits each x 4 = 8 bits.
    //  In is the first 2 bits.
    //  Out is the next 2 bits.
    //  and so on...
    uint8_t orientation;
};


struct Window {

    GtkWidget *win; // The top level window widget.

    GtkNotebook *notebook;

    // Current showing tab layout.
    struct Layout *layout;

    GtkTreeView *treeView;

    // The window pane that is between the layout and the tree view.
    GtkPaned *paned;
};


struct Connection;


struct Layout {

    // overlay is the parent of the GTK3 layout.  It is also the page
    // widget that is in the notebook.
    GtkWidget *overlay;

    // buttonBar is a HORIZONTAL gtk_box.  It shows on top of the layout.
    GtkWidget *buttonBar;

    // Lists of blocks:
    struct Block *selectedBlocks; // selected blocks
    struct Block *blocks; // all in the layout


    // Graph interactive state changing stuff:
    GtkToggleButton *haltButton;
    bool isHalted;
    GtkToggleButton *runButton;
    bool isRunning;
    bool buttonBarShowing;
    GtkWidget *saveButton;

    // The last filename path that successfully did a "Save As".
    // Points to malloc/strdup memory, or is 0 if not set.
    //
    // The "save" is done as 3 files: filename.c filename.so and
    // filename.   We have the filename without any a ".c" or ".so"
    // suffix on the end.
    //
    // It has an extra 3 chars at the end so we can add the suffixes
    // (.c .so) as we play with it in the code.
    char *lastSaveAs;


    // TODO: Will there be more than one layout per qsGraph?  So like, we
    // could have many edit views for a given qsGraph.  Pushing changes
    // between the views will be the hard part.  Text editors do that
    // all the time; that is edit the same file (qsGraph) with many
    // views (layouts).
    //
    struct QsGraph *qsGraph;

    // gtk_layout widget.  We draw blocks and connections on this
    // widget.
    GtkWidget *layout;

    GtkWidget *tabLabel;

    // TODO: the connection stuff needs to be more complex to do
    // connection line selection, so we can click on a line to remove
    // it.
    //
    // The GUI Blocks will own this struct Connection memory.
    //
    // A doubly linked list of connections, with the end being
    // added to as we connect blocks ports.
    struct Connection *firstConnection, *lastConnection;

    // The top level window this layout is in.
    struct Window *window;

    // Values last set with gtk_layout_set_size(), and are not
    // necessarily the same as allocation width and height.
    guint w, h;

    int scrollH, scrollV;

    // The allocation the last time we looked; so we can see when it
    // changes.
    int width, height;

    cairo_surface_t *surface, *newSurface;

    bool surfaceNeedRedraw;
};


struct ConnectionArray {

    struct Connection **connections;
    uint32_t numConnections;
};


// Blocks have connection terminal area that can have many ports for
// connecting to.
struct Terminal {

    // Port type: In, Out, Set, Get.
    enum Type type;

    // location on block: Left, Right, Top, Bottom.
    enum Pos pos;

    struct Block *block;

    GtkWidget *drawingArea;

    // The size of the drawingArea as measured in the larger dimension.
    double width;

    bool needSurfaceRedraw;

    cairo_surface_t *surface;

    // Array of ports for this connection terminal.  This will be
    // allocated memory.  We consider the struct Block to own it.
    uint32_t numPorts;
    struct Port *ports; // 0 if numPorts is 0.
};


struct Block {

    struct Layout *layout;

    // The top GTK3 parent widget for the block.
    //
    // Happens to be an event_box, so that the grid can receive events.
    GtkWidget *parent;

    // The top level gtk widget for a block is a grid.
    GtkWidget *grid;

    GtkWidget *nameLabel;

    struct QsBlock *qsBlock;

    // The singly linked list of selected blocks.
    struct Block *nextSelected;
    // For the list of all blocks in the layout.
    struct Block *next;
    //
    bool isSelected;


    // orientation of the terminals: In, Out, Set, Get.
    uint8_t orientation;

    // There are always NumTypes types of terminal ports,
    // one for In, Out, Set, and Get.  Though they could be
    // no connectable ports in any of them.
    struct Terminal terminals[NumTypes];

    // In the position in GTK layout widget.
    gint x, y;
    // Used to measure motion.
    gint x0, y0;

    struct ConnectionArray cons;
};


struct Connection {

    // For the Layout doubly listed list of all connections.
    struct Connection *next, *prev;

    // Connect port1 to port2.
    //
    struct Port *port1, *port2;
};


// Ports that are displayed in the GUI, and only ports that are
// displayed in the GUI.
struct Port {

    // A pointer to the terminal struct that owns this port:
    struct Terminal *terminal;

    // The index to this port in the terminal struct.
    //
    // We need this to index into the drawing area widget to find this
    // port with the mouse pointer location.  All ports have a num from
    // the sequence 0,1,2,3, to and including numPort-1, for it's
    // terminal.
    //
    uint32_t num;


    struct ConnectionArray cons;


    struct QsPort *qsPort;

    // The name of the port.  If it's from a super block alias, it's
    // the alias name, and not the real QsPort name.
    //
    // For simple blocks it's the real QsPort name.
    char *name;

    // The quickstream graph stores port aliases that become super block
    // aliases when the graph is saved as a block.
    //
    // If set, graphPortAlias, is the name of a graph port alias in the
    // current graph we displaying in the GUI.
    //
    // graphPortAlias is strdup() allocated.
    //
    char *graphPortAlias;


    bool isRequired;
};


// There is just one of these, even if there are many layouts.
// It can be that way because there is only one pointer.
struct LayoutSelection {

    // The current selection box.
    gdouble x, y, width, height;

    // We are currently dragging the selection box.
    bool active; // There is a selection box being drawn.
};


struct QsBlock;


extern
struct LayoutSelection layoutSelection;

extern
void CleanupCSS(void);

extern
void AddCSS(void);


extern
void CreateWindow(const char *path);

extern
struct Layout *CreateTab(struct Window *w, const char *path);
extern
struct Layout *RecreateTab(struct Layout *old, struct Layout *l);
extern
void CloseTab(struct Layout *l);

// This is used by CreateTab()
extern
struct Layout *CreateLayout(struct Window *w, const char *path);


extern
void ShowButtonBar(struct Layout *l, bool doShow);
extern
void SetHaltButton(struct Layout *l, bool isHalted);
extern
void SetRunButton(struct Layout *l, bool isRunning);

// This can fail, but it does not do any thing if it does fail.
extern
void CreateBlock(struct Layout *l,
        const char *path, struct QsBlock *b,
        double x, double y, uint8_t orientation,
        int slFlag /*0 unselectAllButThis,
                   1 add this block to selected
                   2 no selection change */);


extern
void CreateTerminal(struct Terminal *t);

extern
void GetPortConnectionPoint(double *x, double *y,
        const struct Port *p);





// We do not use multi-press and funny button events.
//
// Return true if this is an event we do not use.
static inline bool NoGoodButtonPressEvent(GdkEventButton *e) {

    if(e->type != GDK_BUTTON_PRESS ||
            e->button == 4 || e->button == 5)
        return true;

    switch(e->button) {

        case 1: // left
            return (e->state & GDK_BUTTON2_MASK) ||
                    (e->state & GDK_BUTTON3_MASK);
        case 2: // middle
            return (e->state & GDK_BUTTON1_MASK) ||
                    (e->state & GDK_BUTTON3_MASK);
        case 3: // right
            return (e->state & GDK_BUTTON1_MASK) ||
                    (e->state & GDK_BUTTON2_MASK);
        default:
            // Okay we may use it.
            return false;
    }
    ASSERT(0, "WTF");
    return false;
}


extern
struct Block *movingBlocks;

extern
bool blocksMoved;

extern
gdouble x_root, y_root; // For moving the blocks.

// possible connection in progress from this port:
extern
struct Port *fromPort;
extern
double fromX, fromY;


extern
GtkWidget *setValuePopover;


extern
void AddConnection(struct Port *p1, struct Port *p2, bool qsConnect);





// Position is in drawing area widget coordinate space at the center of
// where we drew the port (pin) icon (arrow or golf tee).
static inline
double PortIndexToPosition(const struct Terminal *t, uint32_t portNum) {

    DASSERT(t);
    DASSERT(t->numPorts);
    DASSERT(portNum < t->numPorts);

    // This is the distance in the drawing area widget.

    return (portNum + 1)*t->width/(t->numPorts+1);
}


extern
char *GetBlockPath(GtkTreeView *treeView, GtkTreePath *tpath);
extern
void CreateTreeView(GtkContainer *sw, struct Window *w);
extern
void RecreateTreeView(struct Window *w);


extern
void CleanUpCursors(void);
extern
void SetWidgetCursor(GtkWidget *w, const char *name);

extern
void PopUpBlock(struct Block *block);

extern
void UnselectAllBlocks(struct Layout *l, struct Block *sb);
extern
void SelectBlock(struct Block *block);
extern
void UnselectBlock(struct Block *block);

extern
void ShowBlockPopupMenu(struct Block *b);

extern
void MakeConfigWidgets(struct Block *b, GtkWidget *grid);

extern
void ShowBlockConfigWindow(struct Block *b);

extern
void DrawLayoutNewSurface(struct Layout *l,
        double x, double y, enum Pos toPos);

extern
void DrawConnectionLineToSurface(cairo_t *cr,
        const struct Port *p1,
        const struct Port *p2);

extern
void HandleConnectingDragEvent(GdkEventMotion *e, struct Layout *l,
        double x, double y, enum Pos toPos);

void GetParameterPopoverText(const struct Port *p, char *text,
        size_t Len);


struct QsParameter;

extern
char *GetParameterValueString(struct QsParameter *p);

extern
void MoveTerminal(struct Terminal *t, enum Pos pos);

extern
void ShowTerminalPopupMenu(struct Terminal *t, struct Port *port);

extern
void Disconnect(struct Port *p);


extern
void ShowGraphTabPopupMenu(struct Layout *l);

extern
void ShowAssignBlocksToThreadsWindow(struct Layout *l);

extern
void ShowCreateThreadPools(struct Layout *l, GtkWindow *parent);

extern
GtkListStore *GetBlockListModel(struct Layout *l);

static inline uint8_t
GetOrientation(const struct Block *b) {

    DASSERT(b);
    const struct Terminal *t = b->terminals;

    // We define a block orientation by this expression; and so
    // SetOrientation() below can inverse this process.
    return t[In].pos +
        (t[Out].pos << 2) +
        (t[Set].pos << 4) +
        (t[Get].pos << 6);
}

extern
char *Splice2(const char *s1, const char *s2);


extern
struct Window *window;

extern
void SaveAs(struct Layout *layout);

extern
void Save(struct Layout *l);

///////////////////////////////////////////////////////////
//   Below here are functions that look into internal
//   data structures in libquickstream.so.
///////////////////////////////////////////////////////////


extern
bool CanConnect(const struct Port *fromPort,
        const struct Port *toPort);

extern
void CreatePorts(struct Terminal *t);

extern
bool BlockHasConfigAttrubutes(const struct Block *b);

extern
void MakeConfigWidgets(struct Block *b, GtkWidget *parent);

extern
bool IsSuperBlock(const struct Block *b);

extern
const char *GetThreadPoolName(const struct Block *b);

extern
GtkTreeModel *MakeThreadPoolListStore(const struct Layout *l);

extern
GtkTreeModel *MakeThreadPoolsEditStore(const struct Layout *l,
        uint32_t *numThreadPools);

extern
void DisplayBlocksAndConnections(struct Layout *l);

extern
const char *GetBlockFileName(struct Block *b);

extern
enum QsPortType GetQsPortType(const struct Port *p);


extern
void SetQsFeedback(struct Layout *l);

extern
void RemoveGraphPortAlias(struct Port *port);

extern
void MakeGraphPortAlias(struct Port *port, const char *name);
