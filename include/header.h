#ifndef __QUICKSTREAM_H__
#define __QUICKSTREAM_H__

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>
#include <signal.h>
#include <dlfcn.h>

/** \defgroup macros CPP macros

@{
*/


/**
\c QS_MAJOR is the major version number.
\c QS_MAJOR may be increased for a given release,
but not often. */
#define QS_MAJOR  0
/**
\c QS_MINOR is the minor version number.
\c QS_MINOR may be changed for a given release,
but not often. */
#define QS_MINOR  0
/**
\c QS_EDIT is the edit version number.
\c QS_EDIT should be changed for each release. */
#define QS_EDIT   5

// doxygen skips QS_STR and QS_XSTR
#define QS_STR(s) QS_XSTR(s)
#define QS_XSTR(s) #s

/**
\c QUICKSTREAM_VERSION is the version of this quickstream software project
as we define it from the \c QS_MAJOR, \c QS_MINOR, and \c QS_EDIT.
*/
#define QUICKSTREAM_VERSION  (QS_STR(QS_MAJOR) "." QS_STR(QS_MINOR) "." QS_STR(QS_EDIT))


/** The signal number used to have libquickstream.so catch a signal and
then run quickstreamGUI; by running quickstreamGUI_attach.

It's like when you attach a debugger to your already running program.  In
this case the debugger is quickstreamGUI, and the program that invokes it
is quickstreamGUI_attach which exits after trying to add the
quickstreamGUI to the already running program.

If the running libquickstream.so program is already running with
quickstreamGUI than running quickstreamGUI_attach with do nothing.

Note: quickstreamGUI does not control the running of the program like a
debugger; i.e. it does not use the system call ptrace(2).
quickstreamGUI_attach just adds the quickstreamGUI code to the running
program using the system dynamic linker/loader, ld-linux.so, dlopen(3),
dlsym(3) and like functions.

We need to define this CPP (C preprocessor) macro in one place and that
place is currently here.
*/
#define QS_GUI_CONNECT_SIGNAL  SIGUSR2


#define QUICKSTREAM_URL  "Unset"


/** @} */ // endof \defgroup macros


#ifndef QS_EXPORT
#  define QS_EXPORT extern
#endif


// Gotten at libquickstream.so program start.
QS_EXPORT
const char *qsLibDir;

// Gotten at libquickstream.so program start.  The core block module
// directory.
QS_EXPORT
const char *qsBlockDir;


/** The list of built-in block modules composed at compile time */
QS_EXPORT
const char * const qsBuiltInBlocks[];

