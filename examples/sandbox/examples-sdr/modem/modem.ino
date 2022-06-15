#include "AudioTools.h"
#include "AudioLibs/AudioKit.h"
#include <liquid.h>

#include "AudioTools.h"
#include "AudioLibs/AudioKit.h"
#include <liquid.h>

// input parameters
unsigned int    bps         = 1;        // number of bits/symbol
float           h           = 0.5f;     // modulation index (h=1/2 for MSK)
unsigned int    k           = 2;        // filter samples/symbol
unsigned int    m           = 3;        // filter delay (symbols)
float           beta        = 0.35f;    // GMSK bandwidth-time factor
float           SNRdB       = 40.0f;    // signal-to-noise ratio [dB]
int             filter_type = LIQUID_CPFSK_SQUARE;

unsigned int nfft        =   1200;  // spectral periodogram FFT size
float        fc          =   0.20f; // carrier (relative to sampling rate)


// message
char message[80];
int counter = 0;

// encoding/decoding 
cpfskmod mod; 
cpfskdem dem; 

// complex number conversion    
iirfilt_crcf   filter_tx;
nco_crcf       mixer_tx;
nco_crcf       mixer_rx;
iirfilt_crcf   filter_rx;

// audio
AudioKitStream i2s; // or replace with I2SStream
int16_t encoded[80];


void setup() {
    Serial.begin(115200);
    AudioLogger::instance().begin(Serial, AudioLogger::Info);
        
    // create modem objects
    mod = cpfskmod_create(bps, h, k, m, beta, filter_type);
    dem = cpfskdem_create(bps, h, k, m, beta, filter_type);

    // complex number conversion
    filter_tx    = iirfilt_crcf_create_lowpass(15, 0.05);
    mixer_tx     = nco_crcf_create(LIQUID_VCO);
    mixer_rx     = nco_crcf_create(LIQUID_VCO);
    filter_rx    = iirfilt_crcf_create_lowpass(7, 0.2);

    // set carrier frequencies
    nco_crcf_set_frequency(mixer_tx, fc * 2*M_PI);
    nco_crcf_set_frequency(mixer_rx, fc * 2*M_PI);

    // setup audio
    auto cfg = i2s.defaultConfig(RXTX_MODE);
    cfg.channels = 1;
    cfg.input_device = AUDIO_HAL_ADC_INPUT_LINE2;
    i2s.begin(cfg);
    
}


void loop() {
    // generate message
    snprintf(message, 80,  "This is message number %d.", ++counter);
    int len = strlen(message);

    // modulate signal
    std::complex<float>  v1; 
    for (int i=0;i<len;i++){
        // modulate
        cpfskmod_modulate(mod, message[i], &v1);
        // convert complex to real number
        iirfilt_crcf_execute(filter_tx, v1, &v1);
        nco_crcf_mix_up(mixer_tx, v1, &v1);
        float v2 = v1.real();
        nco_crcf_step(mixer_tx);
        encoded[i] = v2 * 32766;
    }
    i2s.write((uint8_t*)encoded, len*k );

    // demodulate signal
    i2s.readBytes((uint8_t*)encoded, len*k );
    std::complex<float> v3;
    for (int i=0; i<len; i++) {
        // convert real number to complex number
        float v2 = static_cast<float>(encoded[i]) / 32767.0;
        nco_crcf_mix_down(mixer_rx, v2, &v3);
        iirfilt_crcf_execute(filter_rx, v3, &v3);
        nco_crcf_step(mixer_rx);
        // decode complex number
        message[i] = cpfskdem_demodulate(dem, &v3);
    }
    message[len+1]=0;

    // print demodulated signal
    Serial.print("result: ");
    Serial.println(message);

}