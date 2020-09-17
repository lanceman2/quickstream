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


#define BLOCK_CREATE_BUTTON   (1)
#define BLOCK_CONNECT_BUTTON  (1)
#define BLOCK_MOVE_BUTTON  (1)


static GtkTextView *status = 0;
static GdkPixbuf *inputIcon = 0;
static GdkPixbuf *outputIcon = 0;

// Changing cursor is not well supported in GTK+3 (as of version 3.24.20):
//   1. CSS setting of cursor does not work at all.
//   2. Setting the cursor with  gdk_window_set_cursor() for a widget
//      sets it for all it's children, so that makes it difficult to
//      manage cursors.  There appears to be no stack of cursors,
//      which is the obvious way to manage changing cursors in code.

// List of cursors with images:
// https://developer.gnome.org/gdk3/stable/gdk3-Cursors.html#GdkCursorType
static GdkCursor *moveCursor = 0;

static GdkCursor *getCursor = 0;
static GdkCursor *inputCursor = 0;


static GtkWidget *window = 0;

struct Block;

enum ConnectionType {

    CT_GET,
    CT_SET,
    CT_INPUT,
    CT_OUTPUT
};

struct ConnectionPoint {

    struct Block *block;
    enum ConnectionType type;
    bool gotPress;
};


struct Block {
    GtkWidget *container;
    GtkLayout *layout;

    // This provides a struct that we can send to the connection
    // widget callbacks, without requiring another allocation.
    struct ConnectionPoint get, set, input, output;

    gint x, y; // current position in layout from layout
};



static inline void SetCursor(GtkWidget *w, GdkCursor *cursor) {
    gdk_window_set_cursor(gtk_widget_get_window(w), cursor);
}


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
            "#block {\n"
                "background-color: rgba(83,138,250,0.5);\n"
                "border: 1px solid black;\n"
                "}\n"
            "#block > #get, #set, #input, #output {\n"
                "background-color: rgba(147,176,223,0.5);\n"
                "border: 1px solid rgb(118,162,247);\n"
                "color: black;\n"
                "font-size: 130%;\n"
                "}\n"
            "#block > #get:hover {\n"
                "background-color: rgba(157,186,243,0.8);\n"
                "border: 1px solid rgb(100,142,207);\n"
            "}\n"
             "#block > #label {\n"
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

static void PopForward(GtkLayout *layout, struct Block *b) {

    // 1. get one more reference to the widget; otherwise
    //    the widget will be destroyed in the next call.
    g_object_ref(G_OBJECT(b->container
                ));
    // 2. remove the widget which removes one reference
    gtk_container_remove(GTK_CONTAINER(layout), b->container);
    // Now we have at least one reference.
    // 3. re-add the widget which will take ownership of the
    //    one reference.
    gtk_layout_put(layout, b->container, b->x, b->y);
}


static gboolean
BlockButtonCB(GtkWidget *widget, GdkEventButton *e, struct Block *b) {

    errno = 0;
    static bool gotPress = false;
    static gint xOffset = 0, yOffset = 0;


    switch(e->type) {
    
        case GDK_BUTTON_PRESS:
        {
            if(e->button != BLOCK_MOVE_BUTTON)
                break;

            SetCursor(GTK_WIDGET(b->layout), moveCursor);
            xOffset = e->x_root;
            yOffset = e->y_root;
            PopForward(b->layout, b);
            gotPress = true;
            gtk_grab_add(widget);
            break;
        }

        case GDK_MOTION_NOTIFY:

            // We get e->x and e->y that are relative to the widget where
            // this event first was received by a callback and it may not
            // be this Layout widget; so we must use the x_root and y_root
            // position values, which are positions relative to root
            // (whatever the hell that is).
                
            if(!gotPress) break;

            b->x += ((gint) e->x_root) - xOffset;
            b->y += ((gint) e->y_root) - yOffset;

            xOffset = e->x_root;
            yOffset = e->y_root;
            gtk_layout_move(b->layout, b->container, b->x, b->y);
            break;

        case GDK_BUTTON_RELEASE:
        {
            SetCursor(GTK_WIDGET(b->layout), 0);

            if(e->button != BLOCK_MOVE_BUTTON) break;

            gtk_grab_remove(widget);

            gotPress = false;

            break;
        }

        default:
            break;
    }

    return TRUE; // FALSE = go to next widget
}

