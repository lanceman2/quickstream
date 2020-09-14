/* from: file:///usr/share/gtk-doc/html/gtk3/ch01s03.html
 */

#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdbool.h>

#include <gtk/gtk.h>
#include <gdk/gdkevents.h>

#include <libxml2/libxml/parser.h>
#include <libxml2/libxml/tree.h>

#include "../lib/debug.h"


#define BLOCK_CREATE_BUTTON  (1)


GtkTextView *status = 0;
GdkPixbuf *inputIcon = 0;
GdkPixbuf *outputIcon = 0;

void
WriteStatus(const char *fmt, ...) {
    const size_t len = 1024;
    int size = 0;
    char buf[len];
    va_list ap;
    va_start(ap, fmt);
    size = vsnprintf(buf, len, fmt, ap);
    va_end(ap);
    ASSERT(size > 0);

    DSPEW("STATUS append: %s", buf);
}


static gboolean
draw_callback(GtkWidget *widget, cairo_t *cr, gpointer data)
{
  
    guint width, height;
    GtkStyleContext *context;

    context = gtk_widget_get_style_context(widget);

    width = gtk_widget_get_allocated_width(widget);
    height = gtk_widget_get_allocated_height(widget);

    gtk_render_background(context, cr, 0, 0, width, height);

    cairo_set_source_rgb (cr, 224/255.0, 233/255.0, 244/255.0);
    cairo_paint(cr);

    return FALSE;
}


static void CSS() {

    GtkCssProvider *provider;

    provider = gtk_css_provider_new();
    gtk_style_context_add_provider_for_screen(
            gdk_display_get_default_screen(gdk_display_get_default()),
            GTK_STYLE_PROVIDER(provider),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    GError *error = 0;

    gtk_css_provider_load_from_data(provider,
        /* The CSS stuff in GTK3 version 3.24.20 does not work as
         * documented.  All the example codes that I found did not work.
         * I expect this is a moving target, and this will brake in the
         * future.
         */
            // This CSS works for GTK3 version 3.24.20
            //
            // #block > #foo
            //    is for child of parent named "block" with name "foo"
            //
            // Clearly widget name is not a unique widget ID; it's more
            // like a CSS class name then a name.  The documentation is
            // miss-leading on this.  This is not consistent with regular
            // www3 CSS.  We can probably count on this braking in new
            // versions of GTK3.
            //
            // This took 3 days of trial and error.  Yes: GTK3 sucks;
            // maybe a little less than QT.
            //
            // TODO: It'd be nice to put this in gtk builder files,
            // and compiling it into the code like the .ui widgets.
            // There currently does not appear to be a way to do this.
            "#vpanel {\n"
                "}\n"
            "#block, #top {\n"
                "background-color: rgba(83,138,250,0.5);\n"
                "}\n"
            "#block {\n"
                "box-shadow: 0px 0px 5px black;\n"
                "border: 1px solid black;\n"
                "}\n"
            "#block > #label {\n"
                "background-color: rgba(83,138,250,0.0);\n"
                "border: 8px solid rgba(0,0,0,0.0);\n"
                "color: black;\n"
                "font-size: 130%;\n"
                "}\n"
            "#block > #bar:hover {\n"
                "color: red;\n"
                "background-color: rgba(255,90,10,0.5);\n"
                "}\n"
            , -1, &error);

    g_object_unref (provider);
}


static GtkWidget *CreateBlock(GtkLayout *layout,
        const char *name,
        double x, double y) {

    GtkWidget *grid = gtk_grid_new();
    // As of GTK3 version 3.24.20; gtk widget name is more like a CSS
    // class name.  Name is not a unique ID.  It's more like a CSS
    // class.
    gtk_widget_set_name(grid, "block");
    gtk_layout_put(layout, grid, x, y);
    gtk_widget_set_visible(grid, TRUE);

    // Sets the minimum size of grid.
    gtk_widget_set_size_request(grid, -1, -1);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 0);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 0);
    gtk_grid_set_column_homogeneous(GTK_GRID(grid), FALSE);
    gtk_grid_set_row_homogeneous(GTK_GRID(grid), TRUE);
    // The Grid of a block
    //
    // 0,0  1,0  2,0  3,0  4,0  5,0  6,0
    // 
    // 0,1  1,1  2,1  3,1  4,1  5,1  6,1
    //
    // 0,2  1,2  2,2  3,2  4,2  5,2  6,2
    //
    // 0,3  1,3  2,3  3,3  4,3  5,3  6,3
    //
    // 0,4  1,4  2,4  3,4  4,4  5,4  6,4
    {
        GtkWidget *w = gtk_label_new(name);
        gtk_widget_set_name(w, "label");
        gtk_widget_show(w);
        gtk_grid_attach(GTK_GRID(grid), w, 2, 1, 3, 1);

        w = gtk_label_new("block type 923rj wefj    23r0923j r");
        gtk_widget_set_name(w, "label");
        gtk_widget_show(w);
        gtk_grid_attach(GTK_GRID(grid), w, 2, 2, 3, 1);

        w = gtk_label_new("boston");
        gtk_widget_set_name(w, "label");
        gtk_widget_show(w);
        gtk_grid_attach(GTK_GRID(grid), w, 2, 3, 3, 1);

 
        w = gtk_image_new_from_file("input.png");
        gtk_widget_set_name(w, "input");
        gtk_widget_show(w);
        gtk_grid_attach(GTK_GRID(grid), w, 0, 2, 1, 1);

        w = gtk_image_new_from_file("output.png");
        gtk_widget_set_name(w, "output");
        gtk_widget_show(w);
        gtk_grid_attach(GTK_GRID(grid), w, 6, 2, 1, 1);

        w = gtk_image_new_from_file("set.png");
        gtk_widget_set_name(w, "set");
        gtk_widget_show(w);
        gtk_grid_attach(GTK_GRID(grid), w, 3, 0, 1, 1);

        w = gtk_image_new_from_file("get.png");
        gtk_widget_set_name(w, "get");
        gtk_widget_show(w);
        gtk_grid_attach(GTK_GRID(grid), w, 3, 4, 1, 1);
     }

    gtk_widget_show(grid);

    return grid;
}


