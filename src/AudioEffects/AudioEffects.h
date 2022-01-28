#pragma once

#include "AudioCommon/Vector.h"
#include "AudioEffects/SoundGenerator.h"
#include "AudioEffects/AudioEffect.h"
#include "AudioEffects/AudioEffectsSuite.h"
#ifdef  USE_STK
#include "AudioEffects/STKEffects.h"
#endif

namespace audio_tools {

/**
 * @brief AudioEffects: the template class describes the input audio to which the effects are applied: 
 * e.g. SineWaveGenerator, SquareWaveGenerator, GeneratorFromStream etc. 
 * We support only one channel of int16_t data!
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

template <class GeneratorT>
class AudioEffects  {
    public:
        AudioEffects() = default;

        AudioEffects(AudioEffects &copy) {
            LOGI(LOG_METHOD);
            // create a copy of the source and all effects
            generator_obj = copy.generator_obj;
            for (int j=0;j<copy.size();j++){
                effects.push_back(copy[j]->clone());
            }
            LOGI("Number of effects %d -> %d", copy.size(), this->size());
        }

        ~AudioEffects(){
            LOGD(LOG_METHOD);
            for (int j=0;j<effects.size();j++){
                delete effects[j];
            }
        }

        /// Defines the input source for the raw guitar input
        void setInput(SoundGenerator<int16_t> &in){
            LOGD(LOG_METHOD);
            generator_obj = in;
        }

        /// Adds an effect object (by reference)
        void addEffect(AudioEffect &effect){
            LOGD(LOG_METHOD);
            effects.push_back(&effect);
        }

        /// Adds an effect using a pointer
        void addEffect(AudioEffect *effect){
            LOGD(LOG_METHOD);
            effects.push_back(effect);
            LOGI("addEffect -> Number of effects: %d", size());
        }

        /// provides the resulting sample
        virtual effect_t readSample() {
            effect_t input = generator_obj.readSample();
            int size = effects.size();
            for (int j=0; j<size; j++){
                input = effects[j]->process(input);
            }
            return input;
        }

        /// deletes all defined effects
        virtual void clear() {
            LOGD(LOG_METHOD);
            effects.clear();
        }

        /// Provides the actual number of defined effects
        size_t size() {
            return effects.size();
        }

        /// Provides access to the sound generator
        GeneratorT &generator(){
            return generator_obj;
        }

        /// gets an effect by index
        AudioEffect* operator [](int idx){
            return effects[idx];
        }

        /// Finds an effect by id
        AudioEffect* findEffect(int id){
            // find AudioEffect
            AudioEffect* result = nullptr;
            for (int j=0;j<size();j++){
                // we assume that ADSRGain is the first effect!
                if (effects[j]->id()==id){
                    result = effects[j];
                }
                LOGI("--> findEffect -> %d", effects[j]->id());
            }
            return result;
        }

    protected:
        Vector<AudioEffect*> effects;
        GeneratorT generator_obj;
};


} // namespace