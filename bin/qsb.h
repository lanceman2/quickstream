/* quickstreamBuilder (qsb) is a GUI program that knows a little about
 * quickstream.   quickstreamBuilder builds lists of blocks and input,
 * output, and parameter connections between them.
 */


extern
struct QsGraph *graph;


struct Block {

  struct QsBlock *block;

};


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


// Return true if the block is successfully added, else return false.
extern
bool AddBlock(GtkLayout *layout, const char *blockFile,
    double x, double y);