#if 0
static gboolean
BlockEnterCB(GtkWidget *w, GdkEvent *e, struct Block *b) {

    errno = 0;

    //DSPEW();
    if(movingBlock) return FALSE;

    return TRUE; // TRUE = do not go to next widget
}

static gboolean
BlockLeaveCB(GtkWidget *w, GdkEvent *e, struct Block *b) {

    errno = 0;

    //DSPEW();
    if(movingBlock) return FALSE;

    return TRUE; // TRUE = do not go to next widget
}
#endif


static gint connectX0 = 0;
static gint connectY0 = 0;
static gint connectX = 0;
static gint connectY = 0;



static gboolean
ConnectPressCB(GtkWidget *w, GdkEventButton *e, struct ConnectionPoint *p) {

    errno = 0;

    DSPEW("type=%s", gtk_widget_get_name(w));

    DASSERT(e->type == GDK_BUTTON_PRESS);

    DSPEW("                                       Press=%d", p->type);

    connectX = connectX0 = e->x_root;
    connectY = connectY0 = e->y_root;

    DASSERT(p->gotPress == false);

    p->gotPress = true;

    return TRUE; // TRUE = do not go to next widget
}


static gboolean
ConnectMotionCB(GtkWidget *w, GdkEventButton *e, struct ConnectionPoint *p) {


    gint x = e->x_root;
    gint y = e->y_root;

    if(p->gotPress) {
        DSPEW(" FROM (%d,%d) TO (%d, %d)", connectX, connectY, x, y);
    }

    connectX = (gint) e->x_root;
    connectY = (gint) e->y_root;

    return TRUE; // TRUE = do not go to next widget
}


static gboolean
ConnectReleaseCB(GtkWidget *w, GdkEventButton *e, struct ConnectionPoint *p) {

    errno = 0;

    DASSERT(e->type == GDK_BUTTON_RELEASE);
    
    DSPEW("                                       Release=%d", p->type);

    if(p->gotPress) {

        DSPEW(" FROM (%d,%d) TO (%d, %d)", connectX0, connectY0, connectX, connectY);

        p->gotPress = false;
    }

    return TRUE; // TRUE = do not go to next widget
}

static gboolean
ConnectEnterCB(GtkWidget *w, GdkEvent *e, struct ConnectionPoint *p) {

    errno = 0;
    DASSERT(GDK_ENTER_NOTIFY == e->type);

    DSPEW("                                 ENTER=%d", p->type);

    return TRUE; // TRUE = do not go to next widget
}

static gboolean
ConnectLeaveCB(GtkWidget *w, GdkEvent *e, struct ConnectionPoint *p) {

    errno = 0;
    DASSERT(GDK_LEAVE_NOTIFY == e->type);

    DSPEW("                                LEAVE=%d", p->type);

    return TRUE; // TRUE = do not go to next widget
}


