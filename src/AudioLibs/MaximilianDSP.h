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
class Maximilian {
    public:

        Maximilian(Print &out, int bufferSize=DEFAULT_BUFFER_SIZE, void (*callback)(maxi_float_t *channels)=play){
            buffer_size = bufferSize;
            p_buffer = new uint8_t[bufferSize];
            p_sink = &out;
            this->callback = callback;
        }

        ~Maximilian() {
            delete[] p_buffer;
        }

        /// Setup Maximilian with audio parameters
        void begin(AudioInfo cfg){
            this->cfg = cfg;
            maxiSettings::setup(cfg.sample_rate, cfg.channels, DEFAULT_BUFFER_SIZE);
        }

        /// Defines the volume. The values are between 0.0 and 1.0
        void setVolume(float f){
            volume = f;
            if (volume>1){
                volume = 1;
            }
            if (volume<0){
                volume = 0;
            }
        }

        /// Copies the audio data from maximilian to the audio sink, Call this method from the Arduino Loop. 
        void copy() {
            // fill buffer with data
            maxi_float_t out[cfg.channels];
            uint16_t samples = buffer_size / sizeof(uint16_t);
            int16_t *p_samples = (int16_t *)p_buffer;
            for (uint16_t j=0;j<samples;j+=cfg.channels){
                callback(out);
                // convert all channels to int16
                for (int ch=0;ch<cfg.channels;ch++){
                    p_samples[j+ch] = out[ch]*32767*volume;
                }
            }
            // write buffer to audio sink
            unsigned int result = p_sink->write(p_buffer, buffer_size);
            LOGI("bytes written %u", result)
        }

    protected:
        uint8_t *p_buffer=nullptr;
        float volume=1.0;
        int buffer_size=256;
        Print *p_sink=nullptr;
        AudioInfo cfg;
        void (*callback)(maxi_float_t *channels);
};


} // namespace