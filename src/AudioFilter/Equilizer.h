#pragma once
#include <math.h>
#include "AudioConfig.h"
#include "AudioTools/AudioStreams.h"
#include "AudioTools/AudioOutput.h"

/** 
 * @defgroup equilizer Equilizer
 * @ingroup dsp
 * @brief Digital Equilizer  
**/

namespace audio_tools {

/**
 * @brief Configuration for 3 Band Equilizer: Set channels,bits_per_sample,sample_rate.  Set and update gain_low, gain_medium and gain_high to value between 0 and 1.0
 * @ingroup equilizer
 * @author pschatzmann
 */
struct ConfigEquilizer3Bands : public AudioInfo {
    ConfigEquilizer3Bands(){
        channels = 2;
        bits_per_sample = 16;
        sample_rate = 44100;
    }
    
    ConfigEquilizer3Bands(const ConfigEquilizer3Bands&) = delete;

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
 * @ingroup equilizer
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

        Equilizer3Bands(AudioOutput &out) {
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

        ConfigEquilizer3Bands &config() {
            return cfg;
        }


        ConfigEquilizer3Bands &defaultConfig() {
            return config();
        }

        bool begin(ConfigEquilizer3Bands &config){
            p_cfg = &config;
            if (p_cfg->channels>max_state_count){
                if (state!=nullptr) delete[]state;
                state = new EQSTATE[p_cfg->channels];
                max_state_count = p_cfg->channels;
            }

            // Setup state
            for (int j=0;j<max_state_count;j++){
                memset(&state[j],0,sizeof(EQSTATE));

                // Calculate filter cutoff frequencies
                state[j].lf = 2 * sin((float)PI * ((float)p_cfg->freq_low / (float)p_cfg->sample_rate));
                state[j].hf = 2 * sin((float)PI * ((float)p_cfg->freq_high / (float)p_cfg->sample_rate));
            }
            return true;
        }

        virtual void setAudioInfo(AudioInfo info) override {
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
        ConfigEquilizer3Bands cfg;
        ConfigEquilizer3Bands *p_cfg=&cfg;
        const float vsa = (1.0 / 4294967295.0);   // Very small amount (Denormal Fix)
        Print *p_print = nullptr; // support for write
        Stream *p_stream = nullptr; // support for write
        AudioOutput *p_out=nullptr; // support for write
        AudioStream *p_in=nullptr; // support for readBytes
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
                        for (size_t j=0; j<sample_count; j+=p_cfg->channels){
                            for (int ch=0; ch<p_cfg->channels; ch++){
                                //p_dataT[j+ch] = sample(state[ch], 1.0 / 32767.0 * p_dataT[j+ch]) * 32767;
                                p_dataT[j+ch] = toInt16(sample(state[ch], toFloat(p_dataT[j+ch])));
                            }
                        }
                    }
                    break;

                default: 
                    LOGE("Only 16 bits supported: %d", p_cfg->bits_per_sample);
                    break;
            }
        }

        /// convert float in the range -1 to 1 to a int16 and clip the values that are out of range
        inline int16_t toInt16(float v){
            float result = v * 32767.0f;
            // clip result
            if (result>32767){
                result = 32767;
            } else if (result<-32767){
                result = -32767;
            }
            return result;
        }

        /// convert float in the range -1 to 1 to a int16 and clip the values that are out of range
        inline float toFloat(int16_t v){
            return  static_cast<float>(v) / 32767.0f;
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