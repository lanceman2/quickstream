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



static GtkTextView *status = 0;
static GdkPixbuf *inputIcon = 0;
static GdkPixbuf *outputIcon = 0;

// List of cursors with images:
// https://developer.gnome.org/gdk3/stable/gdk3-Cursors.html#GdkCursorType
static GdkCursor *moveCursor = 0;
static GdkCursor *grabCursor = 0;
static GdkCursor *configureBlockCursor = 0;

static GdkCursor *getHoverCursor = 0;
static GdkCursor *setHoverCursor = 0;
static GdkCursor *inputHoverCursor = 0;
static GdkCursor *outputHoverCursor = 0;

static GdkCursor *getActiveCursor = 0;
static GdkCursor *setActiveCursor = 0;
static GdkCursor *inputActiveCursor = 0;
static GdkCursor *outputActiveCursor = 0;


static GtkWidget *window = 0;


enum BlockMode {

    BM_NONE = 0,
    BM_HAVE_GET,
    BM_HAVE_SET,
    BM_HAVE_INPUT,
    BM_HAVE_OUTPUT,
    BM_MOVING
};


struct Block {
    GtkWidget *top;
    GtkLayout *layout;

    gint x,y; // position in layout

    enum BlockMode mode;
};


static inline void SetCursor(GdkCursor *cursor) {
    gdk_window_set_cursor(gtk_widget_get_window(window), cursor);
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
            "#block, #top {\n"
                "background-color: rgba(83,138,250,0.5);\n"
                "}\n"
            "#block {\n"
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


static gboolean
BlockButtonCB(GtkWidget *widget, GdkEvent *event, struct Block *b) {

    errno = 0;

    static gint x0 = -1;
    static gint y0 = -1;
    static struct Block *movingBlock = 0;
    GtkAllocation rect;

    GdkEventButton *e = (GdkEventButton *) event;

    switch(event->type) {

        case GDK_BUTTON_PRESS:

            SetCursor(moveCursor);

            gtk_widget_get_allocation(b->top, &rect);

            x0 = rect.x + e->x;
            y0 = rect.y + e->y;
            gtk_grab_add(widget);

            // This is how we pop this block to the front: Remove and
            // re-add it to the GtkLayout:
            gtk_container_remove(GTK_CONTAINER(b->layout), b->top);
            gtk_layout_put(b->layout, b->top, b->x, b->y);

            WARN("got button press at x=%d y=%d   widget pos=x,y=%d,%d  w,h=%d,%d",
                    x0, y0, rect.x, rect.y, rect.width, rect.height);

            b->mode = BM_MOVING;
            movingBlock = b;
            break;

        case GDK_MOTION_NOTIFY:
        {
            if(movingBlock != b) {
                movingBlock = 0;
                b->mode = BM_NONE;
                break;
            }

            WARN("x,y = %g %g", e->x, e->y);
            gtk_widget_get_allocation(b->top, &rect);


            b->x += rect.x + e->x - x0;
            b->y += rect.y + e->y - y0;

            x0 = rect.x + e->x;
            y0 = rect.y + e->y;

            gtk_layout_move(b->layout, b->top, b->x, b->y);
            break;
        }

        case GDK_BUTTON_RELEASE:
            gtk_grab_remove(widget);
            SetCursor(grabCursor);
            b->mode = BM_NONE;
            movingBlock = 0;
            break;

        default:
            break;
    }


    return TRUE; // TRUE = do not go to next child
}


static gboolean
ConnectButtonCB(GtkWidget *w, GdkEvent *event, struct Block *b) {

    errno = 0;
    
    DSPEW("block=%p", b);
    GdkEventButton *e = (GdkEventButton *) event;

    switch(event->type) {

        case GDK_BUTTON_PRESS:

            WARN("got button press at x=%lg y=%lg",
                    e->x, e->y);
            break;

        case GDK_BUTTON_RELEASE:
        {
            break;
        }

        default:
            break;
    }


    return TRUE; // TRUE = do not go to next child
}


static gboolean
BlockEnterCB(GtkWidget *w, GdkEvent *e, struct Block *b) {

    errno = 0;

    DSPEW("block=%p", b);

    switch(b->mode) {

        case BM_NONE:
            SetCursor(grabCursor);
            break;
        case BM_MOVING:
            SetCursor(moveCursor);
            break;
        default:
            break;
    }

    return TRUE; // TRUE = do not go to next child
}

static gboolean
BlockLeaveCB(GtkWidget *w, GdkEvent *e, struct Block *b) {

    errno = 0;

    DSPEW("block=%p", b);

    switch(b->mode) {

        case BM_NONE:
            SetCursor(0);
            break;
        case BM_MOVING:
            SetCursor(moveCursor);
            break;
        default:
            break;
    }

    return TRUE; // TRUE = do not go to next child
}



static gboolean
ConnectEnterCB(GtkWidget *w, GdkEvent *e, struct Block *b) {

    errno = 0;

    DSPEW("block=%p", b);

    return TRUE; // TRUE = do not go to next child
}


static void MakeBlockConnector(GtkWidget *grid,
        const char *type,
        const char *imageFile,
        gint x, gint y, gint w, gint h,
        struct Block *block) {

    GtkWidget *ebox = gtk_event_box_new();

    gtk_widget_set_can_focus(ebox, TRUE);
    gtk_widget_set_events(ebox,
                    GDK_BUTTON_RELEASE_MASK|
                    GDK_BUTTON_PRESS_MASK|
                    GDK_ENTER_NOTIFY_MASK|
                    GDK_LEAVE_NOTIFY_MASK
                    );
    g_signal_connect(ebox, "button-release-event",
            G_CALLBACK(ConnectButtonCB), block);
    g_signal_connect(ebox, "button-press-event",
            G_CALLBACK(ConnectButtonCB), block);
    g_signal_connect(ebox, "leave-notify-event",
            G_CALLBACK(ConnectEnterCB), block);
    g_signal_connect(ebox, "enter-notify-event",
            G_CALLBACK(ConnectEnterCB), block);
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

static void DestroyBlock(struct Block *block) {

    DASSERT(block);
    DASSERT(block->top);
    gtk_widget_destroy(block->top);
}

static struct Block *CreateBlock(GtkLayout *layout,
        const char *name,
        double x, double y) {

    struct Block *block = calloc(1, sizeof(*block));
    ASSERT(block, "calloc(1,%zu) failed", sizeof(*block));


    GtkWidget *grid = gtk_grid_new();
    GtkWidget *ebox = gtk_event_box_new();

    block->top = ebox;
    block->layout = layout;

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
                GDK_BUTTON_RELEASE_MASK|
                GDK_BUTTON_PRESS_MASK|
                GDK_POINTER_MOTION_MASK|
                GDK_ENTER_NOTIFY_MASK|
                GDK_LEAVE_NOTIFY_MASK
                );
    g_signal_connect(ebox, "button-release-event",
        G_CALLBACK(BlockButtonCB), block);
    g_signal_connect(ebox, "motion-notify-event",
        G_CALLBACK(BlockButtonCB), block);
    g_signal_connect(ebox, "button-press-event",
        G_CALLBACK(BlockButtonCB), block);
     g_signal_connect(ebox, "leave-notify-event",
        G_CALLBACK(BlockLeaveCB), block);
    g_signal_connect(ebox, "enter-notify-event",
        G_CALLBACK(BlockEnterCB), block);
    gtk_widget_set_name(ebox, "get");
    gtk_widget_show(ebox);
    gtk_container_add(GTK_CONTAINER(ebox), grid);
    block->x = x;
    block->y = y;
    gtk_layout_put(layout, ebox, block->x, block->y);



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

    MakeBlockConnector(grid, "input", "input.png", 0, 2, 1, 1, block);
    MakeBlockConnector(grid, "output", "output.png", 6, 2, 1, 1, block);
    MakeBlockConnector(grid, "set", "set.png", 3, 0, 1, 1, block);
    MakeBlockConnector(grid, "get", "get.png", 3, 4, 1, 1, block);

    gtk_widget_show(grid);

    return block;
}



static gboolean WorkAreaCB(GtkLayout *layout,
        GdkEvent *event, void *data) {

    DASSERT(moveCursor);
    DASSERT(window);

    static bool movingBlock = 0;
    static struct Block *newBlock = 0;
    static char *name = 0;
     errno = 0;
    GdkEventButton *e = (GdkEventButton *) event;

    switch(event->type) {

        case GDK_BUTTON_PRESS:

            if(e->button != BLOCK_CREATE_BUTTON) break;

            WARN("got button press at x=%lg y=%lg",
                    e->x, e->y);
            name = strdup("block Name larger block Name ya");
            newBlock = CreateBlock(layout, name, e->x, e->y);
            movingBlock = false;
            SetCursor(grabCursor);
            break;

        case GDK_MOTION_NOTIFY:

            if(!newBlock) break;

            if(!movingBlock) {
                movingBlock = true;
                SetCursor(moveCursor);
            }

            newBlock->x = e->x;
            newBlock->y = e->y;
            gtk_layout_move(layout, newBlock->top, newBlock->x, newBlock->y);

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
                DestroyBlock(newBlock);
            else
                WriteStatus("created block \"%s\"", name);

            newBlock = 0;
            movingBlock = false;
            SetCursor(0);
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

static inline GdkCursor *GetCursor(const char *type) {
    return gdk_cursor_new_from_name(gdk_display_get_default(), type);
}

static inline GdkCursor *GetCursor_(GdkCursorType type) {
    return gdk_cursor_new_for_display(gdk_display_get_default(), type);
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
    grabCursor = GetCursor("grab");
    configureBlockCursor = GetCursor("crosshair");


    getHoverCursor = GetCursor_(GDK_BOTTOM_SIDE);
    setHoverCursor = GetCursor_(GDK_TOP_SIDE);
    inputHoverCursor = GetCursor_(GDK_RIGHT_SIDE);
    outputHoverCursor = GetCursor_(GDK_LEFT_SIDE);

    getActiveCursor = GetCursor_(GDK_BOTTOM_TEE);
    setActiveCursor = GetCursor_(GDK_TOP_TEE);
    inputActiveCursor = GetCursor_(GDK_RIGHT_TEE);
    outputActiveCursor = GetCursor_(GDK_LEFT_TEE);

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
