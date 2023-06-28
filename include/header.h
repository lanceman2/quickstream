#ifndef __QUICKSTREAM_H__
#define __QUICKSTREAM_H__

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>
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

