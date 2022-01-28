#pragma once
#include <stdint.h> 
#include "AudioTools/AudioLogger.h"
#include "AudioEffects/AudioParameters.h"

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
        AudioEffect() = default;
        virtual ~AudioEffect() = default;
        
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

        virtual AudioEffect *clone() = 0;

        /// Allows to identify an effect
        int id() {
            return id_value;
        }

        /// Allows to identify an effect
        void setId(int id){
            this->id_value = id;
        }

    protected:
        bool active_flag = true;
        int id_value=-1;

        void copyParent(AudioEffect *copy){
            id_value = copy->id_value;
            active_flag = copy->active_flag;
        }

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
        Boost(float volume=1.0){
            effect_value = volume;
        }

        Boost(const Boost &copy) = default;

        float volume() {
            return effect_value;
        }

        void setVolume(float volume){
            effect_value = volume;
        }

        effect_t process(effect_t input){
            if (!active()) return input;
            int32_t result = effect_value * input;
            // clip to int16_t            
            return clip(result);
        }

        Boost* clone() {
            return new Boost(*this);
        }

    protected:
        float effect_value;
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
        Distortion(int16_t clipThreashold=4990, int16_t maxInput=6500){
            p_clip_threashold = clipThreashold;
            max_input = maxInput;   
        }

        Distortion(const Distortion &copy) = default;

        void setClipThreashold(int16_t th){
            p_clip_threashold = th;
        }

        int16_t clipThreashold(){
            return p_clip_threashold;
        }

        void setMaxInput(int16_t maxInput){
            max_input = maxInput;
        }

        int16_t maxInput(){
            return max_input;
        } 

        effect_t process(effect_t input){
            if (!active()) return input;
            //the input signal is 16bits (values from -32768 to +32768
            //the value of input is clipped to the distortion_threshold value      
            return clip(input,p_clip_threashold, max_input);
        }

        Distortion* clone() {
            return new Distortion(*this);
        }

    protected:
        int16_t p_clip_threashold;
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
        Fuzz(float fuzzEffectValue, uint16_t maxOut = 300){
            p_effect_value = fuzzEffectValue;
            max_out = maxOut;
        }

        Fuzz(const Fuzz& copy) = default;

        void setFuzzEffectValue(float v){
            p_effect_value = v;
        }

        float fuzzEffectValue(){
            return p_effect_value;
        }

        void setMaxOut(uint16_t v){
            max_out = v;
        }

        uint16_t maxOut(){
            return max_out;
        }

        effect_t process(effect_t input){
            if (!active()) return input;
            float v = p_effect_value;
            int32_t result = clip(v * input) ;
            return map(result * v, -32768, +32767,-max_out, max_out);
        }

        Fuzz *clone() {
            return new Fuzz(*this);
        }

    protected:
        float p_effect_value;
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
        Tremolo(int16_t duration_ms=2000, uint8_t depthPercent=50, uint32_t sampleRate=44100) {
            this->duration_ms = duration_ms;
            this->sampleRate = sampleRate;
            this->p_percent = depthPercent;
            int32_t rate_count = sampleRate * duration_ms / 1000;
            rate_count_half = rate_count / 2;
        }

        Tremolo(const Tremolo &copy) = default;

        void setDuration(int16_t ms){
            int32_t rate_count = sampleRate * ms / 1000;
            rate_count_half = rate_count / 2;
        }

        int16_t duration(){
            return duration_ms;
        }

        void setDepth(uint8_t percent){
            p_percent = percent;
        }

        uint8_t depth() {
            return p_percent;
        }

        effect_t process(effect_t input) {
            if (!active()) return input;

            // limit value to max 100% and calculate factors
            float tremolo_depth = p_percent > 100 ? 1.0 : 0.01 * p_percent;
            float signal_depth = (100.0 - p_percent) / 100.0;

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

        Tremolo* clone() {
            return new Tremolo(*this);
        }

    protected:
        int16_t duration_ms;
        uint32_t sampleRate;
        int32_t count = 0;
        int16_t inc = 1;
        int32_t rate_count_half; // number of samples for on raise and fall
        uint8_t p_percent;

};

