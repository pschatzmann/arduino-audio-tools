#pragma once
#include "Arduino.h"
#include "freertos/FreeRTOS.h"
#include "Stk.h"
#include "Voicer.h"
#include "Instrmnt.h"

namespace audio_tools {

/**
 * @brief Stream Source which provides the audio data using the STK framework: https://github.com/pschatzmann/Arduino-STK
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class STKStream : public BufferedStream<int16_t> {

    public:
        /// provides the audio from a Voicer
        void begin(stk::Voicer &voicer) {
            p_voicer = &voicer;
            p_stk = p_voicer;
            p_instrument = nullptr;
            active = true;
        }

        /// provides the audio from an Instrument
        void begin(stk::Instrument &instrument) {
            p_instrument = &instrument;
            p_voicer = nullptr;
            active = true;
        }

        /// stops the generation of sound
        void end() {
            active = false;
        }

        /// Provides the basic audio info: mainly the sample rate
        AudioBaseInfo audioInfo() {
            AudioBaseInfo info;
            info.sample_rate = stk::STK::sampleRate();
            info.bits_per_sample = 16;
            info.channels = 1;
            return info;
        }

    protected:
        stk::Voicer *p_voicer=nullptr;
        stk::Instrument *p_instrument=nullptr;
        bool active = false;

        // not supported
        virtual size_t writeExt(const uint8_t* data, size_t len) {    
            LOGE("not supported");
            return 0;
        }

        virtual size_t readExt( uint8_t *data, size_t len) { 
            // generate samples
            size_t result = 0;
            if (active){
                result = len;
                size_t sample_count = len / sizeof(int16_t);
                int16_t* samples = (int16_t*)data;

                if (p_voicer!=nullptr){
                    for (int j=0;j<sample_count;j++){
                        // scale ticks to int16 values
                        samples[j] = p_voicer->tick() * 32768;
                    }
                } else if (p_instrument!=nullptr){
                    for (int j=0;j<sample_count;j++){
                        // scale ticks to int16 values
                        samples[j] = p_instrument->tick() * 32768;
                    }
                }
            }
            return result;
        }
};


}