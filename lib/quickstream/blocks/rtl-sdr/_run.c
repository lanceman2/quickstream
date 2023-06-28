// This file is linked with the librtlsdr libraries.  So, when this DSO
// file is loaded the linker/loader automatically loads the librtlsdr.so
// library and any library dependences that are not loaded already.


#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <rtl-sdr.h>

#include "head.h"

#include "../../../../include/quickstream.h"
#include "../../../debug.h"
#include "../../../SearchArray.h"

#include "common.h"

// Data that is shared between this DSO and the instance of the
// trlsdr.so DSO that caused this DSO to be loaded.
//
static struct Common *c = 0;

// Opaque pointer to a rts-sdr device thingy.  One per Block.
//
static rtlsdr_dev_t *dev = 0;

static int *gains = 0;
static int ngains;


// These librtlsdr.so API wrapper functions add an extra function call on
// the function call stack, but it's worth it especially given these will
// be relatively less rapid than the stream data which will sample at very
// high rates.  I just think of the calling rate of these functions is
// about 100000 times slower than the stream sample rate.  In most use
// causes it will be called less than once per minute; but I imagine it
// could easily go at 100 times a second.  In any case the bottle neck
// is not here.

static void SetFreq(double freq) {

    //DSPEW("freq=%lg", freq);

    uint32_t frequency = freq + 0.5;

    if(rtlsdr_set_center_freq(dev, frequency) < 0)
        WARN("Failed to set center freq.");
#if 0
    else
        DSPEW("Tuned to %" PRIu32 " Hz.", frequency);
#endif
}


static double GetFreq(void) {

    //DSPEW();
    return (double) rtlsdr_get_center_freq(dev);
}


static void SetRate(double rate) {

    uint32_t samp_rate = rate + 0.5;

    // This will put the sample rate to value that /usr/include/rtl-sdr.h
    // says it can handle:
    //
    if(samp_rate < 225001)
        samp_rate = 225001;
    else if(samp_rate < 300000)
        ;
    else if(samp_rate > 300000 && samp_rate < 600000)
        samp_rate = 300000;
    else if(samp_rate < 900001)
        samp_rate = 900001;
    else if(samp_rate > 3200000)
        samp_rate = 3200000;

    if(rtlsdr_set_sample_rate(dev, samp_rate) < 0)
	WARN("Failed to set sample rate");
#if 0
    else
        DSPEW("Sampling at %" PRIu32 " S/s", samp_rate);
#endif
}


static double GetRate(void) {

    //DSPEW();
    return (double) rtlsdr_get_sample_rate(dev);
}


static inline
void PrintGains(void) {

    fprintf(stderr, "Gains =");
    for(int i = 0; i < ngains; ++i)
        fprintf(stderr, " %d", gains[i]);
    fprintf(stderr, "\n");
}


#define BAD_GAIN  INT_MAX

// Return true on error.
static bool GetGains(void) {

    ngains = rtlsdr_get_tuner_gains(dev, 0);
    if(ngains <= 0) {
        WARN("Failed get gain values.");
        return true;
    }
    gains = malloc(ngains * sizeof(*gains));
    ASSERT(gains, "malloc(%zu) failed", ngains * sizeof(*gains));
    int nGains = rtlsdr_get_tuner_gains(dev, gains);
    ASSERT(nGains == ngains);
    ASSERT(ngains >= 2);

PrintGains();
    // sort gains in increasing order like 1 2 3 4
    // They should be sorted already, but are they?
    //
    // Bubble sort is good enough, given I think it's sorted already.
    {
        int j = ngains - 1;
        while(j) {
            int k = 0;
            for(int i = 0; i < j; i++) {
                j = 0;
                if(gains[i] > gains[i+1]) {
                    // Switch them.
                    int x = gains[i];
                    gains[i] = gains[i+1];
                    gains[i+1] = x;
                    k = i;
                }
            }
            j = k;
        }
PrintGains();

        // Make sure there are no duplicates.
        j = 0;
        for(int i = 1; i < ngains; ++i, ++j) {
            while(gains[j] == gains[i] && i < ngains)
                ++i;
            if(j != i - 1)
                gains[j+1] = gains[i];
        }
        ngains = j + 1;
    }
PrintGains();
    return false;
}


