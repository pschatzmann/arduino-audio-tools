#pragma once

#include "AudioConfig.h"
#ifdef USE_STK

#include "Arduino.h"
#include "freertos/FreeRTOS.h"
#include "Stk.h"
#include "Voicer.h"
#include "Instrmnt.h"

namespace audio_tools {


template <class T>
class STKGenerator : public SoundGenerator<T> {
    public:
        // the scale defines the max value which is generated
        STKGenerator(stk::Instrmnt &instrument) : SoundGenerator<T>() {
            this->p_instrument = &instrument;
        }
        STKGenerator(stk::Voicer  &voicer) : SoundGenerator<T>() {
            this->p_voicer = &voicer;
        }

        AudioBaseInfo defaultConfig() {
            AudioBaseInfo info;
            info.channels = 1;
            info.bits_per_sample = sizeof(T) * 8;
            info.sample_rate = stk::Stk::sampleRate();
            return info;
        }

        void begin(AudioBaseInfo cfg){
	 		LOGI(LOG_METHOD);
            cfg.logInfo();
            SoundGenerator<T>::begin(cfg);
            SoundGenerator<T>::setChannels(cfg.channels);
            max_value = maxValue(sizeof(T)*8);
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