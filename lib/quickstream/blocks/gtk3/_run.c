// The compiled DSO (dynamic shared object) from this C file (_run.c) (and
// maybe other files) must be kept in the same directory with button.so
// and slider.so, so that  button.so and slider.so it can find this DSO
// compiled from this file.

// This is kept as a separate DSO from button.so and slider.so that the
// graph builder does not have to load this DSO when just building the
// graph.  This DSO is just loaded at "run-time", and is not required to
// be loaded at quickstream graph "build-time".  This DSO links with the
// large number of libraries needed for the GTK (Gimp ToolKit) widget
// libraries, and we do not necessarily want that at quickstream flow
// graph "build-time".


// Why is this so complicated?  I don't know how thread safe GTK is; and I
// should not have to know either.  This makes all code that accesses the
// GTK3 API run in g_source file (pipe(2) reader) callback.  It writes the
// pipe(2) to trigger the callback that acts as a gate keeper for "other"
// threads that may call GTK API functions.  That's what
// ../../../libqsGtk_init.so does for _run.so.  The source to
// libqsGtk_init.so is in ../../../qsGtk_init.c and maybe other files near
// that.


// This runs a GTK main loop in a created thread and other threads in a
// g_source file callback; that is unless you run this with quickstreamGUI
// which does not run a GTK main loop here, and uses the quickstreamGUI
// GTK main loop.  GTK as in GTK3 is not very modular.  It's the fucking
// center-of-the-universe.  You can use dlopen() to load it and run it,
// but once you run it (gtk_init()) you can't unload it.  It leaks system
// resources like a stuck pig.  I don't imagine QT is any better.  QT is a
// super bloated monster.  GTK3 is just a regular bloated monster.  Both
// are broken in their own way.
//
// I'm just being objective.  quickstream sucks donkey dick.  There I said
// it.  quickstream is total shit.  But, maybe you can use it to do your
// shit.  Get over yourself...  I have.  I'm a little put off by people
// getting personal (upset) about bugs in their code.  I can take
// criticism from anywhere, dumb people, termites, and inanimate objects,
// without getting upset.  Ha, ha; I wish; but, I strive to do so.
//
// Note: we do not poke into the guts of glib and g_main loop stuff to do
// this.  We just use a g_source file to inject callbacks into the main
// GTK loop, by writing and reading a single char to a dummy UNIX pipe
// file.  The GTK3 interfaces we use are all public API user level, with
// no dlsym() function overriding code injections (which are something to
// avoid).


// RANT:
// We cannot get this to run with tests with ValGrind.  It's likely we
// will not be able to get GTK (version 3) to not leak memory and other
// computer operating system resources.  Yes, that is confirmed by GTK
// developers.  GTK3 is not designed to unload all it's system resources
// when it is unloaded via dlclose(2).  It also, obviously, has no way to
// undo what gtk_init() does; and that is not in the GTK3 documentation.
// GTK3 is very refined and finished in many ways, but it totally sucks
// at the core.  Too many programmers specialize and can't doing system
// core programming.  gObject is a total CPP macro madness shit show.
// glib has some cool shit, but the bloat use cost is too high for me and
// most of it is just a very light (and in-your-way useless) wrappers of
// libc and NPTL.  I figure, if you can't program with NPTL, you should
// not call yourself a computer programmer (not in the game I'm playing).
// GNU/Linux was a piece of shit before NPTL; at least when you think
// about threads.
//
// Don't get me started on QT ...


// This file is linked with the GTK3 libraries.

#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <sched.h>
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <stdatomic.h>
#include <pthread.h>

#include <gtk/gtk.h>

#include "../../../debug.h"
#include "../../../qsGtk_init.h"
#include "../../../../include/quickstream.h"

#include "all_common.h"


// This DSO (from compiling this file) is not a quickstream block.  It is
// loaded once per process.
//
// Unlike blocks that get loaded for each instance/call of
// qsGraph_createBlock(); and don't forget there can be any number of
// graphs per process.  This DSO can't be loaded with
// qsGraph_createBlock().


// The first block to load in this gtk3/ directory will load this DSO.