static void MakeBlockConnector(GtkWidget *grid,
        const char *type,
        const char *imageFile,
        gint x, gint y, gint w, gint h,
        struct Block *block,
        struct ConnectionPoint *p
        ) {

    GtkWidget *ebox = gtk_event_box_new();

    gtk_widget_set_can_focus(ebox, TRUE);
    gtk_widget_set_events(ebox,
            GDK_STRUCTURE_MASK |
            GDK_SCROLL_MASK |
            GDK_POINTER_MOTION_MASK |
            GDK_BUTTON_RELEASE_MASK |
            GDK_BUTTON_PRESS_MASK|
            GDK_STRUCTURE_MASK);

    g_signal_connect(ebox, "button-press-event",
            G_CALLBACK(ConnectPressCB), p);
    g_signal_connect(ebox, "motion-notify-event",
            G_CALLBACK(ConnectMotionCB), p);
    g_signal_connect(ebox, "button-release-event",
            G_CALLBACK(ConnectReleaseCB), p);
    g_signal_connect(ebox, "enter-notify-event",
            G_CALLBACK(ConnectEnterCB), p);
    g_signal_connect(ebox, "leave-notify-event",
            G_CALLBACK(ConnectLeaveCB), p);

    gtk_widget_set_name(ebox, type);
    gtk_widget_show(ebox);
    gtk_grid_attach(GTK_GRID(grid), ebox, x, y, w, h);

    GtkWidget *img = gtk_image_new_from_file(imageFile);
    gtk_container_add(GTK_CONTAINER(ebox), img);
    gtk_widget_set_name(img, type);
    gtk_widget_show(img);
}

static void MakeBlockLabel(GtkWidget *grid,
        const char *text,
        gint x, gint y, gint w, gint h) {

    GtkWidget *l = gtk_label_new(text);
    gtk_widget_set_name(l, "label");
    gtk_widget_show(l);
    gtk_grid_attach(GTK_GRID(grid), l, x, y, w, h);
}

#if 0
static void DestroyBlock(struct Block *block) {

    DASSERT(block);
    DASSERT(block->container);
    gtk_widget_destroy(block->container);
}
#endif

static struct Block *CreateBlock(GtkLayout *layout,
        const char *name,
        double x, double y) {

    struct Block *block = calloc(1, sizeof(*block));
    ASSERT(block, "calloc(1,%zu) failed", sizeof(*block));
    block->get.type = CT_GET;
    block->get.block = block;
    block->set.type = CT_SET;
    block->set.block = block;
    block->input.type = CT_INPUT;
    block->input.block = block;
    block->output.type = CT_OUTPUT;
    block->output.block = block;


    GtkWidget *grid = gtk_grid_new();
    GtkWidget *ebox = gtk_event_box_new();

    block->container = ebox;
    block->layout = layout;
    block->x = x;
    block->y = y;

    // As of GTK3 version 3.24.20; gtk widget name is more like a CSS
    // class name.  Name is not a unique ID.  It's more like a CSS
    // class.
    gtk_widget_set_name(grid, "block");
    gtk_widget_set_visible(grid, TRUE);

    // Sets the minimum size of grid.
    gtk_widget_set_size_request(grid, -1, -1);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 0);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 0);
    gtk_grid_set_column_homogeneous(GTK_GRID(grid), FALSE);
    gtk_grid_set_row_homogeneous(GTK_GRID(grid), FALSE);

    gtk_widget_set_can_focus(ebox, TRUE);
    gtk_widget_set_events(ebox,
                GDK_BUTTON_PRESS_MASK|
                GDK_BUTTON_RELEASE_MASK|
                GDK_POINTER_MOTION_MASK|
                GDK_ENTER_NOTIFY_MASK|
                GDK_LEAVE_NOTIFY_MASK
                );

    g_signal_connect(ebox, "button-press-event",
        G_CALLBACK(BlockButtonCB), block);
    g_signal_connect(ebox, "button-release-event",
        G_CALLBACK(BlockButtonCB), block);
    g_signal_connect(ebox, "motion-notify-event",
        G_CALLBACK(BlockButtonCB), block);
#if 0
     g_signal_connect(ebox, "leave-notify-event",
        G_CALLBACK(BlockLeaveCB), block);
    g_signal_connect(ebox, "enter-notify-event",
        G_CALLBACK(BlockEnterCB), block);
