#pragma once

#include "AudioConfig.h"
#ifdef USE_STK

#include "Arduino.h"
#include "freertos/FreeRTOS.h"
#include "Stk.h"
#include "Voicer.h"
#include "Instrmnt.h"

namespace audio_tools {

/**
 * @brief The Synthesis ToolKit in C++ (STK) is a set of open source audio signal processing and algorithmic synthesis classes
 * written in the C++ programming language.  
 * 
 * You can find further informarmation in the original Readme of the STK Project
 *
 * Like many other sound libraries it originates from an University (Princeton) and can look back at a very long history: 
 * it was created in 1995. In the 90s the computers had limited processor power and memory available. 
 * In todays world we can get some cheap Microcontrollers, which provide almost the same capabilities.
 *
 * 
 * @tparam T 
 */

template <class T>
class STKGenerator : public SoundGenerator<T> {
    public:
        // Creates an STKGenerator for an instrument
        STKGenerator(stk::Instrmnt &instrument) : SoundGenerator<T>() {
            this->p_instrument = &instrument;
        }

        // Creates an STKGenerator for a voicer (combination of multiple instruments)
        STKGenerator(stk::Voicer  &voicer) : SoundGenerator<T>() {
            this->p_voicer = &voicer;
        }

        /// provides the default configuration
        AudioBaseInfo defaultConfig() {
            AudioBaseInfo info;
            info.channels = 1;
            info.bits_per_sample = sizeof(T) * 8;
            info.sample_rate = stk::Stk::sampleRate();
            return info;
        }

        /// Starts the processing
        void begin(AudioBaseInfo cfg){
             LOGI(LOG_METHOD);
            cfg.logInfo();
            SoundGenerator<T>::begin(cfg);
            max_value = NumberConverter::maxValue(sizeof(T)*8);
            stk::Stk::setSampleRate(SoundGenerator<T>::info.sample_rate);
        }

        /// Provides a single sample
        T readSample() {
            T result = 0;
            if (p_instrument!=nullptr) {
                result = p_instrument->tick()*max_value;
            } else if (p_voicer!=nullptr){
                result = p_voicer->tick()*max_value;
            }
            return result;
        }

    protected:
        stk::Instrmnt *p_instrument=nullptr;
        stk::Voicer *p_voicer=nullptr;
        T max_value;

};


}

#endif