// Did this DSO run qsGtk_init?
//
// It only can be run once per process.  One thread could run it, but
// a different thread could clean it up, using these two variables and
// the ../../../libqsGtk_init.so API.
//
static bool thisRun_qsGtk_init = false;
static pthread_t gtkThread;


// NOTE: We can't have:
//static struct Window w


// This is called from block_common.h with a win->mutex lock.
//
// This is called from a thread pool worker thread in a setter callback,
// so it needs to call qsGetGTKContext().
//
void show(struct Window *win, bool needLock) {

    DASSERT(win);
    DASSERT(win->mutex);
    DASSERT(win->window);
    DASSERT(!win->isShowing);

#ifdef DEBUG

    fprintf(stderr, "Making Widgets:");
    for(struct Widget *w = win->first; w; w = w->next)
        fprintf(stderr, " [type %d label=\"%s\"]", w->type, w->label);
    fprintf(stderr, "\n");

#endif

    if(needLock)
        qsGetGTKContext();

    gtk_widget_show(win->window);

    if(needLock)
        qsReleaseGTKContext();

    DASSERT(win->isShowing);

    DSPEW();
}


// This is called from block_common.h with a win->mutex lock.
//
// This is called from a thread pool worker thread in a setter callback,
// so it needs to call qsGetGTKContext().
//
void hide(struct Window *win, bool needLock) {

    DASSERT(win);
    DASSERT(win->window);
    DASSERT(win->isShowing);

    if(needLock)
        qsGetGTKContext();

    gtk_widget_hide(win->window);

    if(needLock)
        qsReleaseGTKContext();

    DASSERT(!win->isShowing);

    DSPEW();
}


// This is the main loop GTK thread function.  If another thread
// (non-GTK-threads) call GTK API functions they need to wrap all the GTK
// calls with qsGetGTKContext() before and qsReleaseGTKContext() after the
// GTK API calls.
//
static void *
RunGtk3Main(pthread_barrier_t *barrier) {

    NOTICE("Starting GTK3 main loop thread");

    qsGtk_init(0, 0);

    int r = pthread_barrier_wait(barrier);
    ASSERT(r == 0 || r == PTHREAD_BARRIER_SERIAL_THREAD);

    // GTK main loop.
    //
    gtk_main();


    // TODO: If there is still a main GTK top level window, we need to
    // destroy it now.   WTF??


    // Cleanup.
    qsGtk_cleanup();

    NOTICE("GTK3 main loop thread FINISHED");

    return 0;
}


static void
Hide_cb(GtkWidget *w, struct Window *win) {
    DASSERT(w);
    DASSERT(win);
    DASSERT(win->window == w);
    DASSERT(win->mutex);

    // This is in a GTK3 callback so we do not need a qsGetGTKContext()
    // and qsReleaseGTKContext().  Not to mention we'll not calling any
    // GTK3 API functions.

    // This is a recursive mutex and in some calls to this function
    // we will double lock this mutex.
    CHECK(pthread_mutex_lock(win->mutex));

    DASSERT(win->isShowing);
    win->isShowing = false;

    CHECK(pthread_mutex_unlock(win->mutex));
    DSPEW();
}


static void
Show_cb(GtkWidget *w, struct Window *win) {
    DASSERT(w);
    DASSERT(win);
    DASSERT(win->window == w);
    DASSERT(win->mutex);

    // This is in a GTK3 callback so we do not need a qsGetGTKContext()
    // and qsReleaseGTKContext().  Not to mention we'll not calling any
    // GTK3 API functions.

    CHECK(pthread_mutex_lock(win->mutex));

    DASSERT(!win->isShowing);
    win->isShowing = true;

    CHECK(pthread_mutex_unlock(win->mutex));
    DSPEW();
}


static void
Destroy_cb(GtkWidget *w, struct Window *win) {
    DASSERT(w);
    DASSERT(win);
    DASSERT(win->mutex);
    DASSERT(win->window == w);

    CHECK(pthread_mutex_lock(win->mutex));

    // The GTK3 top level window was destroyed.  If another show happens
    // we'll see that win->window = 0 and recreate the top level window
    // again.
    win->window = 0;

    CHECK(pthread_mutex_unlock(win->mutex));
    DSPEW();
}



