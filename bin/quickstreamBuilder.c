/* from: file:///usr/share/gtk-doc/html/gtk3/ch01s03.html
 */

#include <stdio.h>
#include <gtk/gtk.h>

#include "../lib/debug.h"


static void
print_hello(GtkWidget *widget, gpointer data)
{
  fprintf(stderr, "Hello World\n");
}


static gboolean
draw_callback(GtkWidget *widget, cairo_t *cr, gpointer data)
{
  
    guint width, height;
    GdkRGBA color;
    GtkStyleContext *context;

    context = gtk_widget_get_style_context(widget);

    width = gtk_widget_get_allocated_width(widget);
    height = gtk_widget_get_allocated_height(widget);

    gtk_render_background(context, cr, 0, 0, width, height);

    cairo_arc(cr,
             width / 2.0, height / 2.0,
             MIN(width, height) / 2.0,
             0, 2 * G_PI);

    gtk_style_context_get_color(context,
            gtk_style_context_get_state(context),
            &color);
    gdk_cairo_set_source_rgba(cr, &color);

    cairo_fill(cr);

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

    Connect(b, "window", "destroy", gtk_main_quit, 0);
    Connect(b, "quitMenu", "activate", gtk_main_quit, 0);
    Connect(b, "button1", "clicked", print_hello, 0);
    Connect(b, "button2", "clicked", print_hello, 0);
    Connect(b, "quitButton", "clicked", gtk_main_quit, 0);
    Connect(b, "workArea", "draw", draw_callback, 0);
}


int main(int argc, char *argv[]) {

    gtk_init(&argc, &argv);

    setup_widget_connections();

    gtk_main();

    DSPEW();
    return 0;
}
