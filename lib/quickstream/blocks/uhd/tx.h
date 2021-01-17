
#ifndef __cplusplus
// This is C++ code so we can more easily use libuhd.
#error "This is not a C++ compiler"
#endif


extern "C" {

struct TxParameters {

    struct QsParameter *getFreq;
    struct QsParameter *setFreq;

};


extern
struct TxParameters txParameters;

}
