// This file is NOT linked with the librtlsdr libraries.
//
// This is the declarative part of the block.  The running part of the
// block is in _run.c.  The "running" part of the DSO is not loaded into
// the program until the stream starts flowing.  That's how quickstream
// can load part of a block but not all of it, so that users can build
// stream graphs while automatically verify the block without redundant
// configuration files.

#include <string.h>
#include <unistd.h>
#include <rtl-sdr.h>

#include "head.h"

#include "../../../../include/quickstream.h"
#include "../../../debug.h"

#include "common.h"


#define MAX_OUTPUTMAX      ((size_t) (256 * 16384))
// from rtl-sdr.h
#define MIN_OUTPUTMAX      ((size_t) (512))
// default buffer length in rtlsdr example code.
#define DEFAULT_OUTPUTMAX   262144


static struct Common c = {
    .freq = 105300000.0, // center frequency in Hz
    .rate = 2000000.0, // sample rate
    .gain = 48.0,

    .setFreq = 0,

    .outputMax = DEFAULT_OUTPUTMAX
};



// Data shared with _run.c:
//
// Note: there is a new instance of this data for each time this
// ./rtlsdr.so file is loaded.  libquickstream.so makes another copy for
// each one by copying this file and _run.so to temporary files.  Without
// making another copy this data would be the same for each instance of
// the block loaded.  We end up with a little penalty in that the read
// only process mappings are not shared between these two DSO copies (if
// we load 2), the gain is we can have static variables (that are not
// shared) and do not require as much dynamic memory allocation (if any at
// all).  The bigger gain maybe that dumber people may be able to write
// block modules.
//
// We added an option to disable this DSO load copy thing:
//
// struct QsBlockOptions options {
//    .disableDSOLoadCopy = true
// }
// 
//
// Yes, the run-time tmp files are automatically deleted; which may not be
// something you can not do on windoz (remove a file while it's running).
//
// Are there dl (linker/loader) options to load the same DSO without
// sharing as much between the copies?  I do not think so; and I do not
// want to write my own version of libdl.so.  I don't even know how
// libdl.so does not show in the listings from running "ldd
// quickstream"; maybe it's dynamically loaded at run-time, ha ha.
//
// We keep in mind the libraries like librtlsdr.so do not get other copies
// loaded into the running program, ldopen(3) automatically keeps just one
// loaded and keeps a reference count, so it can really unload it when
// that reference count goes to zero at the last closing dlclose(3).
//
//


#define STR(s)   XSTR(s)
#define XSTR(s)  #s




// amount we write the stream per run cycle (in bytes).
static
size_t outputTotal = 0;



static
struct QsParameter *freqGetter,
                   *rateGetter,
                   *gainGetter,
                   *autoGainGetter;
static
struct QsParameter *freqSetter,
                   *rateSetter,
                   *gainSetter,
                   *autoGainSetter;


static
char *OutputTotal_config(int argc, const char * const *argv,
        struct Common *common) {

    size_t newValue = qsParseSizet(outputTotal);

    if(outputTotal == newValue)
        return 0;

    outputTotal = newValue;

    DSPEW("Setting \"OutputTotal\" to %zu", outputTotal);
    return 0;
}


static
char *OutputMax_config(int argc, const char * const *argv,
        struct Common *common) {

    size_t newValue = qsParseSizet(c.outputMax);

    // Check limits of outputMax:
    if(newValue > MAX_OUTPUTMAX)
        newValue = MAX_OUTPUTMAX;
    else if(newValue < MIN_OUTPUTMAX)
        newValue = MIN_OUTPUTMAX;

    if(newValue == c.outputMax) {
        DSPEW("\"OutputMax\" was already set to %zu", c.outputMax);
        return 0; // done.
    }

    c.outputMax = newValue;

    DSPEW("Setting \"OutputMax\" to %zu", c.outputMax);

    // We will be writing out outputMax bytes for each stream write.
    qsSetOutputMax(0/*port*/, c.outputMax);

    return 0;
}


static
char *dev_index_config(int argc, const char * const *argv,
        struct Common *common) {

    qsParseUint32tArray(c.dev_index, &c.dev_index, 1);

    INFO("Setting \"dev_index\" to %" PRIu32, c.dev_index);

    return 0;
}


// This could be a CPP macro where (x) is (Freq) in this case.  Note this
// code pattern is repeated 4 times below with (x) being Freq, Rate, Gain,
// and AutoGain.
//
static
int Freq_setter(const struct QsParameter *p, const double *value,
            uint32_t readCount, uint32_t queueCount,
            struct Common *common) {

    // Skip all but the last value in the queue.
    if(readCount != queueCount) return 0;

    //DSPEW("freq=%lg", *value);

    if(c.setFreq) {
        double oldFreq = c.freq;
        c.setFreq(*value);
        c.freq = c.getFreq();
        if(c.freq != oldFreq)
            qsGetterPush(freqGetter, &c.freq);
        return 0;
    }

    // All we can do now is store it for now, since the libtrlsdr.so
    // is not loaded yet.
    //
    c.freq = *value;

    return 0;
}