static GdkCursor *moveCursor = 0;
static GdkCursor *crosshairCursor = 0;


static GtkWidget *window = 0;


static gboolean WorkAreaCB(GtkLayout *layout,
        GdkEvent *event, gpointer data) {

    DASSERT(moveCursor);
    DASSERT(window);

    static bool movingBlock = 0;
    static GtkWidget *newBlock = 0;
    static char *name = 0;
     errno = 0;
    GdkEventButton *e = (GdkEventButton *) event;

    switch(event->type) {

        case GDK_BUTTON_PRESS:

            if(e->button != BLOCK_CREATE_BUTTON) break;

            WARN("got button press at x=%lg y=%lg",
                    e->x, e->y);
            name = strdup("block Name larger block Name ya");
            newBlock = CreateBlock(layout,
                    name, e->x, e->y);
            movingBlock = false;
            gdk_window_set_cursor(
                    gtk_widget_get_window(window),
                    crosshairCursor);
            break;

        case GDK_MOTION_NOTIFY:

            if(!newBlock) break;

            if(!movingBlock) {
                movingBlock = true;
                gdk_window_set_cursor(
                    gtk_widget_get_window(window),
                    moveCursor);
            }

            gtk_layout_move(layout, newBlock, e->x, e->y);

            break;

        case GDK_BUTTON_RELEASE:
        {
            if(!newBlock) break;

            DASSERT(name);

            gint width = gtk_widget_get_allocated_width(GTK_WIDGET(layout));
            gint height = gtk_widget_get_allocated_height(GTK_WIDGET(layout));

            gint x = e->x;
            gint y = e->y;

            if(0 > x || x >= width || 0 > y || y >= height)
                // Remove the new block if it was dragged out of the
                // current displayed layout widget.
                gtk_widget_destroy(newBlock);
            else
                WriteStatus("created block \"%s\"", name);

            newBlock = 0;
            movingBlock = false;
            gdk_window_set_cursor(gtk_widget_get_window(window), 0);
            free(name);
            break;
        }

        default:
            break;
    }


    return FALSE;
}


