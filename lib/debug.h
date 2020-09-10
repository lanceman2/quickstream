#ifndef __debug_h__
#define __debug_h__


/** This file provides some CPP (C pre processor) macro debug function:
 *
 *    ASSERT()
 *
 * and compile time conditionally:
 *
 *    ERROR() WARN() NOTICE() INFO() DSPEW() DASSERT()
 *
 * see details below.
 *
 */

///////////////////////////////////////////////////////////////////////////
// USER DEFINABLE SELECTOR MACROS make the follow macros come to life:
//
// DEBUG             -->  DASSERT()
//
// SPEW_LEVEL_DEBUG  -->  DSPEW() INFO() NOTICE() WARN() ERROR()
// SPEW_LEVEL_INFO   -->  INFO() NOTICE() WARN() ERROR()
// SPEW_LEVEL_NOTICE -->  NOTICE() WARN() ERROR()
// SPEW_LEVEL_WARN   -->  WARN() ERROR()
// SPEW_LEVEL_ERROR  -->  ERROR()
//
// always on is      --> ASSERT()
//
// If a macro function is not live it becomes a empty macro with no code.
//
//
// The ERROR() function will also set a string accessible through qsError()
//
//
///////////////////////////////////////////////////////////////////////////
//
// Setting SPEW_LEVEL_NONE with still have ASSERT() spewing and ERROR()
// will not spew but will still set the qsError string
//


/*
 * It's really not much code but is a powerful development
 * tool like assert.  See 'man assert'.
 *
 * CRTSFilter builders do not have to include this file in their
 * plugin modules.  It's pretty handy for developing/debugging.
 */
#include <stdbool.h>
#include <stdio.h>
#include <errno.h>


#ifdef __cplusplus
extern "C" {
#endif



#define SPEW_FILE stderr


// We would like to be able to just call DSPEW() with no arguments
// which can make a zero length printf format.
#pragma GCC diagnostic ignored "-Wformat-zero-length"


extern void qs_spew(int level, FILE *stream, int errn, const char *pre, const char *file,
        int line, const char *func, bool bufferIt, const char *fmt, ...)
#ifdef __GNUC__
        // check printf format errors at compile time:
        __attribute__ ( ( format (printf, 9, 10 ) ) )
#endif
        ;


extern void qs_assertAction(FILE *stream);



#  define _SPEW(level, stream, errn, bufferIt, pre, fmt, ... )\
     qs_spew(level, stream, errn, pre, __BASE_FILE__, __LINE__,\
        __func__, bufferIt, fmt, ##__VA_ARGS__)

#  define ASSERT(val, ...) \
    do {\
        if(!((bool) (val))) {\
            _SPEW(1, SPEW_FILE, errno, true, "ASSERT("#val") failed: ", "" __VA_ARGS__);\
            qs_assertAction(SPEW_FILE);\
        }\
    }\
    while(0)


///////////////////////////////////////////////////////////////////////////
// We let the highest verbosity macro flag win.
//


#ifdef SPEW_LEVEL_DEBUG // The highest verbosity
#  ifndef SPEW_LEVEL_INFO
#    define SPEW_LEVEL_INFO
#  endif
#endif
#ifdef SPEW_LEVEL_INFO
#  ifndef SPEW_LEVEL_NOTICE
#    define SPEW_LEVEL_NOTICE
#  endif
#endif
#ifdef SPEW_LEVEL_NOTICE
#  ifndef SPEW_LEVEL_WARN
#    define SPEW_LEVEL_WARN
#  endif
#endif
#ifdef SPEW_LEVEL_WARN
#  ifndef SPEW_LEVEL_ERROR
#    define SPEW_LEVEL_ERROR
#  endif
#endif

#ifdef SPEW_LEVEL_ERROR
#  ifdef SPEW_LEVEL_NONE
#    undef SPEW_LEVEL_NONE
#  endif
#else
#  ifndef SPEW_LEVEL_NONE
#    define SPEW_LEVEL_NONE // The default.
#  endif
#endif





#ifdef DEBUG
#  define DASSERT(val, ...) ASSERT(val, ##__VA_ARGS__)
#else
#  define DASSERT(val, ...) /*empty marco*/
#endif

#ifdef SPEW_LEVEL_NONE
#define ERROR(...) _SPEW(0, 0/*no spew stream*/, errno, true, "ERROR: ", "" __VA_ARGS__)
#else
#define ERROR(...) _SPEW(1, SPEW_FILE, errno, true, "ERROR: ", "" __VA_ARGS__)
#endif

#ifdef SPEW_LEVEL_WARN
#  define WARN(...) _SPEW(2, SPEW_FILE, errno, false, "WARN: ", "" __VA_ARGS__)
#else
#  define WARN(...) /*empty macro*/
#endif 

#ifdef SPEW_LEVEL_NOTICE
#  define NOTICE(...) _SPEW(3, SPEW_FILE, errno, false, "NOTICE: ", "" __VA_ARGS__)
#else
#  define NOTICE(...) /*empty macro*/
#endif

#ifdef SPEW_LEVEL_INFO
#  define INFO(...)   _SPEW(4, SPEW_FILE, 0, false, "INFO: ", "" __VA_ARGS__)
#else
#  define INFO(...) /*empty macro*/
#endif

#ifdef SPEW_LEVEL_DEBUG
#  define DSPEW(...)  _SPEW(5, SPEW_FILE, 0, false, "DEBUG: ", "" __VA_ARGS__)
#else
#  define DSPEW(...) /*empty macro*/
#endif



#ifdef __cplusplus
}
#endif

#endif // #ifndef __debug_h__
