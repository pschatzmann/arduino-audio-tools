#pragma once
#include "AudioConfig.h"
#include "maximilian.h"
#include "libs/maxiClock.h"

// Maximilian play function - return an array of 2 channels
void play(maxi_float_t *channels);//run dac! 
void play1(maxi_float_t *channels);//run dac! 
void play2(maxi_float_t *channels);//run dac! 

namespace audio_tools {

/**
 * @brief AudioTools integration with Maximilian
 * @ingroup dsp
 */
class Maximilian : public VolumeSupport {
    public:

        Maximilian(Print &out, int bufferSize=DEFAULT_BUFFER_SIZE, void (*callback)(maxi_float_t *channels)=play){
            buffer_size = bufferSize;
            p_sink = &out;
            this->callback = callback;
        }

        ~Maximilian() {
        }

        /// Setup Maximilian with audio parameters
        void begin(AudioInfo cfg){
            this->cfg = cfg;
            buffer.resize(buffer_size);
            maxiSettings::setup(cfg.sample_rate, cfg.channels, DEFAULT_BUFFER_SIZE);
        }

        /// Defines the volume. The values are between 0.0 and 1.0
        bool setVolume(float f) override{
            if (f>1.0f){
                VolumeSupport::setVolume(1.0f);
                return false;
            }
            if (f<0.0f){
                VolumeSupport::setVolume(0.0f);
                return false;
            }
            VolumeSupport::setVolume(f);
            return true;
        }

        /// Copies the audio data from maximilian to the audio sink, Call this method from the Arduino Loop. 
        void copy() {
            // fill buffer with data
            maxi_float_t out[cfg.channels];
            uint16_t samples = buffer_size / sizeof(uint16_t);
            int16_t *p_samples = (int16_t *) buffer.data();
            for (uint16_t j=0;j<samples;j+=cfg.channels){
                callback(out);
                // convert all channels to int16
                for (int ch=0;ch<cfg.channels;ch++){
                    p_samples[j+ch] = volume() * out[ch] * 32767.0f;
                }
            }
            // write buffer to audio sink
            unsigned int result = p_sink->write(buffer.data(), buffer_size);
            LOGI("bytes written %u", result)
        }

    protected:
        Vector<uint8_t> buffer;
        int buffer_size=256;
        Print *p_sink=nullptr;
        AudioInfo cfg;
        void (*callback)(maxi_float_t *channels);
};


} // namespace