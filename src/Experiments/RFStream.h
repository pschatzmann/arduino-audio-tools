#pragma once

#ifdef ESP32
#include <soc/rtc.h>
#include "driver/i2s.h"
#include "AudioTools/AudioStreams.h"
#include "AudioTools/Resample.h"
#include "AudioAnalog/AnalogAudio.h"
#include "AudioEffects/SoundGenerator.h"

namespace audio_tools {

#define MAX_SAMPLE_RATE 10006483

/**
 * @brief Modulation: for the time beeing only Amplitude Modulation (AM) is supported
 * 
 */
enum RFModulation {MOD_AM, MOD_FM, OUT_CARRIER_ONLY,  OUT_SIGNAL_ONLY,OUT_SIGNAL_RESAMPLED } ;

/**
 * @brief Configuration for RfStream
 */
struct RFConfig : public AnalogConfig {
    RFConfig(){
        sample_rate = 44100;
    }
    int rf_frequency = 835000;
    int output_channels = 1;
    int output_sample_rate = MAX_SAMPLE_RATE;
    RFModulation modulation = MOD_AM;
    ResampleScenario resample_scenario = UPSAMPLE;
    float fm_width = 100.0;
};


/**
 * @brief Radio Frequency Stream which uses AM Modulation of 835kHz (as default). The output is on the ESP32 Internal DAC pins
 * inspired by https://github.com/bitluni/ESP32AMRadioTransmitter
 * 
 */
class RfStream : public AnalogAudioStream {
public:

    RFConfig defaultConfig() {
        RFConfig def;
        return def;
    }

    bool begin(RFConfig cfg) {
        this->cfg = cfg;

        if (cfg.bits_per_sample!=16){
            return false;
        }

        if (cfg.channels>cfg.output_channels){
            LOGW("Combining channels to 1");
            reducer.setSourceChannels(cfg.channels);
            reducer.setTargetChannels(cfg.output_channels);
        } 

        // setup resampler
        resample_factor = cfg.output_sample_rate/cfg.sample_rate;
        resampler.begin(cfg.output_channels, resample_factor, cfg.resample_scenario);

        // setup analog output via i2s
        auto ocfg = AnalogAudioStream::defaultConfig();
        ocfg.setAudioInfo(cfg);
        ocfg.channels = cfg.output_channels;
        ocfg.sample_rate = cfg.sample_rate;
        AnalogAudioStream::begin(cfg);
    
        if (cfg.modulation!=OUT_SIGNAL_ONLY  && cfg.output_sample_rate>=10000000){
            LOGI("setting max sample rate");
            cfg.output_sample_rate = MAX_SAMPLE_RATE;
            setMaxSampleRate();

            // setup carrier
            float rate = carrier.setupSine(cfg.output_sample_rate,cfg.rf_frequency);
            LOGW("Effecting rf frequency: %d for requested %d",rate, cfg.rf_frequency);
            carrier.begin();
        }

        return true;
    }


    /// Writes the audio data to I2S
    virtual size_t write(const uint8_t *buffer, size_t size) override {
        if (resample_factor==0) return 0;

        // convert number of channels if necessary
        size_t bytes = size;
        if (cfg.channels>cfg.output_channels){
            LOGW("Combining channels to 1");
            bytes = reducer.convert((uint8_t*)buffer, size);
        }
        size_t input_samples = bytes / sizeof(int16_t); 

        int16_t* resampled_data16 = nullptr;
        size_t resampled_samples = bytes/2;
        size_t resampled_bytes = bytes;
        if (cfg.modulation != OUT_SIGNAL_ONLY){
            // resample buffer
            resampled_bytes = bytes*resample_factor;
            resampled_samples = resampled_bytes / 2;
            resampler.write(buffer, bytes);
            resampled_data16 = resampler.data();
        }
        
        out_data.resize(resampled_samples);

        switch(cfg.modulation){
            case MOD_AM:
                modulateAM(out_data, resampled_data16, resampled_samples);
                break;
            case OUT_CARRIER_ONLY:
                outputCarrierOnly(out_data, resampled_data16, resampled_samples);
                break;
            case OUT_SIGNAL_ONLY:
                outputSignalOnly(out_data, (int16_t*)buffer, resampled_samples);
                break;
            case OUT_SIGNAL_RESAMPLED:
                outputSignalOnly(out_data, resampled_data16, resampled_samples);
                break;

        }

#ifdef DEBUG_RFDATA
        int16_t* pt16 = (int16_t*) out_data.data();
        for (int j=0;j<resampled_samples;j++){
            Serial.println(pt16[j]);
        }
#endif
        // output to analog pins
        AnalogAudioStream::write((uint8_t*)out_data.data(), resampled_bytes);


        return size;
    }

    RFConfig &config(){
        return cfg;
    }

    inline void modulateAM(Vector<int16_t> &out_data, int16_t* resampled_data16, size_t resampled_samples ){
        // fm modulate
        for (uint32_t i=0; i<resampled_samples; i+=cfg.output_channels) {
            float carrier_sample = carrier.readSample();
            for (int ch = 0;ch<cfg.output_channels;ch++){
                out_data[i+ch] =  carrier_sample * resampled_data16[i+ch];
            }
        }
    }

    inline void outputCarrierOnly(Vector<int16_t> &out_data, int16_t* resampled_data16, size_t resampled_samples ){
        // fm modulate
        for (uint32_t i=0; i<resampled_samples; i+=cfg.output_channels) {
            float carrier_sample = carrier.readSample();
            for (int ch = 0;ch<cfg.output_channels;ch++){
                out_data[i+ch] =  carrier_sample*32000;
            }
        }
    }

    inline void outputSignalOnly(Vector<int16_t> &out_data, int16_t* resampled_data16, size_t resampled_samples ){
        // fm modulate
        for (uint32_t i=0; i<resampled_samples; i+=cfg.output_channels) {
            for (int ch = 0;ch<cfg.output_channels;ch++){
                out_data[i+ch] =  resampled_data16[i+ch];
            }
        }
    }

    int getCarrierFrequncy() {
        return cfg.rf_frequency;
    }

protected:
    RFConfig cfg;
    GeneratorFromArray<float> carrier;                // subclass of SoundGenerator with max amplitude of 32000
    ResampleBuffer<int16_t> resampler;
    Vector<int16_t> out_data{0};
    ChannelReducer<int16_t> reducer;
    AnalogAudioStream out; 
    int resample_factor = 0;

};

}

#endif