#pragma once

#ifdef ESP32
#include <soc/rtc.h>
#include "driver/i2s.h"
#include "AudioTools/AudioStreams.h"
#include "AudioTools/Resample.h"
#include "AudioAnalog/AnalogAudio.h"
#include "AudioEffects/SoundGenerator.h"

namespace audio_tools {

/**
 * @brief Modulation: for the time beeing only Amplitude Modulation (AM) is supported
 * 
 */
enum RFModulation {MOD_AM, MOD_FM, MOD_CARRIER_ONLY, MOD_SIGNAL_ONLY};

/**
 * @brief Configuration for RfStream
 */
struct RFConfig : public AnalogConfig {
    RFConfig(){
        sample_rate = 44100;
    }
    int rf_frequency = 835000;
    int output_channels = 1;
    int output_sample_rate = 13000000;
    RFModulation modulation = MOD_AM;
    float fm_width = 100.0;
};


/**
 * @brief RFStream which uses AM Modulation of 835kHz. The output is on the ESP32 Internal DAC pins
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

        reducer.setSourceChannels(cfg.channels);
        reducer.setTargetChannels(cfg.output_channels);

        // setup resampler
        resample_factor = cfg.output_sample_rate/cfg.sample_rate;
        if (p_resample!=nullptr){
            delete p_resample;
        }
        p_resample = new Resample<int16_t>(resampled_data, cfg.output_channels, resample_factor, UP_SAMPLE);

        // carrier tone
        auto cfgc = carrier.defaultConfig();
        cfgc.sample_rate = cfg.output_sample_rate;
        cfgc.channels = cfg.output_channels;
        carrier.begin(cfgc, cfg.rf_frequency);

        // setup analog output via i2s
        auto ocfg = AnalogAudioStream::defaultConfig();
        ocfg.setAudioInfo(cfg);
        ocfg.channels = cfg.output_channels;
        AnalogAudioStream::begin(cfg);
        if (cfg.output_sample_rate>=10000000){
            setMaxSampleRate();
        }

        return true;
    }


    /// Writes the audio data to I2S
    virtual size_t write(const uint8_t *buffer, size_t size) override {
        if (resample_factor==0 || p_resample==nullptr) return 0;

        // convert number of channels if necessary
        size_t bytes = size;
        if (cfg.channels>cfg.output_channels){
            bytes = reducer.convert((uint8_t*)buffer, size);
        }

        // resample buffer
        size_t resampled_bytes = bytes*resample_factor;
        resampled_data.resize(resampled_bytes);
        p_resample->write((uint8_t*)buffer, bytes);
        // resampling result
        int16_t* resampled_data16 = (int16_t*)resampled_data.data();
        resampled_bytes = p_resample->lastBytesWritten();
        size_t resampled_samples = resampled_bytes / sizeof(int16_t);
        out_data.resize(resampled_samples);

        switch(cfg.modulation){
            case MOD_AM:
                modulateAM(resampled_data16, resampled_samples);
                break;
            case MOD_FM:
                modulateFM(resampled_data16, resampled_samples);
                break;
            case MOD_CARRIER_ONLY:
                modulateCarrierOnly(resampled_data16, resampled_samples);
                break;
            case MOD_SIGNAL_ONLY:
                modulateSignalOnly(resampled_data16, resampled_samples);
                break;

        }

        // output to analog pins
        AnalogAudioStream::write((uint8_t*)out_data.data(), resampled_bytes);

        return size;
    }

    RFConfig &config(){
        return cfg;
    }

    inline void modulateAM( int16_t* resampled_data16, size_t resampled_samples ){
        // fm modulate
        for (uint32_t i=0; i<resampled_samples; i+=cfg.output_channels) {
            float carrier_sample = carrier.readSample();
            for (int ch = 0;ch<cfg.output_channels;ch++){
                out_data[i+ch] =  carrier_sample/32000.0 * resampled_data16[i+ch];
            }
        }
    }


    inline void modulateFM( int16_t* resampled_data16, size_t resampled_samples ){
        // fm modulate
        for (uint32_t i=0; i<resampled_samples; i+=cfg.output_channels) {
            float width = static_cast<float>(resampled_data16[i])/32767.0 * cfg.fm_width;
            carrier.setFrequency(getCarrierFrequncy() + width);
            float carrier_sample = carrier.readSample();
            for (int ch = 0;ch<cfg.output_channels;ch++){
                out_data[i+ch] =  carrier_sample;
            }
        }
    }

    inline void modulateCarrierOnly( int16_t* resampled_data16, size_t resampled_samples ){
        // fm modulate
        for (uint32_t i=0; i<resampled_samples; i+=cfg.output_channels) {
            float carrier_sample = carrier.readSample();
            for (int ch = 0;ch<cfg.output_channels;ch++){
                out_data[i+ch] =  carrier_sample*32000;
            }
        }
    }

    inline void modulateSignalOnly( int16_t* resampled_data16, size_t resampled_samples ){
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
    SineFromTable<int16_t> carrier{32000};                // subclass of SoundGenerator with max amplitude of 32000
    MemoryStream resampled_data;
    AnalogAudioStream out; 
    Resample<int16_t> *p_resample = nullptr;
    Vector<int16_t> out_data{0};
    ChannelReducer<int16_t> reducer;
    int resample_factor = 0;

};

}

#endif