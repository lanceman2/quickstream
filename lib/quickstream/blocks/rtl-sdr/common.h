
struct Common {

    double freq, // center frequency in Hz
       rate, // sample rate
       gain; // gain in dB
    bool autoGain;

    size_t outputMax;


    uint32_t dev_index;



    // These get set in function construct() of _run.c
    //
    // These are shared between _run.c and rtlsdr.c.
    //
    // _run.so provides a wrapper API that rtlsdr.c uses so as to not directly
    // use the librtlsdr.so API.  That makes it so that rtlsdr.c does not have
    // to link with librtlsdr.so at build-time.
    //
    // This totally looks like boilerplate code.  Very repetitive shit.
    //
    void (*setFreq)(double freq);

    double (*getFreq)(void);

    void (*setRate)(double rate);

    double (*getRate)(void);

    void (*setGain)(double gain);

    double (*getGain)(void);

    void (*setAutoGain)(bool autoGain);

    double (*getAutoGain)(void);
};
