#pragma once
#include "AudioTools/AudioLogger.h"

namespace audio_tools {

/**
 * @brief Base class for all parameters
 */
class AbstractParameter {
    public:
        virtual ~AbstractParameter() = default;

        virtual float value() {
            return act_value;
        };

        // triggers an update of the value
        virtual float tick() {
            act_value = update();
            return act_value;
        }

        // to manage keyboard related parameters
        virtual void keyOn(float tgt=0){}

        // to manage keyboard related parameters
        virtual void keyOff(){}

    protected:
        float act_value = 0;
        friend class ScaledParameter;

        virtual float update() = 0;
};

/**
 * @brief A constant value
 * @ingroup effects
 */
class Parameter : public AbstractParameter {
    public:
        Parameter(float value){
            act_value = value;
        }
        virtual float update(){ return act_value;}
};

/**
 * @brief Generates ADSR values between 0.0 and 1.0
 */
class ADSR : public  AbstractParameter  {
    public:

        ADSR(float attack=0.001, float decay=0.001, float sustainLevel=0.5, float release= 0.005){
            this->attack = attack;
            this->decay = decay;
            this->sustain = sustainLevel;
            this->release = release;
        }

        ADSR(ADSR &copy) = default;

        void setAttackRate(float a){
            attack = a;
        }

        float attackRate(){
            return attack;
        }

        void setDecayRate(float d){
            decay = d;
        }

        float decayRate() {
            return decay;
        }

        void setSustainLevel(float s){
            sustain = s;
        }

        float sustainLevel(){
            return sustain;
        }

        void setReleaseRate(float r){
            release = r;
        }

        float releaseRate() {
            return release;
        }

        void keyOn(float tgt=0){
            LOGI("%s: %f", LOG_METHOD, tgt);
            state = Attack;
            this->target = tgt>0.0f && tgt<=1.0f ? tgt : sustain;
            this->act_value = 0;
        }

        void keyOff(){
            TRACEI();
            if (state!=Idle){
                state = Release;
                target = 0;
            }
        }

        bool isActive(){
            return state!=Idle;
        }

    protected:
        float attack,  decay,  sustain,  release;
        enum AdsrPhase {Idle, Attack, Decay, Sustain, Release};
        const char* adsrNames[5] = {"Idle", "Attack", "Decay", "Sustain", "Release"};
        AdsrPhase state = Idle;
        float target = 0;
        int zeroCount =  0;

        inline float update( ) {

            switch ( state ) {
                case Attack:
                    act_value += attack;
                    if ( act_value >= target ) {
                        act_value = target;
                        target = sustain;
                        state = Decay;
                    }
                    break;

                case Decay:
                    if ( act_value > sustain ) {
                        act_value -= decay;
                        if ( act_value <= sustain ) {
                            act_value = sustain;
                            state = Sustain;
                        }
                    }
                    else {
                        act_value += decay; // attack target < sustainLevel level
                        if ( act_value >= sustain ) {
                            act_value = sustain;
                            state = Sustain;
                        }
                    }
                    break;

                case Release:
                    act_value -= release;
                    if ( act_value <= 0.0f ) {
                        act_value = 0.0;
                        state = Idle;
                    }
                    break;

                default:
                    // nothing to be done
                    break;
            }
            return act_value;
        }

};


/**
 * @brief Scales a dynamic parameter to the indicated range
 *
 */
class ScaledParameter : public AbstractParameter {
    public:
    ScaledParameter(AbstractParameter *parameter, float min, float max){
        this->min = min;
        this->max = max;
        this->p_parameter = parameter;
    }

    float update() {
        return p_parameter->update() + min * (max-min);
    }

    protected:
        float min=0, max=0;
        AbstractParameter *p_parameter;

};


}
