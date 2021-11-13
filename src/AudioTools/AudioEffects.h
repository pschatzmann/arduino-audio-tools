#pragma once
#include "AudioTools/SoundGenerator.h"
#include "AudioTools/Vector.h"

namespace audio_tools {

// we use int16_t for our effects
typedef int16_t effect_t;

/**
 * @brief Abstract Base class for Sound Effects
 * @author Phil Schatzmann
 * @copyright GPLv3
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
        int16_t clip(int32_t in, int16_t clipLimit=32767, int16_t resultLimit=32767 ) {
            int32_t result = in;
            if (result > clipLimit) {result = resultLimit;}
            if (result < -clipLimit) {result = -resultLimit;}
            return result;
        }
};

/**
 * @brief Boost
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */

class Boost : public AudioEffect {
    public:
        /// Boost Constructor: volume 0.1 - 1.0: decrease result; volume >0: increase result 
        Boost(float &volume){
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
 * @brief Distortion
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class Distortion : public AudioEffect  {
    public:
        /// Distortion Constructor: e.g. use clipThreashold 4990 and maxInput=6500
        Distortion(int16_t &clipThreashold, int16_t maxInput=6500){
            p_clip_threashold = &clipThreashold;
            max_input = maxInput;   
        }

        effect_t process(effect_t input){
            if (!active()) return input;
            //the input signal is 16bits (values from -32768 to +32768
            //the value of input is clipped to the distortion_threshold value      
            return clip(input,*p_clip_threashold, max_input);
        }

    protected:
        int16_t *p_clip_threashold;
        int16_t max_input;

};

/**
 * @brief Fuzz
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class Fuzz : public AudioEffect  {
    public:
        /// Fuzz Constructor: use e.g. effectValue=6.5; maxOut = 300
        Fuzz(float &fuzzEffectValue, uint16_t maxOut = 300){
            p_effect_value = &fuzzEffectValue;
            max_out = maxOut;
        }

        effect_t process(effect_t input){
            if (!active()) return input;
            float v = *p_effect_value;
            int32_t result = clip(v * input) ;
            return map(result * v, -32768, +32767,-max_out, max_out);
        }

    protected:
        float *p_effect_value;
        uint16_t max_out;

};

/**
 * @brief Tremolo
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class Tremolo : public AudioEffect  {
    public:
        /// Tremolo constructor -  use e.g. duration_ms=2000; depth=0.5; sampleRate=44100
        Tremolo(int16_t &duration_ms, float &depth, uint16_t sampleRate) {
            int32_t rate_count = static_cast<int32_t>(duration_ms) * sampleRate / 1000;
            rate_count_half = rate_count / 2;
            
            // limit value to max 1.0
            tremolo_depth = depth>1.0 ? 1.0 : depth;
            signal_depth = 1.0 - depth;
        }

        effect_t process(effect_t input) {
            if (!active()) return input;

            float tremolo_factor = tremolo_depth / rate_count_half;
            int32_t out = (signal_depth * input) + (tremolo_factor * count * input); 

            //saw tooth shaped counter
            count+=inc;
            if (count>=rate_count_half){
                inc = -1;
            } else if (count<=0) {
                inc = +1;
            }

            return clip(out);
        }


    protected:
        int32_t count = 0;
        int16_t inc = 1;
        int32_t rate_count_half; // number of samples for on raise and fall
        float tremolo_depth;
        float signal_depth;
        float tremolo_factor ;

};


/**
 * @brief Simple GuitarEffects
 * Based on Stratocaster with on-board Electrosmash Arduino UNOR3 pedal electronics CC-by-www.Electrosmash.com
 * Based on OpenMusicLabs previous works.
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

#ifdef USE_STK

/**
 * Use any effect from the STK framework: e.g. Chorus, Echo, FreeVerb, JCRev, PitShift...
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class STKEffect : public AudioEffect {
    public:
        STKEffect(stk::Effect &stkEffect){
            p_effect = &stkEffect;
        }

        virtual effect_t process(effect_t in) {
            // just convert between int16 and float
            float value = static_cast<float>(in) / 32767.0;
            return p_effect->tick(value)*32767.0;
        }

    protected:
        stk::Effect *p_effect=nullptr;

};


#endif



} // namespace