static gboolean
Delete_cb(GtkWidget *w, GdkEvent *e, struct Window *win) {
    DASSERT(w);
    DASSERT(win);
    DASSERT(win->mutex);
    DASSERT(win->window == w);

    CHECK(pthread_mutex_lock(win->mutex));

    // I think that the X close window button was clicked and since
    // we are catching this event, that's all that happens; and
    // we can customize what to do from here.

    if(win->destroyWindow) {
        bool val = true;
        qsQueueInterBlockJob(win->destroyWindow, &val);
    }

    //win->window_cleanup(win);
    gtk_widget_hide(win->window);

    CHECK(pthread_mutex_unlock(win->mutex));
    DSPEW();

    return TRUE;
}


// Called when the button setter was set new value.
//
// This is called from a thread pool worker thread in a setter callback,
// so it needs to call qsGetGTKContext().
//
// The caller of this function needs the Window::mutex lock.
//
static
void SetButtonValue(struct Button *button, bool val) {

    DASSERT(button);
    DASSERT(button->widget.win);
    DASSERT(button->widget.win->mutex);

DSPEW("value=%d", val);

    qsGetGTKContext();
    gtk_toggle_button_set_active(
            GTK_TOGGLE_BUTTON(button->button),
            val?TRUE:FALSE);

    if(button->value != val) {
        button->value = val;
        qsQueueInterBlockJob(button->setValue, &button->value);
    }
    qsReleaseGTKContext();
}



static void
ButtonToggled_cb(GtkToggleButton *togglebutton,
        struct Button *button) {

    DASSERT(button);
    DASSERT(button->widget.win);
    DASSERT(button->widget.win->mutex);

    CHECK(pthread_mutex_lock(button->widget.win->mutex));

    DASSERT(button->setValue);

    button->value = button->value?false:true;

    // This copies the value at button->value and then queues the event
    // to a thread pool worker.
    qsQueueInterBlockJob(button->setValue, &button->value);

    CHECK(pthread_mutex_unlock(button->widget.win->mutex));
}


static
void SetLabel(struct Widget *w) {

    if(w->gtkLabel)
        gtk_label_set_text(GTK_LABEL(w->gtkLabel), w->label);
    else {
        DASSERT(w->type == Button);
        struct Button *b = (void *) w;
        gtk_button_set_label(GTK_BUTTON(b->button), w->label);
    }
}

static void
SetTextValue(struct Text *text, char str[32]) {

    DASSERT(text);
    DASSERT(text->entry);

    gtk_entry_set_text(GTK_ENTRY(text->entry), str);
}


static void 
CreateText(struct Window *win, struct Text *text) {

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,
            1 /*spacing*/);

    GtkWidget *w =
        gtk_label_new(text->widget.label);
    text->widget.gtkLabel = w;
    gtk_box_pack_start(GTK_BOX(hbox), w, TRUE/*expand*/,
                TRUE/*fill*/, 2/*padding*/);

    w = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(w), text->value);
    gtk_editable_set_editable(GTK_EDITABLE(w), FALSE);
    text->entry = w;

    gtk_box_pack_start(GTK_BOX(hbox), w, TRUE/*expand*/,
                TRUE/*fill*/, 2/*padding*/);

    gtk_box_pack_start(GTK_BOX(win->vbox), hbox, FALSE, TRUE, 2);

    text->setTextValue = SetTextValue;
}


static void 
CreateButton(struct Window *win, struct Button *button) {

    GtkWidget *b =
        gtk_toggle_button_new_with_label(button->widget.label);
    g_signal_connect(b, "toggled", G_CALLBACK(ButtonToggled_cb), button);
    gtk_box_pack_start(GTK_BOX(win->vbox), b, FALSE, TRUE, 2);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(b), button->value);
    button->button = b;
    button->setButtonValue = SetButtonValue;
}


