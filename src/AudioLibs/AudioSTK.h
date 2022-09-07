#pragma once

#include "AudioConfig.h"

#include "Arduino.h"
#include "freertos/FreeRTOS.h"
#include "StkAll.h"

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
        STKGenerator() = default;

        // Creates an STKGenerator for an instrument
        STKGenerator(stk::Instrmnt &instrument) : SoundGenerator<T>() {
            this->p_instrument = &instrument;
        }

        // Creates an STKGenerator for a voicer (combination of multiple instruments)
        STKGenerator(stk::Voicer  &voicer) : SoundGenerator<T>() {
            this->p_voicer = &voicer;
        }

        void setInput(stk::Instrmnt &instrument){
            this->p_instrument = &instrument;
        }

        void setInput(stk::Voicer &voicer){            
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
        bool begin(AudioBaseInfo cfg){
             LOGI(LOG_METHOD);
            cfg.logInfo();
            SoundGenerator<T>::begin(cfg);
            max_value = NumberConverter::maxValue(sizeof(T)*8);
            stk::Stk::setSampleRate(SoundGenerator<T>::info.sample_rate);
            return true;
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

/**
 * @brief STK Stream for Instrument or Voicer
 * 
 * @tparam T 
 */
template <class T>
class STKStream : public GeneratedSoundStream<T> {
    public:
        STKStream(Instrmnt &instrument){
            generator.setInput(instrument);
            GeneratedSoundStream<T>::setInput(generator);
        }
        STKStream(Voicer &voicer){
            generator.setInput(voicer);
            GeneratedSoundStream<T>::setInput(generator);
        }
    protected:
        STKGenerator<T> generator;

};


}

