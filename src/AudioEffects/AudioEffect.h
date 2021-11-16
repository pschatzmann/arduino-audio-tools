#pragma once
#include <stdint.h> 

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
 * @brief Boost AudioEffect
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
 * @brief Distortion AudioEffect
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
 * @brief Fuzz AudioEffect
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
 * @brief Tremolo AudioEffect
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class Tremolo : public AudioEffect  {
    public:
        /// Tremolo constructor -  use e.g. duration_ms=2000; depthPercent=50; sampleRate=44100
        Tremolo(int16_t &duration_ms, uint8_t& depthPercent, uint32_t sampleRate=44100) {
            int32_t rate_count = sampleRate * duration_ms / 1000;
            rate_count_half = rate_count / 2;
            p_percent = &depthPercent;
        }

        effect_t process(effect_t input) {
            if (!active()) return input;

            // limit value to max 100% and calculate factors
            float tremolo_depth = (*p_percent) > 100 ? 1.0 : 0.01 * (*p_percent);
            float signal_depth = (100.0 - (*p_percent)) / 100.0;

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
        uint8_t* p_percent;

};

/**
 * @brief Delay AudioEffect. We mix the actual signal with the delayed signal
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class Delay : public AudioEffect  {
    public:
        /// e.g. depthPercent=50, ms=1000, sampleRate=44100
        Delay(uint16_t &duration_ms, uint8_t& depthPercent,  uint32_t sampleRate=44100) {
            this->sampleRate = sampleRate;
            p_percent = &depthPercent;
            p_ms = &duration_ms;
        }

        effect_t process(effect_t input) {
            if (!active()) return input;

            updateBufferSize();
            // get value from buffer
            int32_t value = (p_history->available()<sampleCount) ? input : p_history->read();
            // add actual input value
            p_history->write(input);
            // mix input with result
            return (value * (*p_percent) / 100) + (input * (100-(*p_percent))/100); 
        }

    protected:
        RingBuffer<effect_t>* p_history=nullptr;
        uint8_t* p_percent;
        uint16_t *p_ms;
        uint16_t sampleCount=0;
        uint32_t sampleRate;

        void updateBufferSize(){
            uint16_t newSampleCount = sampleRate * (*p_ms) / 1000;
            if (newSampleCount>sampleCount){
                if (p_history!=nullptr) delete p_history;
                sampleCount = newSampleCount;
                p_history = new RingBuffer<effect_t>(sampleCount);
            }
        }

};

}
