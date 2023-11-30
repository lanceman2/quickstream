// This is a generated file.
//
// This is the source to a quickstream Super Block module.

#include <string.h>

#include <quickstream.h>
#include <quickstream_debug.h>


struct QsBlockOptions options = { .type = QsBlockType_super };

// ERROR() will print a line number which is a big help.
#define FAIL()   do { ERROR(); return -1; } while(0)


///////////////////////////////////////////////////////////////////
//  Block Meta Data
///////////////////////////////////////////////////////////////////
const char *QS_quickstreamGUI =
    "cHJpbnQgZnJlcQBwcmludCBnYWluAGZyZXEgdGV4dABmNjRUb1N0cmluZzMyAGd0"
    "ayBiYXNlAHJ1bgBydW4gYnV0dG9uAGdhaW4gc2xpZGVyAGZyZXEgc2xpZGVyAHU4"
    "VG9GMzIAUVRfR05VcmFkaW9fc2luawBydGxzZHIAAAAAAAAAAAAAAAAAAHSZQAAA"
    "AAAAhJxA5AAAAAAAAAAAAAAAAPSfQAAAAAAALKBA5AAAAAAAAAAAAAAAABidQAAA"
    "AAAANqFA5AAAAAAAAAAAAAAAAPycQAAAAAAAvJ9A5AAAAAAAAAAAAAAAAO6hQAAA"
    "AAAA/JhA5AAAAAAAAAAAAAAAADikQAAAAAAAAJxA5AAAAAAAAAAAAAAAAOqjQAAA"
    "AAAANJlA5AAAAAAAAAAAAAAAAMSeQAAAAAAA/JhA5AAAAAAAAAAAAAAAALibQAAA"
    "AAAAAJlA5AAAAAAAAAAAAAAAAGigQAAAAAAA9JxA5AAAAAAAAAAAAAAAACCiQAAA"
    "AAAAOJxA5AAAAAAAAAAAAAAAAMycQAAAAAAAgJxA5AAAAAAAAAA=";

// A null terminated array of all metaData symbol suffixes above:
const char *qsMetaDataKeys[] = {
    "quickstreamGUI",
    0 /*0 null terminator*/
};
///////////////////////////////////////////////////////////////////


int declare(void) {

    ///////////////////////////////////////////////////////////////
    //    Load child blocks
    ///////////////////////////////////////////////////////////////

    if(!qsGraph_createBlock(0, 0, "rtl-sdr/rtlsdr.so", "rtlsdr", 0))
        FAIL();

    if(!qsGraph_createBlock(0, 0, "gnuradio-3.10.1/QT_GNUradio_sink.so", "QT_GNUradio_sink", 0))
        FAIL();

    if(!qsGraph_createBlock(0, 0, "stream_type_converters/u8ToF32.so", "u8ToF32", 0))
        FAIL();

    if(!qsGraph_createBlock(0, 0, "gtk3/slider.so", "freq slider", 0))
        FAIL();

    if(!qsGraph_createBlock(0, 0, "gtk3/slider.so", "gain slider", 0))
        FAIL();

    if(!qsGraph_createBlock(0, 0, "gtk3/button.so", "run button", 0))
        FAIL();

    if(!qsGraph_createBlock(0, 0, "runner/run.so", "run", 0))
        FAIL();

    if(!qsGraph_createBlock(0, 0, "gtk3/base.so", "gtk base", 0))
        FAIL();

    if(!qsGraph_createBlock(0, 0, "parameter_type_converters/f64ToString32.so", "f64ToString32", 0))
        FAIL();

    if(!qsGraph_createBlock(0, 0, "gtk3/text.so", "freq text", 0))
        FAIL();

    if(!qsGraph_createBlock(0, 0, "parameter_type_converters/f64ToStderr.so", "print gain", 0))
        FAIL();

    if(!qsGraph_createBlock(0, 0, "parameter_type_converters/f64ToStderr.so", "print freq", 0))
        FAIL();

    ///////////////////////////////////////////////////////////////
    //    Setup Port Aliases to child block ports
    ///////////////////////////////////////////////////////////////

    if(qsBlock_makePortAlias(0, "gtk base", "s", "show", "show GUI"))
        FAIL();

    ///////////////////////////////////////////////////////////////
    //    Configure child blocks
    ///////////////////////////////////////////////////////////////

    if(qsBlock_configVByName(0/*graph*/, "freq slider",
             "label", "Center Frequency",
             0/*null terminate*/))
        FAIL();

    if(qsBlock_configVByName(0/*graph*/, "freq slider",
             "attributes", "92", "92", "110", "110", "1e+06", "181", "0", "%1.1lf MHz", "105.3",
             0/*null terminate*/))
        FAIL();

    if(qsBlock_configVByName(0/*graph*/, "gain slider",
             "label", "Gain",
             0/*null terminate*/))
        FAIL();

    if(qsBlock_configVByName(0/*graph*/, "gain slider",
             "attributes", "0", "0", "50", "50", "1", "501", "0", "%1.1lf dB", "48",
             0/*null terminate*/))
        FAIL();

    if(qsBlock_configVByName(0/*graph*/, "run button",
             "label", "Run",
             0/*null terminate*/))
        FAIL();

    if(qsBlock_configVByName(0/*graph*/, "gtk base",
             "title", "RTL-SDR Radio with GNURadio GUI Sink",
             0/*null terminate*/))
        FAIL();

    if(qsBlock_configVByName(0/*graph*/, "f64ToString32",
             "format", "%lg Hz",
             0/*null terminate*/))
        FAIL();

    if(qsBlock_configVByName(0/*graph*/, "freq text",
             "label", "center frequency",
             0/*null terminate*/))
        FAIL();

    if(qsBlock_configVByName(0/*graph*/, "print gain",
             "format", "gain= %lg dB",
             0/*null terminate*/))
        FAIL();

    if(qsBlock_configVByName(0/*graph*/, "print freq",
             "format", "freq=%lg Hz",
             0/*null terminate*/))
        FAIL();

    ///////////////////////////////////////////////////////////////
    //    Connect child blocks
    ///////////////////////////////////////////////////////////////

    if(qsGraph_connectByStrings(0, "freq slider", "g", "value", "rtlsdr", "s", "freq"))
        FAIL();

    if(qsGraph_connectByStrings(0, "rtlsdr", "s", "freq", "print freq", "s", "value"))
        FAIL();

    if(qsGraph_connectByStrings(0, "gain slider", "g", "value", "rtlsdr", "s", "gain"))
        FAIL();

    if(qsGraph_connectByStrings(0, "u8ToF32", "o", "0", "QT_GNUradio_sink", "i", "input"))
        FAIL();

    if(qsGraph_connectByStrings(0, "rtlsdr", "o", "0", "u8ToF32", "i", "0"))
        FAIL();

    if(qsGraph_connectByStrings(0, "run button", "g", "value", "run", "s", "run"))
        FAIL();

    if(qsGraph_connectByStrings(0, "rtlsdr", "g", "freq", "f64ToString32", "s", "value"))
        FAIL();

    if(qsGraph_connectByStrings(0, "f64ToString32", "g", "value", "freq text", "s", "value"))
        FAIL();

    if(qsGraph_connectByStrings(0, "rtlsdr", "g", "gain", "print gain", "s", "value"))
        FAIL();

    ///////////////////////////////////////////////////////////////
    //    Maybe add some qsAddConfig() calls below here
    ///////////////////////////////////////////////////////////////


    return 0;
}
