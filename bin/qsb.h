/* quickstreamBuilder (qsb) is a GUI program that knows a little about
 * quickstream.   quickstreamBuilder builds lists of blocks and input,
 * output, and parameter connections between them.
 */
struct Block;


struct Page {
   GTree *selectedBlocks;

   struct Block *blocks; // singly linked list of blocks.

   // For drawing new lines or selecting box.
   cairo_surface_t *newDrawSurface;

   // width and height of layout drawing area.
   gint w, h;
};


enum ConnectorType {
    Input,
    Constant,
    Output,
    Getter,
    Setter
};


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


struct Connector {
    enum ConnectorType type;
    const char *name;
    GtkWidget *widget;
    struct Block *block;
};


struct Block {

    GtkWidget *ebox; // block container widget.
    GtkWidget *grid; // block ebox child container widget.
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


extern
struct Block *movingBlock;



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


// This is uses in gtk_widget_set_size_request() for some of the block
// widgets.
#define MIN_BLOCK_LEN     (20)



#define CREATE_BLOCK_BUTTON   (1) // 1 = left mouse
#define MOVE_BLOCK_BUTTON     (1) // 1 = left mouse

#define BLOCK_POPUP_BUTTON    (3) // 3 = right mouse