static
void SliderValueChanged_cb(GtkRange *range, struct Slider *slider) {

    DASSERT(slider);
    DASSERT(slider->widget.win);
    DASSERT(slider->widget.win->mutex);

    CHECK(pthread_mutex_lock(slider->widget.win->mutex));

    double val = gtk_range_get_value(range);

    DASSERT(slider->setValue);

    //DSPEW("              val=%lg", val);

    qsQueueInterBlockJob(slider->setValue, &val);

    if(!slider->isActive && gtk_widget_is_focus(GTK_WIDGET(range))) {
        // The value changed and this widget has focus.  We guess that
        // the change was due to moving the slider, so we signal the
        // isActive control parameter.
        slider->isActive = true;
        qsQueueInterBlockJob(slider->setActive, &slider->isActive);
    }

    CHECK(pthread_mutex_unlock(slider->widget.win->mutex));
}


static
gboolean SliderFocusOutEvent_cb(GtkWidget *widget,
               GdkEvent  *event,
               struct Slider *slider) {

    DASSERT(slider);
    DASSERT(slider->widget.win);
    DASSERT(slider->widget.win->mutex);

    CHECK(pthread_mutex_lock(slider->widget.win->mutex));

    if(slider->isActive) {
        // The slider just became inactive, it's out of focus now.
        // TODO: Find a better event than focus out for this.
        slider->isActive = false;
        qsQueueInterBlockJob(slider->setActive, &slider->isActive);
    }

    CHECK(pthread_mutex_unlock(slider->widget.win->mutex));

    return FALSE; // FALSE to propagate the event further.
}


// The caller of this must lock the Window::mutex.
//
static
void SetSliderValue(struct Slider *s, double val) {

    DASSERT(s);
    DASSERT(s->widget.win);
    DASSERT(s->widget.win->mutex);

    CHECK(pthread_mutex_lock(s->widget.win->mutex));

    qsGetGTKContext();

    gtk_adjustment_set_value(s->adjustment, val);

    qsReleaseGTKContext();

    CHECK(pthread_mutex_unlock(s->widget.win->mutex));
}


// Set the slider parameters that are used in GTK.
//
// The caller of this function should have a Window mutex lock.
//
static void
SetSliderRange(struct Slider *s) {

    // It would appear that half the parameters of the GtkAdjustment are
    // ignored in the overriding gtk_scale class does whatever the fuck it
    // does.  So, as a consequence of this inadequate parametrization, we
    // use the GtkScale at it's highest resolution bounding it's output
    // values between -1.0 to 1.0 and add our own display and output value
    // which will be a general linear scaling followed by digitization of
    // the values from the GtkScale.  If the user does use discrete
    // increments for values, the scale will be shown moving smoothly
    // between discrete values instead of jumping to the discrete step
    // points in the scale, as it does when using the step increment
    // provided by GtkAdjustment and GtkScale.  I'm not sure if this
    // presentation will be better or worst, that may just depend on the
    // end application.
    //
    // Note: we end up with lots of data replication, values in
    // GtkAdjustment which are also in the struct Slider.  That is hard to
    // avoid if we wish to use the GTK3 interfaces that are provided.  And
    // also note, GTK3 already has useless data in GtkAdjustment which
    // GtkScale ignores.  So pretty much double the data that is needed is
    // used.  That's what computers mostly do these days, compute useless
    // shit.  Metadata about metadata about metadata ... to infinity and
    // beyond.  That's why our faster computers are slower than computers
    // of the past, more layers of useless shit keeps getting added as
    // time progresses.

    gtk_scale_clear_marks(s->scale);

    if(s->numTicks) {

        for(uint32_t i = 0; i < s->numTicks; ++i) {
            double x = -1.0 + 2.0*i/((double) (s->numTicks - 1));
            // Note: the slider makes these points "sticky".  You will
            // have to try it to see what this means.
            //
            gtk_scale_add_mark(s->scale, x, GTK_POS_BOTTOM, 0/*text*/);
        }
    }
}


static char *
FormatValue_cb(GtkScale *scale,
               double value,
               struct Slider *s) {

//DSPEW("value=%lg", value);

    return s->setDisplayedValue(s, value);
}


