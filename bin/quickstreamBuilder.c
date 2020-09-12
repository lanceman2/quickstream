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
                "border: 10px solid black;\n"
                "}\n"
            "#block {\n"
                "background-color: rgba(83,138,250,0.5);\n"
                "text-shadow: 1px 1px 5px black;\n"
                "box-shadow: 0px 0px 5px black;\n"
                "border: 1px solid black;\n"
                "}\n"
            "#block > #label {\n"
                "background-color: rgba(255,0,10,0.5);\n"
                "border: 1px solid black;\n"
                "color: white;\n"
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
    gtk_widget_set_size_request(grid, 150, 90);

    {
        GtkWidget *label = gtk_label_new(name);
        gtk_widget_set_name(label, "label");
        gtk_widget_show(label);
        gtk_grid_attach(GTK_GRID(grid), label, 20, 0, 20, 20);

        label = gtk_label_new("");
        gtk_widget_set_name(label, "label");
        gtk_widget_show(label);
        gtk_grid_attach(GTK_GRID(grid), label, 20, 100, 20, 20);


        GtkWidget *button = gtk_button_new_with_label("BUTTON");
        gtk_widget_set_name(button, "bar");
        gtk_widget_show(button);
        gtk_grid_attach(GTK_GRID(grid), button, 10, 40, 20, 20);
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
            name = strdup("block Name");
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
