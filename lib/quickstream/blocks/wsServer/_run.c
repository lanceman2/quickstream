

#include "wsServer.h"

#include "../../../debug.h"
#include "../../../../include/quickstream.h"

// Note: currently the wsServer library is a static archive file, libws.a,
// not a DSO (dynamic shared object) library (like libws.so);  so this C
// file builds a DSO object with the wsServer functions and other wsServer
// symbols in it.  This DSO gets loaded with dlopen(3) in wsServer.so
// (source wsServer.c).


// This DSO (from compiling this file) is not a quickstream block.  It
// is loaded by the block compiled from wsServer.c

void remove_this_DUMMY_test_function(void) {

// Force libws.a to provide a function to the linked DSO.
ERROR("ws_sendframe=%p", ws_sendframe);
}

