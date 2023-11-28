
// NOTE: Everything in this file is static and it has to be that way.
//
// Don't confuse this C global static with C++ class static.  Nothing but
// the block code can see this code (Why I'm writing this comment.).
//
// In effect this adds code to what ever C file you put it in.
// It's only used for blocks that are "Widget blocks".


// This will point to inter-thread shared memory for this block.
//
// There's a different pointer variable in each loaded block.
static struct Window *win = 0;

// mutex to protect the data pointed to by win.
static pthread_mutex_t *mutex;



// The first block to "show" gets to call this to load _run.so.
static inline void InitRunAPI(void) {

    DASSERT(win);

    // This will ASSERT() if it fails; a small dlopen(3) wrapper.
    //
    // _run.so is linked with the needed GTK3 libraries, so the GTK3
    // libraries get automatically loaded with _run.so if they are not
    // loaded already.
    //
    void *dlhandle = qsOpenRelativeDLHandle("_run.so");
    win->dlhandle = dlhandle;

    // Now dynamically load the _run.so library API.
    //
    // All these will assert too, if they fail.
    // qsGetDLSymbol() is a small dlsym(3) wrapper.
    //
    win->window_init = qsGetDLSymbol(dlhandle, "window_init");
    win->window_reinit = qsGetDLSymbol(dlhandle, "window_reinit");
    win->show = qsGetDLSymbol(dlhandle, "show");
    win->hide = qsGetDLSymbol(dlhandle, "hide");
    win->window_cleanup = qsGetDLSymbol(dlhandle, "window_cleanup");
    win->window_setTitle = qsGetDLSymbol(dlhandle, "window_setTitle");
}


static inline void Show(bool show, bool gtkLock) {

    DASSERT(win);
    DASSERT(mutex);

    DSPEW();

    CHECK(pthread_mutex_lock(mutex));

    if(show && !win->show) {
        DASSERT(!win->hide);
        DASSERT(!win->window_init);
        DASSERT(!win->window_cleanup);

        // Setup the _run.so API with dlopen() and dlsym().
        InitRunAPI();

        DASSERT(win->hide);
        DASSERT(win->window_init);
        DASSERT(win->show);
        DASSERT(win->window_cleanup);
    }

    if(!show && !win->window)
        goto finish;

    if(show && !win->window) {
        // This will build or rebuild the GTK3 top level window widget.
        win->window_init(win, gtkLock);
        goto finish;
    }

    if((show && win->isShowing) ||
        (!show && !win->isShowing))
        // no visual state change requested
        goto finish;

    // Now just show or hide the top level window.
    if(show) {
        win->show(win, gtkLock);
        goto finish;
    }
    // else
    DASSERT(win->isShowing);
    win->hide(win, gtkLock);

finish:

    CHECK(pthread_mutex_unlock(mutex));
    DSPEW("show=%d", show);
}


// This is the setter "show" callback for base.so, button.so, and
// slider.so.  It's indirectly doing gtk_widget_show(topWindow).  It can't
// call any of the GTK3 widget API directly, because the blocks that use
// it can't statically (build-time) link with the GTK3 libraries.
static
int Show_setter(const struct QsParameter *p, const bool *value,
            uint32_t readCount, uint32_t queueCount,
            void *userData) {

    Show((*value)?true:false, true);
    return 0;
}


static
char *Show_config(int argc, const char * const *argv,
            struct Widget *w) {

    bool show;
    qsParseBoolArray(true, &show, 1);
    Show(show, false);

    return 0;
}


static
char *Label_config(int argc, const char * const *argv,
            struct Widget *w) {

    DASSERT(win);
    DASSERT(mutex);


    CHECK(pthread_mutex_lock(mutex));


    if(argc < 2) {
        // Blank label
        w->label[0] = '\0';
        goto finish;
    }

    // We just take argv[1] as the widget label.
    snprintf(w->label, LABEL_SIZE, "%s", argv[1]);

    if(w->setLabel)
        w->setLabel(w);

finish:

    CHECK(pthread_mutex_unlock(mutex));

    return mprintf("label \"%s\"", argv[1]);
}