static inline int FindNearestGain(int gain) {

    if(rtlsdr_set_tuner_gain_mode(dev, 1) < 0) {
        WARN("Failed to enable manual gain.");
	    return BAD_GAIN;
    }

    c->autoGain = false;

    if(!gains) {
        if(GetGains())
            return BAD_GAIN;
    }

    // Find the closest gain.
    //
    // We assume that the gains[] array has an ascending order.
    //
    return FindNearestInArray(gain/*key*/, gains/*array[]*/,
            ngains/*arrayLen*/);
}


static void SetGain(double gain_in) {

    int gain = (10.0 * gain_in) + 0.5;

    gain = FindNearestGain(gain);

    //WARN("Nearest Gain: %d", gain);

    if(gain == BAD_GAIN) return;

    if(rtlsdr_set_tuner_gain(dev, gain) != 0) {
        WARN("Failed to set tuner gain");
        return;
    }

    //DSPEW("Tuner gain set to %0.2f dB.", gain/10.0);
}


static double GetGain(void) {

    //DSPEW();

    double gain = rtlsdr_get_tuner_gain(dev);

    return gain/10.0;
}


// This is on rtlsdr_get_tuner_gain_mode() so we store this:
static bool autoGain;


static void SetAutoGain(bool autoGain_in) {

    if(rtlsdr_set_tuner_gain_mode(dev, autoGain_in?0:1) < 0) {
        WARN("Failed to enable manual gain.");
	    return;
    }

    autoGain = autoGain_in;

    DSPEW("autoGain=%d", autoGain);
}


static double GetAutoGain(void) {

    //DSPEW();
    return autoGain;
}



// construct() is called first; so we set c here.
//
int construct(struct Common *common) {

    DSPEW();
    DASSERT(c == 0);
    c = common;
    DASSERT(c->setFreq == 0);

    // Setup the functions that we need in rtlsdr.c.
    //
    c->setFreq = SetFreq;
    c->getFreq = GetFreq;
    c->setRate = SetRate;
    c->getRate = GetRate;
    c->setGain = SetGain;
    c->getGain = GetGain;
    c->setAutoGain = SetAutoGain;
    c->getAutoGain = GetAutoGain;

    return 0;
}


static size_t currentOutputMax;


int start(uint32_t numInputs, uint32_t numOutputs,
        struct Common *common) {

    DSPEW("c->freq=%lg", c->freq);

    errno = 0;
    int ret = rtlsdr_open(&dev, c->dev_index);
    if(ret < 0) {
        dev = 0;
        ERROR("rtlsdr_open(%p,0) failed", &dev);
        return -1;
    }

    SetRate(c->rate);
    SetFreq(c->freq);
    SetGain(c->gain);
#if 0
    if(0 > rtlsdr_set_freq_correction(dev, ppm_error)) {
        ERROR("rtlsdr_set_freq_correction(dev,) failed");
        return 1;
    }
#endif
    if(0 > rtlsdr_reset_buffer(dev)) {
        ERROR("rtlsdr_reset_buffer(dev) failed");
        return 1;
    }

    // We need an extra copy of this since it can change while the
    // stream is running and than does not take effect.  This value
    // of outputMax 
    currentOutputMax = c->outputMax;

    DSPEW();

    return 0; // success.
}


int flow(const void * const in[], const size_t inLens[], uint32_t numIn,
        void * const out[], const size_t outLens[], uint32_t numOut,
        struct Common *common) {

    if((*outLens) < currentOutputMax)
        // We want until we can write this promised amount of data.
        // rtlsdr_read_sync() seem to be happier with the same buffer
        // size every time.
        return 0;

    DASSERT((*outLens) == currentOutputMax);

    int n_read = 0;

    // TODO: We have found that this call sometimes hangs forever.
    // If that happens, re-plugging in the USB rtl-sdr device seems
    // to fix it.

    int r = rtlsdr_read_sync(dev, out[0], (uint32_t) currentOutputMax,
            &n_read);

    if(r < 0 || n_read < currentOutputMax) {
        if(r < 0)
            WARN("rtlsdr_read_sync() failed");
        else
            WARN("rtlsdr_read_sync() read %d bytes < "
                    "%zu bytes requested",
                    n_read, currentOutputMax);
        return 1;
    }

    qsAdvanceOutput(0, (size_t) n_read);

    return 0;
}


int stop(uint32_t numInputs, uint32_t numOutputs,
        struct Common *common) {

    // Let users put in other USB devices between runs.
    if(dev) {
        rtlsdr_close(dev);
        dev = 0;
    }
    if(gains) {
        free(gains);
        gains = 0;
    }

    DSPEW();

    return 0;
}
