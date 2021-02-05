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

#include "../lib/debug.h"

#include "qsb.h"




// The top GTK widget of a block is a gtk_event_box.
//
// Return true if the block is successfully added, else return false.
//
// layout - is the widget we add the block to.
//
// The name of the block can be changed later.
//
bool AddBlock(GtkLayout *layout, const char *blockFile,
    double x, double y) {

      DSPEW("Trying to add block \"%s\"", blockFile);


  


      return true;
}
