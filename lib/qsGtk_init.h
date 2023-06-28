#ifndef __QSGTK_INIT_H__
#define __QSGTK_INIT_H__


#ifdef BUILD_LIB
// BUILD_LIB is set when compiling qsGtk_init.so
#  define QSG_EXPORT __attribute__((visibility("default"))) extern
#else
// This "undef QSG_EXPORT" stops gcc from printing a warning when we run:
// cc -fpreprocessed -dD -E -P quickstream_expanded.h >> quickstream.h
#  undef QSG_EXPORT
#  define QSG_EXPORT extern
#endif


// Users should use these two variables as read only:
QSG_EXPORT
atomic_uint qsGtk_finishedRunning;
QSG_EXPORT
atomic_uint qsGtk_didRun; // it may still be running.



// Wrapper of gtk_init().  It actually creates a thread that runs
// a "special gtk main loop".  One that lets you call the GTK3 APIs
// from other "unrelated threads" by using qsGetGTKContext() and
// qsReleaseGTKContext().
QSG_EXPORT
void qsGtk_init(int *argc, char ***argv);

// There is no cleanup function for gtk_init() or should I say the cleanup
// function for gtk_init() is exit().   We cleanup what we can of this
// wrapper stuff.
QSG_EXPORT
void qsGtk_cleanup(void);


// Use these as gate keepers for where you call functions in the GTK3
// family of APIs.  Yes qsGetGTKContext() is like an "enter GTK3" and
// qsReleaseGTKContext() is like an "exit GTK3".
QSG_EXPORT
void qsGetGTKContext(void);

QSG_EXPORT
void qsReleaseGTKContext(void);


#endif // #ifndef __QSGTK_INIT_H__
