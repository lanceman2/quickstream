// In this file data structures and other interfaces that are
// shared between the wsServer.so block and _run.so.

// We brake from our standard of not including include files in our
// private include files, because we have a problem here with CPP (C
// preprocessor) marco DEBUG getting redefined in the file ws.h.


// We have a CPP macro name-space pollution problem here.
//
// Stupid ws.h defines DEBUG and so does this when compiled with compiler
// option flag -DDEBUG.  We can only hope that ws.h does not intend to use
// the CPP macro DEBUG from outside of that file, like in a wsServer CPP
// macro function that we may be using here in this file.  We'll find out
// if this is not okay when the wsServer functions fail to work.
//
// Exporting a CPP macro into a C API without a unique prefix is not
// a good idea.  This is what can happen...
//
// I think it is "bad form" for a C API to define a CPP macro that is such
// a common name, DEBUG.  Does quickstream do something so bad (in
// quickstream.h)?  I need to check that.  I don't think it does...
//
// Here's the fix we choose:
//
// --- BEGIN FIX
#ifdef DEBUG
#  define zzxxNEED_DEBUGg
#  undef DEBUG
#endif
// Now we can load ws.h without a CPP macro redefine compiler error.

#include <ws.h>

#ifdef zzxxNEED_DEBUGg
// Because we got here we know that DEBUG was set by quickstream too.
// Undo what we did a few lines above.
//
// We don't assume that ws.h set DEBUG so we check if it did.
#  ifdef DEBUG
#    undef DEBUG
#  endif
#  undef zzxxNEED_DEBUGg
// Reset DEBUG to the empty value that quickstream uses:
#  define DEBUG
#endif
// It's amazing how complicated CPP conditionals get for something this
// simple.
// --- END FIX


struct wsServer {

    struct ws_server server;

    // We could add a lot more server configuration stuff here:
};

