
// "Widget Types" that we define in this directory.
enum Type {

    Text,
    Slider,
    Button
};


// We limit widget label sizes.
#define LABEL_SIZE  (133)

#define MEM_NAME  "qsGTK3"


// We will get at most one of these per QsGraph.  So there can be many per
// process if there is more than one graph.
//
struct Window {

    // We cannot directly call any of the GTK3 API in this file; because
    // we did not and cannot build-time link this code with the GTK3
    // libraries.  Doing so would render the blocks into monster sized
    // blocks at graph build/edit time.  We dlopen(3) loaded (_run.so)
    // wrapper code that does build-time link with the GTK3 libraries via
    // ../../../libqsGtk_init.so (which is build-time linked with the GTK3
    // libraries).
    //
    // Since we must run-time load GTK3 libraries, we load (dlsym(3))
    // these functions from _run.so:
    //

    void *dlhandle;

    void (*window_init)(struct Window *win, bool needLock);
    void (*window_reinit)(struct Window *win, bool needLock);
    void (*show)(struct Window *win, bool needLock);
    void (*hide)(struct Window *win, bool needLock);
    void (*window_cleanup)(struct Window *win, bool needLock);
    void (*window_setTitle)(struct Window *win);

    // List of widgets.  For now, just a linear list.
    struct Widget *first, *last;

    // Used by _run.so
    // A top level GTK window widget. 
    GtkWidget *window;
    GtkWidget *vbox;

    // mutex to protect the data pointed to by this inter-thread shared
    // struct.  In _run.so (_run.c) we use this.  We have a file scope
    // static pointer to it in block_common.h.
    pthread_mutex_t *mutex;

    // This saves us from having to write code to "enter" and "exist" the
    // GTK3 API to query the corresponding visibility of the window.  You
    // see, we can't just call GTK3 functions willy nilly like a "normal"
    // program.  We are running the GTK3 main loop in a way that is
    // impossible...  Do you really want to go down that rabbit hole?
    // If so, read more of the source comments in other related files.
    //
    bool isShowing;

    // Top level window title.
    char *title;

    // count widgets: button.so, slider.so, and other widgets, and the
    // base.so too.
    uint32_t refCount;

    // base.so creates this interface in declare(), if a base.so it
    // loaded.
    //
    // It lets code in _run.so tell base.so that the GTK3 top level window
    // was destroyed; like when the close window X button is pressed.
    struct QsInterBlockJob *destroyWindow;
};


// Widget base class:
struct Widget {

    enum Type type;

    // Doubly linked list of widgets:
    struct Widget *next, *prev;

    // Points back to the struct that holds this.
    struct Window *win;

    void (*setLabel)(struct Widget *widget);

    GtkWidget *gtkLabel;

    char label[LABEL_SIZE];
};


struct Text {

    struct Widget widget; // inherit Widget

    char value[32];

    GtkWidget *entry;

    // Lets the block set the GTK3 entry text.
    void (*setTextValue)(struct Text *text, char *val);
};


struct Button {

    struct Widget widget; // inherit Widget

    bool value; // toggle depressed or not.

    // Do we push the getter value?  We do not if we are just setting the
    // button value that is displayed; that's different than toggling the
    // button with the mouse.
    bool pushValue;

    GtkWidget *button;

    // Lets the block set the GTK3 button value.
    void (*setButtonValue)(struct Button *button, bool val,
            bool pushValue/*to getter*/);

    // button.so creates this interface in declare().
    //
    // It lets code in _run.so tell button.so what the button value is.
    struct QsInterBlockJob *setValue;
};


struct Slider {

    // This is only data that needs to be seen by _run.c (_run.so)
    // and slider.c (slider.so).

    struct Widget widget; // inherit Widget

    // These are the only two slider varying parameters that we let _run.c
    // see.  numTicks is constrained to be less than or equal to
    // numValues; and also numTicks - 1 must be divisible by numTicks - 1.
    uint32_t numValues, numTicks;

    // This is the value, in slider coordinates [-1, 1], that is saved
    // from before the slider GTK widget is setup; so that it will be
    // displayed as the value on the slider when the GTK widget first
    // shows.
    double value;

    // Do we push the getter value?  We do not if we are just setting the
    // slider value that is displayed; that's different than moving the
    // slider with the mouse.
    bool pushValue;

    GtkAdjustment *adjustment; // That the scale is built from.
    GtkScale *scale;

    ////////////////////////////////////////////////////////////////////
    // For quickstream block slider.so telling GTK3 _run.so what to do
    ///////////////////////////////////////////////////////////////////
    //
    // Lets the block set the GTK3 range (slider) value.
    //
    // Because GTK provides inadequate parametrization of the slider we
    // just let the slider be from -1.0 to +1.0 and scale the values on
    // the quckstream control parameter side.
    //
    // Called from a thread pool worker.
    //
    void (*setSliderValue)(struct Slider *s, double x, bool pushValue);
    //
    void (*setSliderRange)(struct Slider *s);
    //

    // The values coming from _run.so are constrained to be from -1.0 to
    // 1.0 with ...

    ////////////////////////////////////////////////////////////////////
    // For GTK3 _run.so telling quickstream block slider.so what to do
    ////////////////////////////////////////////////////////////////////
    //
    // slider.so creates this interface in declare().
    //
    // It lets code in _run.so tell block slider.so what the slider value
    // is.
    //
    struct QsInterBlockJob *setValue;
    //
    // slider.so creates this interface in declare().
    //
    // It lets code in _run.so tell slider.so that the widget is being
    // played with by the user now, or is not being played with now.
    //
    struct QsInterBlockJob *setActive;
    //
    // Called by a GTK callback in a GTK thread, so we need a
    // window::mutex lock in it, to access the Slider data; because
    // this can be accessing Slider data 
    //
    char *(*setDisplayedValue)(const struct Slider *s, double val);
    //
    ///////////////////////////////////////////////////////////////////



    bool isActive;
};