static
int Rate_setter(const struct QsParameter *p, const double *value,
            uint32_t readCount, uint32_t queueCount,
            struct Common *common) {

    // Skip all but the last value in the queue.
    if(readCount != queueCount) return 0;

    if(c.setRate) {
        double oldRate = c.rate;
        c.setRate(*value);
        c.rate = c.getRate();
        if(c.rate != oldRate)
            qsGetterPush(rateGetter, &c.rate);
        return 0;
    }

    // All we can do now is store it for now, since the libtrlsdr.so
    // is not loaded yet.
    //
    c.rate = *value;

    return 0;
}


static
int Gain_setter(const struct QsParameter *p, const double *value,
            uint32_t readCount, uint32_t queueCount,
            struct Common *common) {

    // Skip all but the last value in the queue.
    if(readCount != queueCount) return 0;

    if(c.setGain) {
        double oldGain = c.gain;
        c.setGain(*value);
        c.gain = c.getGain();
        if(c.gain != oldGain)
            qsGetterPush(gainGetter, &c.gain);
        return 0;
    }

    // All we can do now is store it for now, since the libtrlsdr.so
    // is not loaded yet.
    //
    c.gain = *value;

    return 0;
}


static
int AutoGain_setter(const struct QsParameter *p, const double *value,
            uint32_t readCount, uint32_t queueCount,
            struct Common *common) {

    // Skip all but the last value in the queue.
    if(readCount != queueCount) return 0;

    if(c.setAutoGain) {
        bool oldAutoGain = c.autoGain;
        c.setAutoGain(*value);
        c.autoGain = c.getAutoGain();
        if(c.autoGain != oldAutoGain)
            qsGetterPush(autoGainGetter, &c.autoGain);
        return 0;
    }

    // All we can do now is store it for now, since the libtrlsdr.so
    // is not loaded yet.
    //
    c.autoGain = *value;

    return 0;
}



int declare(void) {

    qsSetNumOutputs(1, 1);

#if 0
    // Test rtl-sdr.h without linking to librtlsdr
    enum rtlsdr_tuner tuner = RTLSDR_TUNER_E4000;
ERROR("tuner=%d", tuner);
#endif

    // This will delay loading of librtlsdr.so until the just before the
    // stream starts.
    //
    // I wonder if unloading this block after running the stream will find
    // resource leaks in librtlsdr.so (and other libraries).  How robust
    // is librtlsdr.so ?
    //
    qsAddRunFile("_run.so", true/*true => load DSO copy*/);

    qsSetUserData(&c);

    qsSetOutputMax(0/*port*/, c.outputMax);

    freqSetter =
        qsCreateSetter("freq", sizeof(c.freq),
            QsValueType_double, 0/*0 == no initial value*/,
            (int (*)(const struct QsParameter *p, const void *value,
                uint32_t readCount, uint32_t queueCount, void *userData))
                Freq_setter);

    freqGetter = qsCreateGetter(
            "freq", sizeof(c.freq), QsValueType_double,
            0/*initValue*/);


    rateSetter =
        qsCreateSetter("rate", sizeof(c.rate),
            QsValueType_double, 0/*0 == no initial value*/,
            (int (*)(const struct QsParameter *p, const void *value,
                uint32_t readCount, uint32_t queueCount, void *userData))
                Rate_setter);

    rateGetter = qsCreateGetter(
            "rate", sizeof(c.rate), QsValueType_double,
            0/*initValue*/);


    gainSetter =
        qsCreateSetter("gain", sizeof(c.gain),
            QsValueType_double, 0/*0 == no initial value*/,
            (int (*)(const struct QsParameter *p, const void *value,
                uint32_t readCount, uint32_t queueCount, void *userData))
                Gain_setter);

    gainGetter = qsCreateGetter(
            "gain", sizeof(c.gain), QsValueType_double,
            0/*initValue*/);


    autoGainSetter =
        qsCreateSetter("autoGain", sizeof(c.autoGain),
            QsValueType_bool, 0/*0 == no initial value*/,
            (int (*)(const struct QsParameter *p, const void *value,
                uint32_t readCount, uint32_t queueCount, void *userData))
                AutoGain_setter);

    autoGainGetter = qsCreateGetter(
            "autoGain", sizeof(c.autoGain), QsValueType_bool,
            0/*initValue*/);

    qsAddConfig((char *(*) (int, const char * const *,
                void *)) OutputTotal_config, "OutputTotal",
            "Total bytes read and written per stream run cycle."
            "  0 is for infinite output",
            "OutputTotal NUM_BYTES",
            "OutputTotal 0");

    qsAddConfig((char *(*) (int, const char * const *,
                void *)) OutputMax_config, "OutputMax",
            "Something like the maximum bytes requested for"
            " each rtl-sdr read",
            "OutputMax NUM_BYTES",
            "OutputMax " STR(DEFAULT_OUTPUTMAX));

    qsAddConfig((char *(*) (int, const char * const *,
                void *)) dev_index_config, "dev_index",
            "The rtlsdr device index used in rtlsdr_open(), from 0 up",
            "dev_index NUM",
            "dev_index 0");

    DSPEW();
    return 0;
}


int undeclare(struct Common *common) {


   return 0;
}
