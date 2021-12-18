#pragma once

#include "AudioTools/SoundGenerator.h"
#include "AudioTools/Vector.h"
#include "AudioEffects/AudioEffect.h"
#include "AudioEffects/AudioEffectsSuite.h"
#ifdef  USE_STK
#include "AudioEffects/STKEffects.h"
#endif

namespace audio_tools {

/**
 * @brief AudioEffects
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class AudioEffects : public SoundGenerator<effect_t>  {

    public:
        
        /// Default constructor
        AudioEffects(SoundGenerator &in){
            setInput(in);
        }

        AudioEffects(Stream &in){
            setInput(in);
        }

        AudioEffects() = default;

        ~AudioEffects(){
            if (p_stream_gen!=nullptr) delete p_stream_gen;
        }


        /// Defines the input source for the raw guitar input
        void setInput(SoundGenerator &in){
            p_source = &in;
        }

        /// Defines the input source for the raw guitar input; MEMORY LEAK WARNING use only once!
        void setInput(Stream &in){
            // allocate optional adapter class
            if (p_stream_gen==nullptr){
                p_stream_gen = new GeneratorFromStream<int16_t>();
            } 
            p_stream_gen->setStream(in);
            p_source = p_stream_gen;
        }

        /// Adds an effect object (by reference)
        void addEffect(AudioEffect &effect){
            effects.push_back(&effect);
        }

        /// Adds an effect using a pointer
        void addEffect(AudioEffect *effect){
            effects.push_back(effect);
        }

        /// provides the resulting sample
        virtual  effect_t readSample() {
            effect_t input = p_source->readSample();
            int size = effects.size();
            for (int j=0; j<size; j++){
                input = effects[j]->process(input);
            }
            return input;
        }

    protected:
        Vector<AudioEffect*> effects;
        SoundGenerator *p_source;
        // optional adapter class to support streams
        GeneratorFromStream<int16_t> *p_stream_gen = nullptr;
};


} // namespace