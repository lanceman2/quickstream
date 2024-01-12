
#include "../../../../include/quickstream.h"
#include "../../../debug.h"

// TODO: Use volk library to use SIMD; for now just get it working.

// TODO: vary MAX_INPUTS so we can convert more I/Q channels.
// Care is required to do that ...
#define MAX_INPUTS 1
#define MIN_INPUTS MAX_INPUTS


// 2000000.0 works for block from source file ../rtl-sdr/rtlsdr.c
//
#define DEFAULT_SAMP_RATE (2000000.0) // sample rate


static float sample_rate = DEFAULT_SAMP_RATE;
static float phi_0 = 0.0F;

static
int SampleRate_setter(const struct QsParameter *p, const float *value,
            uint32_t readCount, uint32_t queueCount,
            void *userData) {

    // Skip all but the last value in the queue.
    if(readCount != queueCount) return 0;

    // TODO: check the range of the value.
    sample_rate = *value;
    return 0;
}


static
int Phi0_setter(const struct QsParameter *p, const float *value,
            uint32_t readCount, uint32_t queueCount,
            void *userData) {

    // Skip all but the last value in the queue.
    if(readCount != queueCount) return 0;

    // TODO: check the range of the value.
    phi_0 = *value;

    return 0;
}




int declare(void) {

    // TODO: add more constant parameters

    qsSetNumInputs(MIN_INPUTS, MAX_INPUTS);
    qsSetNumOutputs(MIN_INPUTS, MAX_INPUTS);

    qsCreateSetter("sample_rate", sizeof(sample_rate),
        QsValueType_float, &sample_rate/*0 == no initial value*/,
        (int (*)(const struct QsParameter *p, const void *value,
            uint32_t readCount, uint32_t queueCount, void *userData))
            SampleRate_setter);

    qsCreateSetter("phi_0", sizeof(phi_0),
        QsValueType_float, &phi_0/*0 == no initial value*/,
        (int (*)(const struct QsParameter *p, const void *value,
            uint32_t readCount, uint32_t queueCount, void *userData))
            Phi0_setter);

    return 0;
}


// Go from 2 dimensions to 1 dimension, that is
// 2 D to 1 D.
//
/*
    Input I, Q


parameter: \phi_0 is the initial carrier phase at t=0 (in radians)
parameter: sample_rate in Hz (samples/second)

    time = t (in seconds)

    sample number count = n

    carrier frequency = freq (in Hz)

    We define carrier phase as: theta = \phi_0 + \2 * Pi * freq * t
    t = n / sample_rate


    We keep the carrier phase bound to a finite interval.

    Output = I * cos(theta) + Q * sin(theta)

ref:
https://www.sciencedirect.com/topics/engineering/carrier-phase
http://107.191.96.171/blogging/SDR_Tutor/IQ_variables.html


*/
// Input complex float (2 D)
// Output float        (1 D)
//
int flow(const void * const in[], const size_t inLens[], uint32_t numIn,
        void * const out[], const size_t outLens[], uint32_t numOut,
        void *userData) {

    size_t n = inLens[0]/(2*sizeof(float));
    if(!n)
        return 0;
    if(outLens[0]/sizeof(float) < n)
        n = outLens[0]/sizeof(float);
    if(!n)
        return 0;

    qsAdvanceInput(0, 2*n*sizeof(float));
    qsAdvanceOutput(0, n*sizeof(float));

    for(; n ; n--) {

        //float 


    }


ERROR("WRITE MORE CODE HERE ----------------------------------");
return 1;
}
