#pragma once

#include "AudioBasic/Vector.h"
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
 * 
 * We subclass the AudioEffects from GeneratorT so that we can use this class with the GeneratedSoundStream 
 * class to output the audio.
 *   
 * @author Phil Schatzmann
 * @copyright GPLv3
 */


template <class GeneratorT>
class AudioEffects : public SoundGenerator<effect_t> {
    public:
        /// Default constructor
        AudioEffects() = default;

        /// Copy constructor
        AudioEffects(AudioEffects &copy) {
            LOGI(LOG_METHOD);
            // create a copy of the source and all effects
            generator_obj = copy.generator_obj;
            for (int j=0;j<copy.size();j++){
                effects.push_back(copy[j]->clone());
            }
            LOGI("Number of effects %d -> %d", copy.size(), this->size());
        }

        /// Constructor which is assigning a generator
        AudioEffects(GeneratorT &generator) {
            setInput(generator);
        }

        /// Destructor
        ~AudioEffects(){
            LOGD(LOG_METHOD);
            for (int j=0;j<effects.size();j++){
                delete effects[j];
            }
        }
        
        /// Defines the input source for the raw guitar input
        void setInput(GeneratorT &in){
            LOGD(LOG_METHOD);
            generator_obj = in;
            // automatically activate this object
            begin();
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
        effect_t readSample() override {
            effect_t sample = generator_obj.readSample();
            int size = effects.size();
            for (int j=0; j<size; j++){
                sample = effects[j]->process(sample);
            }
            return sample;
        }

        /// deletes all defined effects
        void clear() {
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