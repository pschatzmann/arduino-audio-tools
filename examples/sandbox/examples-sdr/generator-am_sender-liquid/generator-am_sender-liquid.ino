/**
 * @file generator-am_sender.ino
 * @author Phil Schatzmann
 * @brief We build a AM radio with the help of the liquid dsp library
 * The audio data is resampled up to the carrier sample rate and then modulated with the carrier frequency
 * @version 0.1
 * @date 2022-06-14
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include "AudioTools.h"
#include "liquid.h"

// Amplitude Modulation options
float        mod_index          = 0.8f;     // modulation index (bandwidth)
liquid_ampmodem_type type       = LIQUID_AMPMODEM_USB;
bool         suppressed_carrier = false;
ampmodem     mod ;

// Resample
constexpr float r    = 12989500.0 / 44100.0;   // resampling rate (output/input) = 313
resamp_crcf  q;


// Signals
const unsigned int num_samples = 10;
constexpr unsigned int y_len = (unsigned int) ceilf(1.1 * num_samples * r) + 4;
Vector<std::complex<float>> x{0};
Vector<std::complex<float>> y{0};
Vector<std::complex<float>> y_mod{0};
Vector<int16_t>             z{0};

// Audio
uint16_t sample_rate=44100;
uint8_t channels = 1;                                      // The stream will have 2 channels 
SineWaveGenerator<int16_t> sineWave(32000);                // subclass of SoundGenerator with max amplitude of 32000
AnalogAudioStream out; 


void setup() {
    Serial.begin(115200);
    AudioLogger::instance().begin(Serial, AudioLogger::Info);

    // allocate memory
    x.resize(num_samples);
    y.resize(y_len);
    y_mod.resize(y_len);
    z.resize(y_len);

    // create mod/demod objects
    mod   = ampmodem_create(mod_index, type, suppressed_carrier);

    // setup resampler
    q = resamp_crcf_create_default(r);

    // setup test tone
    auto cfgs = sineWave.defaultConfig();
    cfgs.sample_rate = sample_rate;
    cfgs.channels = channels;
    sineWave.begin(cfgs);

    // setup analog i2s
    auto cfg = out.defaultConfig(TX_MODE);
    cfg.channels = 1;
    cfg.bits_per_sample = 16;
    out.begin(cfg);
    out.setMaxSampleRate();
}

void loop() {
    // fill input buffer with audio data
    for (uint32_t i=0; i<num_samples; i++) {
        // generate 'audio' signal from sine wave 
        x[i] =  static_cast<float>(sineWave.readSample())/32766 + 0i;
    }

    // execute on block of samples
    uint32_t ny;
    for (uint32_t i=0; i<num_samples; i++) {
        // resample
        resamp_crcf_execute_block(q, x.data(), num_samples, y.data(), &ny);
        
        // modulate signal
        for (uint32_t j=0; j<ny; j++){     
            ampmodem_modulate(mod, y[i].real(), &y_mod[i]);
            // convert complex to simple value for output and amplify 
            z[i] = (y_mod[i].real() * y_mod[i].real() + y_mod[i].imag() *  y_mod[i].imag()) * 10000.0 ;
        }
        // output to analog pins
        out.write((uint8_t*)z.data(), ny*sizeof(int16_t));
    }

}