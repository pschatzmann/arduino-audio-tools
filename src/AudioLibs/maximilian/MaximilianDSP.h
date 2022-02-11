#pragma once
#include "AudioConfig.h"
#include "maximilian.h"

// Maximilian play function - return an array of 2 channels
void play(double *channels);//run dac! 

namespace audio_tools {

/**
 * @brief AudioTools integration with Maximilian
 * 
 */
class Maximilian {
    public:

        Maximilian(Print &out, int bufferSize=DEFAULT_BUFFER_SIZE){
            buffer_size = bufferSize;
            p_buffer = new uint8_t[bufferSize];
            p_sink = &out;
        }

        ~Maximilian() {
            delete[] p_buffer;
        }

        /// Setup Maximilian with audio parameters
        void begin(AudioBaseInfo cfg){
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
            double out[2];
            uint16_t samples = buffer_size / sizeof(uint16_t);
            int16_t *p_samples = (int16_t *)p_buffer;
            for (uint16_t j=0;j<samples;j+=2){
                play(out);
                // convert to int16
                p_samples[j] = out[0]*32767*volume;
                p_samples[j+1] = out[1]*32767*volume;
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
};


} // namespace