static inline void
Connect(GtkBuilder *builder, const char *id, const char *action,
        void *callback, void *userData) {
    g_signal_connect(gtk_builder_get_object(builder, id),
            action, G_CALLBACK(callback), userData);
}


static inline void
setup_widget_connections(void) {

    // From the XML files: quickstreamBuilder.gresource.xml and
    // quickstreamBuilder.ui, a gObject compiler, named
    // glib-compile-resources, builds quickstreamBuilder_resources.c.
    // quickstreamBuilder_resources.c is compiled into an object and the
    // object is linked with this file.  The call to
    // gtk_builder_new_from_resource() calls some generated functions from
    // quickstreamBuilder_resources.c to get the builder struct that has
    // lots of widgets in it which we connect action callbacks to by id;
    // as in for example: <object class="GtkMenuItem" id="quitMenu">.

    GtkBuilder *b = gtk_builder_new_from_resource(
            "/quickstream/quickstreamBuilder.ui");



    moveCursor = gdk_cursor_new_for_display(gdk_display_get_default(), GDK_FLEUR);
    crosshairCursor = gdk_cursor_new_for_display(gdk_display_get_default(), GDK_CROSSHAIR);
    window = GTK_WIDGET(gtk_builder_get_object(b, "window"));

    GtkIconTheme *icon_theme = gtk_icon_theme_get_default();
    GError *error = 0;
    inputIcon = gtk_icon_theme_load_icon(icon_theme,
                                   "list-add", // icon name
                                   32, // icon size
                                   0,  // flags
                                   &error);
    DASSERT(inputIcon);
    outputIcon = gtk_icon_theme_load_icon(icon_theme,
                                   "list-remove", // icon name
                                   32, // icon size
                                   0,  // flags
                                   &error);
    DASSERT(outputIcon);




    status = GTK_TEXT_VIEW(gtk_builder_get_object(b, "status"));

    {
        GdkGeometry geo;

        geo.min_width = 300;
        geo.min_height = 300;

        ASSERT(window);
        gtk_window_set_geometry_hints(GTK_WINDOW(window), 0, &geo, GDK_HINT_MIN_SIZE);

        // TODO: add the window size setting to user preferences
        // just before exiting the app.
        gtk_window_resize(GTK_WINDOW(window), 1000, 1000);
    }

    Connect(b, "window", "destroy", gtk_main_quit, 0);
    Connect(b, "quitMenu", "activate", gtk_main_quit, 0);
    Connect(b, "quitButton", "clicked", gtk_main_quit, 0);
    Connect(b, "workArea", "draw", draw_callback, 0);
    Connect(b, "workArea", "button-release-event", WorkAreaCB, (void *) 0x01);
    Connect(b, "workArea", "button-press-event", WorkAreaCB, (void *) 0x02);
    Connect(b, "workArea", "motion-notify-event", WorkAreaCB, (void *) 0x03);
}

void
catcher(int sig) {

    fprintf(stderr, "Caught signal %d\n"
        "\n"
        "  Consider running:  gdb -pid %u\n\n",
        getpid(), sig);

    ASSERT(0);
}


int main(int argc, char *argv[]) {

    signal(SIGSEGV, catcher);
    signal(SIGABRT, catcher);

    gtk_init(&argc, &argv);

    CSS();

    setup_widget_connections();

    gtk_main();

    DSPEW("xmlReadFile=%p", xmlReadFile);
    return 0;
}