// This makes the control parameter "show" and adds the "widget"
// for a block.
//
static inline struct Window *CreateWidget(struct Widget *w) {

    DASSERT(!win);
    DASSERT(!mutex);

    if(w)
        // Zero the parts of Widget that need it.
        w->setLabel = 0;

    // Get inter-thread shared memory.  win is the pointer to it.
    //
    // Yes, yes; we know that all memory in this address space is
    // inter-thread shared memory, but we need this memory to come from
    // code that is not originating from in a block so that it can be
    // shared by all blocks that exist in this graph now and may not exist
    // in the future, and all new blocks too.  Memory for all the blocks
    // that is not from any block.  [so good I added this comment to the
    // docs]
    bool firstCaller;
    // The block knows what graph it is in, so we do not need
    // to pass a graph pointer to qsGetMemory().
    //
    win = qsGetMemory(MEM_NAME, sizeof(*win),
            &mutex, &firstCaller, QS_GETMEMORY_RECURSIVE);
    win->mutex = mutex;
    DASSERT(win); // it was checked in qsGetMemory().

    if(!firstCaller)
        // This little (lock interface) asymmetry is necessary to keep
        // initialization of the allocated memory to happen in just one
        // thread.
        //
        // In order to make the creation of the memory thread safe we will
        // already have a mutex lock if we created the memory; and we'll
        // not need to do this pthread_mutex_lock(mutex).
        //
        // So the first block to call this does not need to call this:
        CHECK(pthread_mutex_lock(mutex));

    // And what-do-ya-know the win object being initialized with just 0
    // works; so that shit I said about win initialization does not seem
    // to matter in this use case, but in a more generic case it may.


    if(!w && win->destroyWindow) {
        // This is base.so and win->destroyWindow is set already, so it
        // is another base.so, and we can't have more than one base.so
        // per graph.
        CHECK(pthread_mutex_unlock(mutex));
        mutex = 0;
        win = 0;
        return 0; // Fail.
    }

    // Add the widget to the Window list of widgets.
    //
    // These are not GTK3 widgets just a log of what GTK3 widgets there
    // will be, when and if the window is shown via the "show" quickstream
    // control parameter.
    //
    // Add to the last, end of the Window widget list.
    if(w) {
        w->win = win;

        DASSERT(w->next == 0);
        DASSERT(w->prev == 0);
        if(win->last) {
            DASSERT(win->first);
            win->last->next = w;
            w->prev = win->last;
        } else {
            DASSERT(!win->first);
            win->first = w;
        }
        win->last = w;

        snprintf(w->label, LABEL_SIZE, "%s",
                qsBlockGetName());

        char *text = mprintf("label \"%s\"", w->label);

        qsAddConfig((char *(*)(int argc, const char * const *argv,
            void *userData)) Label_config, "label",
            "set widget label"/*desc*/,
            "label TEXT",
            text);

        DZMEM(text, strlen(text));
        free(text);
    }

    qsAddConfig((char *(*)(int argc, const char * const *argv,
            void *userData)) Show_config,
            "show",
            "show the widgets"/*desc*/,
            "show [bool]",
            "show true");


    // We need block user data set for Label_config().
    //
    // For the base.so w == 0 and that's okay too.
    //
    qsSetUserData(w);


    // We make a control parameter to show (value=true) and hide
    // (value=false) the GTK3 top level window widget(s).
    qsCreateSetter("show",
        sizeof(win->isShowing), QsValueType_bool, &win->isShowing,
        (int (*)(const struct QsParameter *p, const void *value,
            uint32_t readCount, uint32_t queueCount,
            void *userData)) Show_setter);


    ++win->refCount;


    if(w && win->window_init) {
        DASSERT(win->first);
        DASSERT(win->refCount);
        // The top level window needs to be rebuilt.
        //
        // TODO: Maybe add a function that just adds the GTK3 widget
        // without recreating the GTK3 top level window.
        //
        // Note: if the block is base.so (w==0) we do not get here.
        // That is a case where the showing widgets do not change
        // how the top level window looks.
        win->window_reinit(win, false);
    }

    if(w)
        CHECK(pthread_mutex_unlock(mutex));
    // else
    //   this is the one and only base.so and we return with the mutex
    //   locked.

    return win;
}


