#pragma once
#include <math.h>
#include "AudioTools/AudioOutput.h"
#include "AudioBasic/Int24.h"

namespace audio_tools {

/**
 * @brief Configuration for 3 Band Equilizer: Set channels,bits_per_sample,sample_rate.  Set and update gain_low, gain_medium and gain_high to value between 0 and 1.0
 * @author pschatzmann
 */
struct ConfigEquilizer3Bands : public AudioBaseInfo {
    ConfigEquilizer3Bands(){
        channels = 2;
        bits_per_sample = 16;
        sample_rate = 44100;
    }
    // Frequencies
    int freq_low=880;
    int freq_high=5000;
    
    // Gain Controls
    float  gain_low = 1.0;
    float  gain_medium = 1.0;
    float  gain_high = 1.0;
};

/**
 * @brief 3 Band Equilizer inspired from https://www.musicdsp.org/en/latest/Filters/236-3-band-equaliser.html
 * @author pschatzmann
 */
class Equilizer3Bands : public AudioStream {
    public:

        Equilizer3Bands(Print &out) {
            p_print = &out;
        }

        Equilizer3Bands(Stream &in) {
            p_stream = &in;
        }

        Equilizer3Bands(AudioPrint &out) {
            p_out = &out;
            p_print = &out;
            out.setNotifyAudioChange(*this);
        }

        Equilizer3Bands(AudioStream &stream) {
            p_in = &stream;
            p_stream = &stream;
            p_print = &stream;
            stream.setNotifyAudioChange(*this);
        }

        ~Equilizer3Bands(){
            if (state!=nullptr) delete[]state;
        }

        ConfigEquilizer3Bands defaultConfig() {
            ConfigEquilizer3Bands v;
            return v;
        }

        bool begin(ConfigEquilizer3Bands &config){
            p_cfg = &config;
            if (p_cfg->channels>max_state_count){
                if (state!=nullptr) delete[]state;
                state = new EQSTATE[p_cfg->channels];
                max_state_count = p_cfg->channels;
            }
            // Clear state
            memset(&state[0],0,sizeof(EQSTATE));

            // Calculate filter cutoff frequencies
            state[0].lf = 2 * sin(M_PI * ((float)p_cfg->freq_low / (float)p_cfg->sample_rate));
            state[0].hf = 2 * sin(M_PI * ((float)p_cfg->freq_high / (float)p_cfg->sample_rate));

            // setup state for all channels
            for (int j=1;j<p_cfg->channels;j++){
                state[j]=state[0];
            }
            return true;
        }

        virtual void setAudioInfo(AudioBaseInfo info) override {
            p_cfg->sample_rate = info.sample_rate;
            p_cfg->channels = info.channels;
            p_cfg->bits_per_sample = info.bits_per_sample;
            begin(*p_cfg);
        }

        size_t write(const uint8_t *data, size_t len) override {
            filterSamples(data, len);
            return p_print->write(data, len);
        }

        int availableForWrite() override {
            return p_print->availableForWrite();
        }

        /// Provides the data from all streams mixed together 
        size_t readBytes(uint8_t* data, size_t len) override {
            size_t result = 0;
            if (p_stream!=nullptr){
                result = p_stream->readBytes(data, len);
                filterSamples(data, len);
            }
            return result;
        }

        int available()  override {
            return p_stream!=nullptr ? p_stream->available():0;
        }


    protected:
        const float vsa = (1.0 / 4294967295.0);   // Very small amount (Denormal Fix)
        Print *p_print = nullptr; // support for write
        Stream *p_stream = nullptr; // support for write
        AudioPrint *p_out=nullptr; // support for write
        AudioStream *p_in=nullptr; // support for readBytes
        ConfigEquilizer3Bands *p_cfg=nullptr;
        int max_state_count=0;

        struct EQSTATE {
            // Filter #1 (Low band)
            float  lf;       // Frequency
            float  f1p0;     // Poles ...
            float  f1p1;
            float  f1p2;
            float  f1p3;

            // Filter #2 (High band)
            float  hf;       // Frequency
            float  f2p0;     // Poles ...
            float  f2p1;
            float  f2p2;
            float  f2p3;

            // Sample history buffer
            float  sdm1;     // Sample data minus 1
            float  sdm2;     //                   2
            float  sdm3;     //                   3

        } *state=nullptr;

        void filterSamples(const uint8_t *data, size_t len){
            switch(p_cfg->bits_per_sample){
                case 16: {
                        int16_t* p_dataT = (int16_t*)data;
                        size_t sample_count = len / sizeof(int16_t);
                        for (int j=0; j<sample_count; j+=p_cfg->channels){
                            for (int ch=0; ch<p_cfg->channels; ch++){
                                p_dataT[j+ch] = sample(state[ch], p_dataT[j+ch]);
                            }
                        }
                    }
                case 24: {
                        int24_t* p_dataT = (int24_t*)data;
                        size_t sample_count = len / sizeof(int24_t);
                        for (int j=0; j<sample_count; j+=p_cfg->channels){
                            for (int ch=0; ch<p_cfg->channels; ch++){
                                int32_t tmp_i = sample(state[ch],p_dataT[j+ch].toFloat());
                                p_dataT[j+ch] = tmp_i;
                            }
                        }
                    }
                case 32: {
                        int32_t* p_dataT = (int32_t*)data;
                        size_t sample_count = len / sizeof(int32_t);
                        for (int j=0; j<sample_count; j+=p_cfg->channels){
                            for (int ch=0; ch<p_cfg->channels; ch++){
                                p_dataT[j+ch] = (const int32_t)sample(state[ch], p_dataT[j+ch]);
                            }
                        }
                    }
            }

        }

        // calculates a single sample using the indicated state 
        float sample(EQSTATE &es, float sample) {
            // Locals
            float  l,m,h;      // Low / Mid / High - Sample Values
            // Filter #1 (lowpass)
            es.f1p0  += (es.lf * (sample   - es.f1p0)) + vsa;
            es.f1p1  += (es.lf * (es.f1p0 - es.f1p1));
            es.f1p2  += (es.lf * (es.f1p1 - es.f1p2));
            es.f1p3  += (es.lf * (es.f1p2 - es.f1p3));

            l          = es.f1p3;

            // Filter #2 (highpass)
            es.f2p0  += (es.hf * (sample   - es.f2p0)) + vsa;
            es.f2p1  += (es.hf * (es.f2p0 - es.f2p1));
            es.f2p2  += (es.hf * (es.f2p1 - es.f2p2));
            es.f2p3  += (es.hf * (es.f2p2 - es.f2p3));

            h          = es.sdm3 - es.f2p3;
            // Calculate midrange (signal - (low + high))
            m          = es.sdm3 - (h + l);
            // Scale, Combine and store
            l         *= p_cfg->gain_low;
            m         *= p_cfg->gain_medium;
            h         *= p_cfg->gain_high;

            // Shuffle history buffer
            es.sdm3   = es.sdm2;
            es.sdm2   = es.sdm1;
            es.sdm1   = sample;

            // Return result
            return(l + m + h);
        }

};

} // namespace