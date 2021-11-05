#pragma once
#include "AudioTools/SoundGenerator.h"
#include "AudioTools/Vector.h"

namespace audio_tools {

// we use int16_t for our effects
typedef int16_t effect_t;

/**
 * @brief Abstract Base class for Sound Effects
 * 
 * @tparam effect_t 
 */

class AudioEffect  {
    public:
        /// calculates the effect output from the input
        virtual effect_t process(effect_t in) = 0;

        /// sets the effect active/inactive
        virtual void setActive(bool value){
            active_flag = value;
        }

        /// determines if the effect is active
        virtual bool active() {
            return active_flag;
        }
    protected:
        bool active_flag = true;
        /// generic clipping method
        int16_t clip(int32_t in, int16_t clipLimit=32768, int16_t resultLimit=32768 ) {
            int32_t result = in;
            if (result > clipLimit) {result = resultLimit;}
            if (result < -clipLimit) {result = -resultLimit;}
            return result;
        }
};

/**
 * @brief EffectBoost
 * 
 * @tparam effect_t 
 */

class Boost : public AudioEffect {
    public:
        /// volume 0.1 - 1.0: decrease result; volume >0: increase result 
        Boost(float &volume, float ym=0.1){
            p_effect_value = &volume;
        }

        effect_t process(effect_t input){
            if (!active()) return input;
            int32_t result = (*p_effect_value) * input;
            // clip to int16_t            
            return clip(result);
        }

    protected:
        float *p_effect_value;
};

/**
 * @brief EffectDistortion
 * 
 * @tparam effect_t 
 */

class Distortion : public AudioEffect  {
    public:
        // e.g. use clipThreashold 4990
        Distortion(int16_t &clipThreashold, int16_t maxInput=6500){
            p_effect_value = &clipThreashold;
            max_input = maxInput;   
        }

        effect_t process(effect_t input){
            if (!active()) return input;
            //the input signal is 16bits (values from -32768 to +32768
            //the value of input is clipped to the distortion_threshold value      
            return clip(input,*p_effect_value,max_input);
        }

    protected:
        int16_t *p_effect_value;
        int16_t max_input;

};

/**
 * @brief EffectFuzz
 * 
 * @tparam effect_t 
 */

class Fuzz : public AudioEffect  {
    public:
        // use e.g. effectValue=6.5
        Fuzz(float &fuzzEffectValue, uint16_t maxOut = 300){
            p_effect_value = &fuzzEffectValue;
            max_out = maxOut;
        }

        effect_t process(effect_t input){
            if (!active()) return input;
            int32_t result = clip(*p_effect_value * input) ;
            return map(result * (*p_effect_value), -32768, +32768,-max_out, max_out);
        }

    protected:
        float *p_effect_value;
        uint16_t max_out;

};

/**
 * @brief EffectTremolo
 * see https://www.dsprelated.com/showcode/234.php
 * @tparam effect_t 
 */

class Tremolo : public AudioEffect  {
    public:
        // use e.g. effect_rate=4000
        Tremolo(int16_t &effect_rate, int32_t depth=1) {
            dep     = depth; 
            control = 1;
            mod     = 0;
            counter_limit = effect_rate;
            offset  = 1 - dep;
        }

        effect_t process(effect_t input) {
            if (!active()) return input;
            int32_t yout;
            int32_t m;

            m = (int32_t)mod*dep/counter_limit;
            yout = (m + offset)*input;

            // Makes LFO vary
            sweep();
            return yout;
        }


    protected:
        int32_t dep;
        int32_t offset;
        int16_t counter_limit;
        int16_t control;
        int16_t mod;

        void sweep(void) {
            mod += control;
    
            if (mod > counter_limit) {
                control = -1;
            } 
            else if(!mod) {
                control = 1;
            }
        }

};


/**
 * @brief Simple GuitarEffects
 * Based on Stratocaster with on-board Electrosmash Arduino UNOR3 pedal electronics CC-by-www.Electrosmash.com
 * Based on OpenMusicLabs previous works.
 */

class AudioEffects : public SoundGenerator<effect_t>  {

    public:
        /// Defines the input source for the raw guitar input
        void setInput(SoundGenerator &in){
            p_source = &in;
        }

        /// Adds an effect
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
        SoundGenerator *p_source;
        Vector<AudioEffect*> effects;

};


} // namespace