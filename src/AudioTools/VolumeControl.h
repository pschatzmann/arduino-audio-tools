#pragma once

/**
 * @defgroup volume Volume
 * @ingroup dsp
 * @brief Volume Calculations
 */
namespace audio_tools {

/**
 * @brief Abstract class for handling of the linear input volume to determine the multiplication factor which should be applied to the audio signal 
 * @ingroup volume
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class VolumeControl {
    public:
        /// determines a multiplication factor (0.0 to 1.0) from an input value (0.0 to 1.0).
        virtual float getVolumeFactor(float volume) = 0;

    protected:

        /// limits the output to the range of 0 to 1.0
        virtual float limit(float in){
            float result = in;
            if (result<0.0f) result = 0;
            if (result>1.0f) result = 1;
            return result;
        }

};

/**
 * @brief In order to optimize the processing time we cache the last input & factor and recalculate the new factor only if the input has changed.
 * @ingroup volume
 * @author Phil Schatzmann
 * @copyright GPLv3 
 */
class CachedVolumeControl : public VolumeControl {
    public:
        CachedVolumeControl(VolumeControl &vc){
            setVolumeControl(vc);
        }

        CachedVolumeControl(VolumeControl *vc){
            if (vc!=nullptr) setVolumeControl(*vc);
        }

        void setVolumeControl(VolumeControl &vc){
            p_vc = &vc;
        }

        /// determines a multiplication factor (0.0 to 1.0) from an input value (0.0 to 1.0).
        virtual float getVolumeFactor(float volume) {
            if (p_vc==nullptr) return 1.0f; // prevent NPE
            if (abs(volume-in)<0.01f){
                return out;
            } else {
                in = volume;
                out = p_vc->getVolumeFactor(volume);
                return out;
            }
        }

    protected:
        VolumeControl *p_vc=nullptr;
        float in=1.0, out=1.0;

};


/**
 * @brief Parametric Logarithmic volume control. Using the formula pow(b,input) * a - a, where b is b = pow(((1/ym)-1), 2) and a is a = 1.0 / (b - 1.0). 
 * The parameter ym is determining the steepness.
 * See https://electronics.stackexchange.com/questions/304692/formula-for-logarithmic-audio-taper-pot
 * @ingroup volume
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class LogarithmicVolumeControl : public VolumeControl {
    public:
        LogarithmicVolumeControl(float ym=0.1){
            this->ym = ym;
        }

        // provides a factor in the range of 0.0 to 1.0
        virtual float getVolumeFactor(float input) {
            float b = pow(((1/ym)-1), 2);
            float a = 1.0f / (b - 1.0f);
            float volumeFactor = pow(b,input) * a - a;
            return limit(volumeFactor);
        }

    protected:
        float ym;
};

/**
 * @brief Simple exponentional volume control using the formula pow(2.0, input) - 1.0;
 * @ingroup volume
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class ExponentialVolumeControl : public VolumeControl {
    public:
        // provides a factor in the range of 0.0 to 1.0
        virtual float getVolumeFactor(float volume) {
            float volumeFactor = pow(2.0, volume) - 1.0;
            return limit(volumeFactor);
        }
};

/**
 * @brief Simple simulated audio pot volume control inspired by https://eepower.com/resistor-guide/resistor-types/potentiometer-taper/#
 * We split up the input/output curve into 2 linear pieces with a slow and a fast raising part. 
 * The slow raising part goes from (0,0) to (x,y).
 * The fast raising part goes from (x,y) to (1,1).
 * @ingroup volume
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class SimulatedAudioPot : public VolumeControl {
    public:
        SimulatedAudioPot(float x=0.5, float y=0.1){
            this->x = x;
            this->y = y;
        }
        virtual float getVolumeFactor(float volume) {
            float result = 0;
            if (volume<=x){
                result = mapFloat(volume, 0.0, x, 0, y );
            } else {
                result = mapFloat(volume, x, 1.0, y, 1.0);
            }
            return limit(result);
        }
    protected:
        float x, y;
};

/**
 * @brief The simplest possible implementation of a VolumeControl: The input = output which describes a linear curve.
 * You would use this implementation if you physically connect an audio pot!
 * @ingroup volume
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class LinearVolumeControl : public VolumeControl {
    public:
        LinearVolumeControl(bool allowBoost=false){
            is_limited = !allowBoost;
        }
        // provides a factor in the range of 0.0 to 1.0
        virtual float getVolumeFactor(float volume) {
            return is_limited ? limit(volume) : volume;
        }
    protected:
        bool is_limited;
};


/**
 * @brief Provide the volume function as callback method: This is easy to use e.g together with a lamda function!
 * @ingroup volume
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class CallbackVolumeControl : public VolumeControl {
    public:
        CallbackVolumeControl(float(*cb)(float in)){
            callback = cb;
        }
        // provides a factor in the range of 0.0 to 1.0
        virtual float getVolumeFactor(float volume) {
            return limit(callback(volume));
        }
    protected:
        float (*callback)(float in) = nullptr;

};

} // namespace