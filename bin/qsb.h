/* quickstreamBuilder (qsb) is a GUI program that knows a little about
 * quickstream.   quickstreamBuilder builds lists of blocks and input,
 * output, and parameter connections between them.
 */


// This adds a Gtk tree view to use as the block selecting thingy.
//
// tree is a passed in and is empty to start with.
//
extern
void AddBlockSelector(GtkWidget *tree);


extern
void CleanupBlockSelector(void);
