#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <string>

#include "../../../../include/quickstream/block.h"
#include "../../../../lib/debug.h"

#ifndef __cplusplus
// This is C++ code so we can more easily use libuhd.
#error "This is not a C++ compiler"
#endif


// We did not use the quickstream C++ block module interface.  I'm just
// not a fan of C++, and it's dependency hell, complexity, long compile
// times, super verbose code, more code lines, OOP bull shit, be an object
// or die mentality, impossible to scope code, non-native loader, valgrind
// can't debug, and forever growing syntax.
//
// The dependences of libUDH are fucking ridiculously large.  How much
// code should it take to read and write to a UDP socket?  They, pretty
// much, fucked it.  Don't get me started on all those fucking libUHD
// threads.  It's bloated in so many ways.  I have not seen a libUHD
// program that is capable of restart, you pretty much have to kill the
// process to get a new UHD Tx or Rx object.  That's my running model, in
// my head, of libUHD.  Lets hope this changes for the better with time.
//
// libUDH is not robust code:
//
// Setting some of the tx parameters at run-time can crash the libUHD code
// (maybe it's a hardware problem), so it may be best not to adjust
// bandwidth at run-time and instead output to a filter that can adjust
// the bandwidth via re-sampling using software in a down-stream filter.
// Just peg the sample rate (bandwidth) as high as it will go and
// down-sample in a downstream filter.  The cost is more computer system
// resources for a more robust program.
//
// Another "great" feature of libUHD is their are built-in wrong things
// you can do.  Depreciated code and interfaces that is not documented or
// marked as such.  At some point you just "poke and hope."  I rant to
// three: Remove the fucking depreciated examples from the source code or
// at least mark them as such in the example code files.
//
// This is C++ code so we can more easily use libuhd, but we are using the
// quickstream C API as the interface to this block module; hence we
// declare the compiled interface to be "C" like so:
//
extern "C" {



int declare(void) {

    qsAddRunFile("uhd/txRun.so", 0);


    return 0;
}



} // extern "C" {
