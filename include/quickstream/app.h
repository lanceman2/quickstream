#ifndef __qsapp_h__
#define __qsapp_h__

/** \page app quickstream application objects
 
  The libquickstream application application programming interface (API)
 
  The app object is the top level programming object of quickstream.
  We need to make an app to run a quickstream program.  We use an
  app object with a compiled C or C++ program that has main() in it.
  App is just a word chosen to be the name of the object that is the
  highest level construct the quickstream API.  quickstream users that
  are writing plugin block modules do not need to access app objects.

 */

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>

// We wanted there to be one source to this important QUICKSTREAM_VERSION
// string.  This string must be changed to each release of the quickstream
// software package.  All other files must get this value from here.  When
// using GNU autotools, somehow configure.ac must get this string from
// here.  bootscrap can get it from here.
//
// We avoided having a app.h.in file.  We had that in the first iteration
// of quickstream and it was a pain.
//
#define QS_MAJOR  0
#define QS_MINOR  0
#define QS_EDIT   3

//
#define STR(s) XSTR(s)
#define XSTR(s) #s

#define QUICKSTREAM_VERSION  (STR(QS_MAJOR) ":" STR(QS_MINOR) ":" STR(QS_EDIT))




struct QsApp;


extern
struct QsApp *qsAppCreate(void);


extern
void qsAppDestroy(struct QsApp *app);


#endif // #ifndef __qsapp_h__