static void 
CreateSlider(struct Window *win, struct Slider *s) {

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,
            1 /*spacing*/);

    GtkWidget *w = gtk_label_new(s->widget.label);
    s->widget.gtkLabel = w;

    gtk_box_pack_start(GTK_BOX(hbox), w, FALSE/*expand*/,
                FALSE/*fill*/, 2/*padding*/);

    double val = 0.0;
    if(isnormal(s->value))
        val = s->value;
    GtkAdjustment *a =
        gtk_adjustment_new(val, -1.0, 1.0, 0.0, 0.0, 0.0);

    s->adjustment = a;

    w = gtk_scale_new(GTK_ORIENTATION_HORIZONTAL, a);
    gtk_scale_set_digits(GTK_SCALE(w), 16);
    s->scale = GTK_SCALE(w);

    gtk_box_pack_start(GTK_BOX(hbox), w, TRUE/*expand*/,
                TRUE/*fill*/, 2/*padding*/);

    g_signal_connect(w, "value-changed",
                G_CALLBACK(SliderValueChanged_cb), s);
    g_signal_connect(w, "format-value",
                G_CALLBACK(FormatValue_cb), s);
    g_signal_connect(w, "focus-out-event",
                G_CALLBACK(SliderFocusOutEvent_cb), s);

    gtk_box_pack_start(GTK_BOX(win->vbox), hbox, FALSE/*expand*/,
            FALSE/*fill*/, 2/*padding*/);

    // One function to set just the value.  Used by quickstream setter
    // and getter control parameters.
    s->setSliderValue = SetSliderValue;
    // One function that sets all the slider parameters as this slider
    // thing defines them.  Used by a quickstream configure interface.
    SetSliderRange(s);

    s->setSliderRange = SetSliderRange;
}



// The caller of this will have the mutex locked from block_common.h.
//
void window_setTitle(struct Window *win) {

    DASSERT(win);

    if(!win->window) return;

    // Not correct. qsGetGTKContext() and qsReleaseGTKContext() should
    // not be used here.  This BUG was not hard to find, but I want this
    // comment here so I don't add this BUG back to the code.
    //
    // window_setTitle() is called by a quickstream configure thingy,
    // which means a thread pool worker is not calling this; and the
    // thread calling this is friendly to the GTK3 API; it should already
    // have good access to the GTK API; like a main thread.  Calling
    // qsGetGTKContext() will cause a mutex dead lock, and GTK3 hangs
    // with an unresponsive window.  Running: "gdb -pid PID" and see
    // the thread hang at this qsGetGTKContext() (mutex) lock call:
    //qsGetGTKContext();

    if(win->title)
            gtk_window_set_title(GTK_WINDOW(win->window), win->title);
        else
            gtk_window_set_title(GTK_WINDOW(win->window), "quickstream");

    // Not correct.  See above.
    //qsReleaseGTKContext();
}


// The caller of this will have the Window::mutex locked from
// block_common.h.
//
// if needLock is true call qsGetGTKContext() and qsReleaseGTKContext().
//
void window_reinit(struct Window *win, bool needLock) {

    DASSERT(win);
    DASSERT(win->window);

    if(needLock)
        qsGetGTKContext();

    if(win->vbox)
        gtk_widget_destroy(win->vbox);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_container_add(GTK_CONTAINER(win->window), vbox);
    win->vbox = vbox;

    // Create all the widgets in the, win->first, list if there are
    // any.
    for(struct Widget *w = win->first; w; w = w->next) {

        switch(w->type) {
            case Button:
                CreateButton(win, (void *) w);
                break;
            case Slider:
                CreateSlider(win, (void *) w);
                break;
            case Text:
                CreateText(win, (void *) w);
                break;
            default:
                break;
        }
        w->setLabel = SetLabel;
    }
    gtk_widget_show_all(win->window);

    if(needLock)
        qsReleaseGTKContext();
}



