#pragma once
#include "AudioToolsConfig.h"
#include "AudioTools/CoreAudio/AudioLogger.h"
#include "AudioTools/CoreAudio/AudioBasic/q1_14_t.h"

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
            setAttackRate(attack);
            setDecayRate(decay);
            setSustainLevel(sustainLevel);
            setReleaseRate(release);
        }

        ADSR(ADSR &copy) = default;

        void setAttackRate(float a){
            attack = a;
#if PREFER_FIXEDPOINT
            attack_q = q1_14_t(a);
#endif
        }

        float attackRate(){
            return attack;
        }

        void setDecayRate(float d){
            decay = d;
#if PREFER_FIXEDPOINT
            decay_q = q1_14_t(d);
#endif
        }

        float decayRate() {
            return decay;
        }

        void setSustainLevel(float s){
            sustain = s;
#if PREFER_FIXEDPOINT
            sustain_q = q1_14_t(s);
#endif
        }

        float sustainLevel(){
            return sustain;
        }

        void setReleaseRate(float r){
            release = r;
#if PREFER_FIXEDPOINT
            release_q = q1_14_t(r);
#endif
        }

        float releaseRate() {
            return release;
        }

        void keyOn(float tgt=0){
            LOGI("%s: %f", LOG_METHOD, tgt);
            state = Attack;
            this->target = tgt>0.0f && tgt<=1.0f ? tgt : sustain;
            this->act_value = 0;
#if PREFER_FIXEDPOINT
            target_q = q1_14_t(this->target);
            act_value_q = q1_14_t(0.0f);
#endif
        }

        void keyOff(){
            TRACEI();
            if (state!=Idle){
                state = Release;
                target = 0;
#if PREFER_FIXEDPOINT
                target_q = q1_14_t(0.0f);
#endif
            }
        }

        bool isActive(){
            return state!=Idle;
        }

#if PREFER_FIXEDPOINT
        /// Fixed-point envelope tick: integer-only per-sample math (no FPU
        /// needed). Prefer this over tick()/value() in per-sample hot paths
        /// (e.g. ADSRGain::process()); tick() still works but pays for one
        /// float conversion per call.
        q1_14_t tickFixed() {
            act_value_q = updateFixed();
            act_value = (float)act_value_q;
            return act_value_q;
        }
#endif

    protected:
        float attack,  decay,  sustain,  release;
        enum AdsrPhase {Idle, Attack, Decay, Sustain, Release};
        const char* adsrNames[5] = {"Idle", "Attack", "Decay", "Sustain", "Release"};
        AdsrPhase state = Idle;
        float target = 0;
        int zeroCount =  0;

#if PREFER_FIXEDPOINT
        q1_14_t attack_q{0.001f}, decay_q{0.001f}, sustain_q{0.5f}, release_q{0.005f};
        q1_14_t target_q{0.0f}, act_value_q{0.0f};

        q1_14_t updateFixed() {
            switch ( state ) {
                case Attack:
                    act_value_q += attack_q;
                    if ( act_value_q >= target_q ) {
                        act_value_q = target_q;
                        target_q = sustain_q;
                        target = sustain;
                        state = Decay;
                    }
                    break;

                case Decay:
                    if ( act_value_q > sustain_q ) {
                        act_value_q -= decay_q;
                        if ( act_value_q <= sustain_q ) {
                            act_value_q = sustain_q;
                            state = Sustain;
                        }
                    }
                    else {
                        act_value_q += decay_q; // attack target < sustainLevel level
                        if ( act_value_q >= sustain_q ) {
                            act_value_q = sustain_q;
                            state = Sustain;
                        }
                    }
                    break;

                case Release:
                    act_value_q -= release_q;
                    if ( act_value_q <= q1_14_t(0.0f) ) {
                        act_value_q = q1_14_t(0.0f);
                        state = Idle;
                    }
                    break;

                default:
                    // nothing to be done
                    break;
            }
            return act_value_q;
        }
#endif

        inline float update( ) {
#if PREFER_FIXEDPOINT
            return (float)updateFixed();
#else
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
#endif
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
