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
setup_widget_connections(void) {
    GtkBuilder *builder = gtk_builder_new_from_resource(
          "/quickstream/quickstreamBuilder.ui");

    /* Connect signal handlers to the constructed widgets. */
    g_signal_connect(gtk_builder_get_object(builder, "window"),
            "destroy", G_CALLBACK(gtk_main_quit), NULL);

    g_signal_connect(gtk_builder_get_object(builder, "quitMenu"),
            "activate", G_CALLBACK(gtk_main_quit), NULL);

    GObject *button = gtk_builder_get_object(builder, "button1");
    g_signal_connect(button, "clicked", G_CALLBACK(print_hello), NULL);

    button = gtk_builder_get_object(builder, "button2");
    g_signal_connect(button, "clicked", G_CALLBACK(print_hello), NULL);

    button = gtk_builder_get_object(builder, "quitButton");
    g_signal_connect(button, "clicked", G_CALLBACK(gtk_main_quit), NULL);

    g_signal_connect(gtk_builder_get_object(builder, "workArea"), "draw",
            G_CALLBACK(draw_callback), NULL);
}


int main(int argc, char *argv[]) {

    gtk_init(&argc, &argv);

    setup_widget_connections();

    gtk_main();

    DSPEW();
    return 0;
}
