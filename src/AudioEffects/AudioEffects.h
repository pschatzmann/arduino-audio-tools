#pragma once

#include "AudioBasic/Vector.h"
#include "AudioEffects/SoundGenerator.h"
#include "AudioEffects/AudioEffect.h"
#ifdef  USE_EFFECTS_SUITE
#include "AudioEffects/AudioEffectsSuite.h"
#endif
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
            p_generator = copy.p_generator;
            for (int j=0;j<copy.size();j++){
                effects.push_back(copy[j]->clone());
            }
            LOGI("Number of effects %d -> %d", copy.size(), this->size());
        }

        /// Constructor which is assigning a generator
        AudioEffects(GeneratorT &generator) {
            setInput(generator);
        }

        /// Constructor which is assigning a Stream as input. The stream must consist of int16_t values 
        /// with the indicated number of channels. Type type parameter is e.g.  <GeneratorFromStream<effect_t>
        AudioEffects(Stream &input, int channels=2, float volume=1.0) {
            setInput(* (new GeneratorT(input, channels, volume)));
            owns_generator = true;
        }

        /// Destructor
        virtual ~AudioEffects(){
            LOGD(LOG_METHOD);
            if (owns_generator && p_generator!=nullptr){
                delete p_generator;
            }
            for (int j=0;j<effects.size();j++){
                delete effects[j];
            }
        }
        
        /// Defines the input source for the raw guitar input
        void setInput(GeneratorT &in){
            LOGD(LOG_METHOD);
            p_generator = &in;
            // automatically activate this object
            AudioBaseInfo info;
            info.channels = 1;
            info.bits_per_sample = sizeof(effect_t)*8;
            begin(info);
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
            effect_t sample;
            if (p_generator!=nullptr){
                sample  = p_generator->readSample();
                int size = effects.size();
                for (int j=0; j<size; j++){
                    sample = effects[j]->process(sample);
                }
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
            return *p_generator;
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
        GeneratorT *p_generator=nullptr;
        bool owns_generator = false;
};




} // namespace