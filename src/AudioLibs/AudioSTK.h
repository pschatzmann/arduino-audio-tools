#pragma once

#include "AudioConfig.h"
#include "Arduino.h"
#ifdef ESP32
#  include "freertos/FreeRTOS.h"
#endif
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

template <class StkCls, class T>
class STKGenerator : public SoundGenerator<T> {
    public:
        STKGenerator() = default;

        // Creates an STKGenerator for an instrument
        STKGenerator(StkCls &instrument) : SoundGenerator<T>() {
            this->p_instrument = &instrument;
        }

        void setInput(StkCls &instrument){
            this->p_instrument = &instrument;
        }

        /// provides the default configuration
        AudioBaseInfo defaultConfig() {
            AudioBaseInfo info;
            info.channels = 2;
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
            }
            return result;
        }

    protected:
        StkCls *p_instrument=nullptr;
        T max_value;

};

/**
 * @brief STK Stream for Instrument or Voicer
 */
template <class StkCls>
class STKStream : public GeneratedSoundStream<int16_t> {
    public:
        STKStream() = default;

        STKStream(StkCls &instrument){
            generator.setInput(instrument);
            GeneratedSoundStream<int16_t>::setInput(generator);
        }
        void setInput(StkCls &instrument){
            generator.setInput(instrument);
            GeneratedSoundStream<int16_t>::setInput(generator);
        }
        void setInput(StkCls *instrument){
            generator.setInput(*instrument);
            GeneratedSoundStream<int16_t>::setInput(generator);
        }

        AudioBaseInfo defaultConfig() {
            AudioBaseInfo info;
            info.channels = 2;
            info.bits_per_sample = 16;
            info.sample_rate = stk::Stk::sampleRate();
            return info;
        }

    protected:
        STKGenerator<StkCls,int16_t> generator;

};


}

