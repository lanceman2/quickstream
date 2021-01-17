#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <string>

#include <uhd/usrp/multi_usrp.hpp>

#include "../../../../include/quickstream/block.h"
#include "../../../../lib/debug.h"

#include "tx.h"

// We did not use the quickstream C++ block module interface.  I'm just
// not a fan of C++, and it's dependency hell, complexity, long compile
// times, super verbose code, more code lines, OOP bull shit, be an object
// of die mentality, impossible to scope code, non-native loader, valgrind
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
// the bandwidth via re-sampling using software in the down-stream filter.
// Just peg the sample rate (bandwidth) as high as it will go and
// down-sample in a downstream filter.  The cost is more computer system
// resources for a more robust program.
//
// Another "great" feature of libUHD is they are built in wrong things you
// can do.  Depreciated code and interfaces that is not documented or
// marked as such.  At some point you just "poke and hope."
//
// This is C++ code so we can more easily use libuhd, but we are using the
// quickstream C API as the interface to this block module; hence we
// declare the compiled interface to be "C" like so:
//
extern "C" {



// These will store values from before start() to initialize in start().
// Not to be confused with what the current values are at running time.
// They are just for initialize in start() that may be gotten from the
// command line, or whatever.
static std::vector<double> freq, rate, gain;


static std::string uhd_args, subdev;
static std::vector<size_t> channels; // USRP channels


// Stupid libuhd state; they spill into three objects that we need to
// keep, because they are stupid-heads.
//
static uhd::usrp::multi_usrp::sptr usrp;
static uhd::tx_streamer::sptr tx_stream;
static uhd::tx_metadata_t tx_metadata;

int construct(void) {

    DSPEW();

    return 0;
}


int start(uint32_t numInputs, uint32_t numOutputs) {


    return 0;
}


int flow(void *buffers[], const size_t lens[],
            uint32_t numInputs, uint32_t numOutputs) {


    return 1;
}


int stop(uint32_t numInputs, uint32_t numOutputs) {


    return 0;
}



} // extern "C" {