/**
 * @brief Delay AudioEffect. We mix the actual signal with the delayed signal
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class Delay : public AudioEffect  {
    public:
        /// e.g. depthPercent=50, ms=1000, sampleRate=44100
        Delay(uint16_t duration_ms=1000, uint8_t depthPercent=50,  uint32_t sampleRate=44100) {
            this->sampleRate = sampleRate;
            p_percent = depthPercent;
            p_ms = duration_ms;
        }

        Delay(const Delay &copy) = default;

        void setDuration(int16_t ms){
            p_ms = ms;
        }

        int16_t duration(){
            return p_ms;
        }

        void setDepth(uint8_t percent){
            p_percent = percent;
        }

        uint8_t depth() {
            return p_percent;
        }

        effect_t process(effect_t input) {
            if (!active()) return input;

            updateBufferSize();
            // get value from buffer
            int32_t value = (p_history->available()<sampleCount) ? input : p_history->read();
            // add actual input value
            p_history->write(input);
            // mix input with result
            return (value * p_percent / 100) + (input * (100-p_percent)/100); 
        }

        Delay *clone() {
            return new Delay(*this);
        }

    protected:
        RingBuffer<effect_t>* p_history=nullptr;
        uint8_t  p_percent;
        uint16_t p_ms;
        uint16_t sampleCount=0;
        uint32_t sampleRate;

        void updateBufferSize(){
            uint16_t newSampleCount = sampleRate * p_ms / 1000;
            if (newSampleCount>sampleCount){
                if (p_history!=nullptr) delete p_history;
                sampleCount = newSampleCount;
                p_history = new RingBuffer<effect_t>(sampleCount);
            }
        }
};

/**
 * @brief ADSR Envelope: Attack, Decay, Sustain and Release.
 * Attack is the time taken for initial run-up of level from nil to peak, beginning when the key is pressed.
 * Decay is the time taken for the subsequent run down from the attack level to the designated sustainLevel level.
 * Sustain is the level during the main sequence of the sound's duration, until the key is released.
 * Release is the time taken for the level to decay from the sustainLevel level to zero after the key is released.[4]

 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class ADSRGain : public AudioEffect {
    public:

        ADSRGain(float attack=0.001, float decay=0.001, float sustainLevel=0.5, float release= 0.005, float boostFactor=1.0){
            this->factor = boostFactor;
            adsr = new ADSR(attack,decay,sustainLevel,release);
        }

        ADSRGain(const ADSRGain &ref) {
            adsr = new ADSR(*(ref.adsr));
            factor = ref.factor;
            copyParent((AudioEffect *)&ref);
        };

        virtual ~ADSRGain(){
            delete adsr;
        }

        void setAttackRate(float a){
            adsr->setAttackRate(a);
        }

        float attackRate(){
            return adsr->attackRate();
        }

        void setDecayRate(float d){
            adsr->setDecayRate(d);
        }

        float decayRate() {
            return adsr->decayRate();
        }

        void setSustainLevel(float s){
            adsr->setSustainLevel(s);
        }

        float sustainLevel(){
            return adsr->sustainLevel();
        }
        
        void setReleaseRate(float r){
            adsr->setReleaseRate(r);
        }

        float releaseRate() {
            return adsr->releaseRate();
        }

        void keyOn(float tgt=0){
            adsr->keyOn(tgt);
        }

        void keyOff(){
            adsr->keyOff();
        }

        effect_t process(effect_t input) {
            effect_t result = factor * adsr->tick() * input;
            return result;
        }

        bool isActive(){
            return adsr->isActive();
        }

        ADSRGain *clone() {
            return new ADSRGain(*this);
        }

    protected:
        ADSR *adsr;
        float factor;

};

}