#endif
    gtk_widget_set_name(ebox, "get");
    gtk_widget_show(ebox);
    gtk_container_add(GTK_CONTAINER(ebox), grid);
    gtk_layout_put(layout, ebox, x, y);


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

    MakeBlockLabel(grid, name, 2, 1, 3, 1);
    MakeBlockLabel(grid, "block type 923rj", 2, 2, 3, 1);
    MakeBlockLabel(grid, "Boston", 2, 3, 3, 1);

    MakeBlockConnector(grid, "input", "input.png", 0, 2, 1, 1, block, &block->input);
    MakeBlockConnector(grid, "output", "output.png", 6, 2, 1, 1, block, &block->output);
    MakeBlockConnector(grid, "set", "set.png", 3, 0, 1, 1, block, &block->set);
    MakeBlockConnector(grid, "get", "get.png", 3, 4, 1, 1, block, &block->get);

    gtk_widget_show(grid);

    return block;
}


static gboolean WorkAreaCB(GtkLayout *layout,
        GdkEventButton *e, void *data) {

    DASSERT(moveCursor);
    DASSERT(window);

    static gint xOffset = 0, yOffset = 0;
    static struct Block *movingBlock = 0;
    errno = 0;

    switch(e->type) {

        case GDK_BUTTON_PRESS:
        {
            if(e->button != BLOCK_CREATE_BUTTON) break;

            // Create a new block and then move it.
            char *name = strdup("block Name larger block Name ya");
            movingBlock = CreateBlock(layout, name, e->x, e->y);

            xOffset = e->x_root;
            yOffset = e->y_root;

            SetCursor(GTK_WIDGET(layout), moveCursor);
            WriteStatus("created block \"%s\"", name);
            free(name);
            break;
        }

        case GDK_MOTION_NOTIFY:

            // We get e->x and e->y that are relative to the widget where
            // this event first was received by a callback and it may not
            // be this Layout widget; so we must use the x_root and y_root
            // position values, which are positions relative to root
            // (whatever the hell that is).

            if(!movingBlock) break;

            movingBlock->x += ((gint) e->x_root) - xOffset;
            movingBlock->y += ((gint) e->y_root) - yOffset;

            xOffset = e->x_root;
            yOffset = e->y_root;

            gtk_layout_move(layout, movingBlock->container,
                    movingBlock->x, movingBlock->y);
            break;

        case GDK_BUTTON_RELEASE:

            SetCursor(GTK_WIDGET(layout), 0);

            if(e->button != BLOCK_CREATE_BUTTON) break;

            movingBlock = 0;
            break;

        default:
            break;
    }

    return FALSE; // FALSE = go to next widget
}


static inline void
Connect(GtkBuilder *builder, const char *id, const char *action,
        void *callback, void *userData) {
    g_signal_connect(gtk_builder_get_object(builder, id),
            action, G_CALLBACK(callback), userData);
}

static inline GdkCursor *GetCursor(const char *type) {
    return gdk_cursor_new_from_name(gdk_display_get_default(), type);
}

static inline GdkCursor *GetCursor_(GdkCursorType type) {
    return gdk_cursor_new_for_display(gdk_display_get_default(), type);
}

static gboolean MapWorkAreaCB(GtkWidget *w, GdkEvent *e, void *d) {

    DASSERT(e->type == GDK_MAP);
    return FALSE;
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


    ///////////////////////////////////////////////////////////////////
    //    Here is where cursors are configured
    //////////////////////////////////////////////////////////////////

    moveCursor = GetCursor("grabbing");

    getCursor = GetCursor_(GDK_BOTTOM_SIDE);
    inputCursor = GetCursor_(GDK_RIGHT_SIDE);


    //////////////////////////////////////////////////////////////////


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
    Connect(b, "workArea", "button-release-event", WorkAreaCB, (void *) 1);
    Connect(b, "workArea", "button-press-event", WorkAreaCB, (void *) 2);
    Connect(b, "workArea", "motion-notify-event", WorkAreaCB, (void *) 3);
    Connect(b, "workArea", "map-event", MapWorkAreaCB, 0);
}


void
catcher(int sig) {

    fprintf(stderr, "Caught signal %d\n"
        "\n"
        "  Consider running:  gdb -pid %u\n\n",
        sig, getpid());

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