// The caller of this will have the mutex locked from block_common.h.
//
// if needLock is true call qsGetGTKContext() and qsReleaseGTKContext().
//
void window_init(struct Window *win, bool needLock) {

    DASSERT(win);

    ASSERT(!qsGtk_finishedRunning);


    if(!qsGtk_didRun) {

        // This DSO will now launch a custom GTK3 main loop pthread.  Ya,
        // I've been told this is impossible by the GTK development team,
        // but I'm fuck'n smart.  It's not a piece-of-shit gthread either.
        // Well then again, no, I'm a dumb-ass that is very persistent.

        // There is no point in making the barrier a global since it's
        // just temporary and we can share it so long as this code block
        // is in scope in some thread.
        pthread_barrier_t barrier;
        CHECK(pthread_barrier_init(&barrier, 0, 2));

        CHECK(pthread_create(&gtkThread, 0,
                (void  *(*)(void *)) RunGtk3Main, &barrier));

        thisRun_qsGtk_init = true;
        // Wait for the thread to get the mutex and call gtk_init():
        int r = pthread_barrier_wait(&barrier);
        ASSERT(r == 0 || r == PTHREAD_BARRIER_SERIAL_THREAD);
        // We just needed this barrier once:
        CHECK(pthread_barrier_destroy(&barrier));
    }


    if(!win->window) {

        if(needLock)
            qsGetGTKContext();

        // Create the top level window.  Like I said above, we do this
        // once.
        DASSERT(win->window == 0);
        GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        ASSERT(window);
        win->window = window;
        if(win->title)
            gtk_window_set_title(GTK_WINDOW(window), win->title);
        else
            gtk_window_set_title(GTK_WINDOW(window), "quickstream");
        gtk_window_set_default_size(GTK_WINDOW(window), 600, 10);
        gtk_container_set_border_width(GTK_CONTAINER(window), 8);
        // Make is so we keep win->isShowing in sync with whither the top
        // window is showing or not.

        g_signal_connect(window, "delete-event",
                G_CALLBACK(Delete_cb), win);
#if 0
        g_signal_connect(window, "delete-event",
                G_CALLBACK(gtk_widget_hide_on_delete), 0);
#endif
        g_signal_connect(window, "hide", G_CALLBACK(Hide_cb), win);
        g_signal_connect(window, "show", G_CALLBACK(Show_cb), win);
        // Make it so that if window is destroyed win->window is set to 0. 
        g_signal_connect(window, "destroy", G_CALLBACK(Destroy_cb), win);

        // Add the rest of the GTK3 widgets.
        window_reinit(win, false);

        if(needLock)
            qsReleaseGTKContext();
    }

    DSPEW();
}


// The caller of this will have the mutex locked from block_common.h.
//
// if needLock is true call qsGetGTKContext() and qsReleaseGTKContext().
//
void window_cleanup(struct Window *win, bool needLock) {

    DASSERT(win);

    if(win->window) {

        if(win->window) {

            if(needLock)
                qsGetGTKContext();

            gtk_widget_destroy(win->window);
            win->vbox = 0;

            if(needLock)
                qsReleaseGTKContext();

            DASSERT(win->window == 0);
        }

        for(struct Widget *w = win->first; w; w = w->next) {
            switch(w->type) {
                case Button:
                {
                    struct Button *b = (void *) w;
                    b->setButtonValue = 0;
                    b->button = 0;
                }
                    break;
                case Slider:
                {
                    struct Slider *s = (void *) w;
                    s->setSliderValue = 0;
                    s->setSliderRange = 0;
                    s->scale = 0;
                    s->adjustment = 0;
                }
                    break;
                case Text:
                {
                    struct Text *t = (void *) w;
                    t->setTextValue = 0;
                }
                    break;
                default:
                    break;
            }
        }
        DASSERT(win->window == 0);
    }
    DSPEW();
}


static void __attribute__ ((destructor)) gtk3_cleanup(void);

static void gtk3_cleanup(void) {

    if(qsGtk_finishedRunning || !thisRun_qsGtk_init)
        // Nothing to do now.  The main loop thread terminated somehow.
        // It is likely that the GTK3 main loop thread was not started
        // by this code/thread.
        return;

    //qsGetGTKContext();
    // We have a gtk main loop thread from this DSO file, and so stop our
    // main loop.
    gtk_main_quit();
    //qsReleaseGTKContext();

    // We do not continue from here until the GTK3 main loop thread is
    // joined.
    CHECK(pthread_join(gtkThread, 0));
    INFO("Joined GTK main loop thread from _run.so destructor");
}