static inline void _FreeWindowTitle() {

    DASSERT(win);
    if(win->title) {
#ifdef DEBUG
        memset(win->title, 0, strlen(win->title));
#endif
        free(win->title);
        win->title = 0;
    }
}


static inline void
SetWindowTitle(const char *title) {

    DASSERT(win);
    DASSERT(mutex);
    if(title && !title[0])
        title = 0;


    CHECK(pthread_mutex_lock(mutex));

    _FreeWindowTitle();

    if(title) {
        win->title = strdup(title);
        ASSERT(win->title, "strdup() failed");
    }

    if(win->window_setTitle)
        win->window_setTitle(win);

    CHECK(pthread_mutex_unlock(mutex));
}
    

// Destroy the widget, w, that is in the given Window.
//
// This happens when a "widget" block is unloaded and its'
// undeclare() is called.
static inline void
DestroyWidget(struct Widget *w) {

    DASSERT(win);
    DASSERT(win->refCount);
    DASSERT(mutex);

    bool doCleanup = false;

    CHECK(pthread_mutex_lock(mutex));

    --win->refCount;

    if(w) {
        // Remove the widget, w, from the Window, win, widget list.
        if(w->next) {
            DASSERT(win->last != w);
            w->next->prev = w->prev;
        } else {
            DASSERT(win->last == w);
            win->last = w->prev;
        }
        if(w->prev) {
            DASSERT(win->first != w);
            w->prev->next = w->next;
        } else {
            DASSERT(win->first == w);
            win->first = w->next;
            w->prev = 0;
        }
        w->next = 0;
    } else {
        // This is base.so calling undeclare().
        DASSERT(win->destroyWindow);
        win->destroyWindow = 0;
    }


    if(w && win->first && win->window_cleanup && win->refCount)
        // The top level window needs to be rebuilt.
        //
        // Note: if the block is base.so (w==0) we do not get here.
        // That is a case where the showing widgets do not change
        // how the top level window looks.
        win->window_reinit(win, false);


    doCleanup = (win->refCount == 0)?true:false;

    CHECK(pthread_mutex_unlock(mutex));

    if(!doCleanup) return;


    // This is the last block that is a widget, or base.so, in this graph
    // that is being destroyed and undeclare() in this last block is being
    // called; for this graph.
    //
    // The block knows what graph it is in, so we do not need to pass
    // a graph pointer to qsFreeMemory().

    // There is the case where _run.so was never loaded and none
    // of the _run.so API functions have been gotten (or called).
    //
    if(win->window_cleanup) {
        win->window_cleanup(win, false);
        // We can not call dlclose(win->dlhandle) now.  GTK3 can't have
        // gtk_main_quit() called now.  This _run.so will automatically
        // have it's destructor called when the program exits, which it
        // what happen when you call dlclose(win->dlhandle).  Calling
        // dlclose(win->dlhandle) now crashes the program.  GTK3 sucks
        // that way.
        //dlclose(win->dlhandle); // Cannot do it.
    }
    // else InitRunAPI() was never called.

    DASSERT(!win->window);

    _FreeWindowTitle();

    // In general there is not a requirement to free this memory for
    // a program that is exiting, but it may be the case that the
    // blocks are unloaded and than loaded again.  We need this to
    // work for this "reset" case, and without a memory leak.
    //
    // Without this "reset" case the graph would cleanup this memory
    // for us.  We do have a Valgrind test (../tests/750_qsGetMemory)
    // for it too, but we could also add a more complex test of it
    // too.

    qsFreeMemory(MEM_NAME); // for this block's graph.

    // Not that this matters, but makes it easier to think about it.
    win = 0;
    mutex = 0;

    // If slider.so, button.so, base.so, or other "widget" block in
    // this directory is loaded into this graph; a CreateWidget() call
    // from the blocks declare() will get win and mutex reset.
}
