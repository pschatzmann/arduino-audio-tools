#pragma once
#include <math.h>
#include <stdint.h>

#include "AudioLogger.h"
#include "AudioParameters.h"
#include "AudioTools/CoreAudio/AudioBasic/q1_14_t.h"
#include "AudioTools/CoreAudio/AudioOutput.h"
#include "AudioTools/CoreAudio/AudioTypes.h"
#include "PitchShift.h"

namespace audio_tools {

// we use int16_t for our effects
typedef int16_t effect_t;

/**
 * @brief Abstract Base class for Sound Effects
 * @ingroup effects
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class AudioEffect {
 public:
  AudioEffect() = default;
  virtual ~AudioEffect() = default;

  /// calculates the effect output from the input
  virtual effect_t process(effect_t in) = 0;

  /// sets the effect active/inactive
  virtual void setActive(bool value) { active_flag = value; }

  /// determines if the effect is active
  virtual bool active() { return active_flag; }

  virtual AudioEffect* clone() = 0;

  /// Allows to identify an effect
  int id() { return id_value; }

  /// Allows to identify an effect
  void setId(int id) { this->id_value = id; }

 protected:
  bool active_flag = true;
  int id_value = -1;

  void copyParent(AudioEffect* copy) {
    id_value = copy->id_value;
    active_flag = copy->active_flag;
  }

  /// generic clipping method
  int16_t clip(int32_t in, int16_t clipLimit = 32767,
               int16_t resultLimit = 32767) {
    int32_t result = in;
    if (result > clipLimit) {
      result = resultLimit;
    }
    if (result < -clipLimit) {
      result = -resultLimit;
    }
    return result;
  }
};

/**
 * @brief Boost AudioEffect
 * @ingroup effects
 * @author Phil Schatzmann
 * @copyright GPLv3
 *
 */

class Boost : public AudioEffect, public VolumeSupport {
 public:
  /// Boost Constructor: volume 0.1 - 1.0: decrease result; volume >0: increase
  /// result
  Boost(float volume = 1.0) { setVolume(volume); }

  Boost(const Boost& copy) = default;

  /// define the actual volume in the range of 0.0f to 1.0f (values > 1.0f
  /// boost the signal, capped to q1_14_t's ~1.99994 range when
  /// PREFER_FIXEDPOINT is active)
  bool setVolume(float volume) override {
    bool result = VolumeSupport::setVolume(volume);
#if PREFER_FIXEDPOINT
    factor_q = q1_14_t(volume);
#endif
    return result;
  }

  effect_t process(effect_t input) {
    if (!active()) return input;
#if PREFER_FIXEDPOINT
    return factor_q.scale(input);
#else
    int32_t result = volume() * input;
    // clip to int16_t
    return clip(result);
#endif
  }

  Boost* clone() { return new Boost(*this); }

 protected:
#if PREFER_FIXEDPOINT
  q1_14_t factor_q{1.0f};
#endif
};

/**
 * @brief Distortion AudioEffect
 * @ingroup effects
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class Distortion : public AudioEffect {
 public:
  /// Distortion Constructor: e.g. use clipThreashold 4990 and maxInput=6500
  Distortion(int16_t clipThreashold = 4990, int16_t maxInput = 6500) {
    p_clip_threashold = clipThreashold;
    max_input = maxInput;
  }

  Distortion(const Distortion& copy) = default;

  void setClipThreashold(int16_t th) { p_clip_threashold = th; }

  int16_t clipThreashold() { return p_clip_threashold; }

  void setMaxInput(int16_t maxInput) { max_input = maxInput; }

  int16_t maxInput() { return max_input; }

  effect_t process(effect_t input) {
    if (!active()) return input;
    // the input signal is 16bits (values from -32768 to +32768
    // the value of input is clipped to the distortion_threshold value
    return clip(input, p_clip_threashold, max_input);
  }

  Distortion* clone() { return new Distortion(*this); }

 protected:
  int16_t p_clip_threashold;
  int16_t max_input;
};

/**
 * @brief Fuzz AudioEffect
 * @ingroup effects
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class Fuzz : public AudioEffect {
 public:
  /// Fuzz Constructor: use e.g. effectValue=6.5; maxOut = 300
  Fuzz(float fuzzEffectValue = 6.5, uint16_t maxOut = 300) {
    setFuzzEffectValue(fuzzEffectValue);
    max_out = maxOut;
  }

  Fuzz(const Fuzz& copy) = default;

  void setFuzzEffectValue(float v) {
    p_effect_value = v;
    // Q8.8: p_effect_value can exceed q1_14_t's +-2.0 range (default 6.5),
    // so a plain 8-fractional-bit integer scale is used instead -- still
    // integer-only in process(), just with a wider (+-128) integer range.
    v_fixed = (int32_t)(v * 256.0f + (v >= 0 ? 0.5f : -0.5f));
  }

  float fuzzEffectValue() { return p_effect_value; }

  void setMaxOut(uint16_t v) { max_out = v; }

  uint16_t maxOut() { return max_out; }

  effect_t process(effect_t input) {
    if (!active()) return input;
#if PREFER_FIXEDPOINT
    int32_t result = clip((v_fixed * input) >> 8);
    int32_t result2 = (result * v_fixed) >> 8;
    return map(result2, -32768, +32767, -max_out, max_out);
#else
    float v = p_effect_value;
    int32_t result = clip(v * input);
    return map(result * v, -32768, +32767, -max_out, max_out);
#endif
  }

  Fuzz* clone() { return new Fuzz(*this); }

 protected:
  float p_effect_value;
  int32_t v_fixed;  // Q8.8 fixed-point mirror of p_effect_value
  uint16_t max_out;
};

/**
 * @brief Tremolo AudioEffect
 * @ingroup effects
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class Tremolo : public AudioEffect {
 public:
  /// Tremolo constructor -  use e.g. duration_ms=2000; depthPercent=50;
  /// sampleRate=44100
  Tremolo(int16_t duration_ms = 2000, uint8_t depthPercent = 50,
          uint32_t sampleRate = 44100) {
    this->duration_ms = duration_ms;
    this->sampleRate = sampleRate;
    this->p_percent = depthPercent;
    int32_t rate_count = sampleRate * duration_ms / 1000;
    rate_count_half = rate_count / 2;
    recalculate();
  }

  Tremolo(const Tremolo& copy) = default;

  void setDuration(int16_t ms) {
    this->duration_ms = ms;
    int32_t rate_count = sampleRate * ms / 1000;
    rate_count_half = rate_count / 2;
    // tremolo_factor (float path) depends on rate_count_half too
    recalculate();
  }

  int16_t duration() { return duration_ms; }

  void setDepth(uint8_t percent) {
    p_percent = percent;
    recalculate();
  }

  uint8_t depth() { return p_percent; }

  effect_t process(effect_t input) {
    if (!active()) return input;
    // guards the division below; also matches the float path's original
    // (silent, non-crashing) behavior for this misconfiguration
    if (rate_count_half <= 0) return input;

#if PREFER_FIXEDPOINT
    // triangle-wave position weight (count/rate_count_half) applied to the
    // depth-scaled sample, all integer -- no per-sample float division
    int32_t out =
        signal_depth_q.scale(input) +
        (int32_t)(((int64_t)tremolo_depth_q.scale(input) * count) /
                   rate_count_half);
#else
    int32_t out = (int32_t)(signal_depth * input) +
                   (int32_t)(tremolo_factor * count * input);
#endif

    // saw tooth shaped counter
    count += inc;
    if (count >= rate_count_half) {
      inc = -1;
    } else if (count <= 0) {
      inc = +1;
    }

    return clip(out);
  }

  Tremolo* clone() { return new Tremolo(*this); }

 protected:
  int16_t duration_ms;
  uint32_t sampleRate;
  int32_t count = 0;
  int16_t inc = 1;
  int32_t rate_count_half;  // number of samples for on raise and fall
  uint8_t p_percent;
#if PREFER_FIXEDPOINT
  q1_14_t signal_depth_q{1.0f};
  q1_14_t tremolo_depth_q{0.0f};
#else
  float signal_depth = 1.0f;
  float tremolo_depth = 0.0f;
  float tremolo_factor = 0.0f;
#endif

  // p_percent only changes on setDepth(), so the depth split is computed
  // once here instead of every process() call
  void recalculate() {
    float td = p_percent > 100 ? 1.0f : 0.01f * p_percent;
    float sd = (100.0f - p_percent) / 100.0f;
#if PREFER_FIXEDPOINT
    tremolo_depth_q = q1_14_t(td);
    signal_depth_q = q1_14_t(sd);
#else
    tremolo_depth = td;
    signal_depth = sd;
    tremolo_factor = rate_count_half > 0 ? tremolo_depth / rate_count_half : 0.0f;
#endif
  }
};

/**
 * @brief Delay/Echo AudioEffect. See
 * https://wiki.analog.com/resources/tools-software/sharc-audio-module/baremetal/delay-effect-tutorial
 * Howver the dry value and wet value were replace by the depth parameter.
 * @ingroup effects
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class Delay : public AudioEffect {
 public:
  /// e.g. depth=0.5, ms=1000, sampleRate=44100
  Delay(uint16_t duration_ms = 1000, float depth = 0.5,
        float feedbackAmount = 1.0, uint32_t sampleRate = 44100) {
    setSampleRate(sampleRate);
    setFeedback(feedbackAmount);
    setDepth(depth);
    setDuration(duration_ms);
  }

  Delay(const Delay& copy) {
    setSampleRate(copy.sampleRate);
    setFeedback(copy.feedback);
    setDepth(copy.depth);
    setDuration(copy.duration);
  };

  void setDuration(int16_t dur) {
    duration = dur;
    updateBufferSize();
  }

  int16_t getDuration() { return duration; }

  void setDepth(float value) {
    depth = value;
    if (depth > 1.0f) depth = 1.0f;
    if (depth < 0.0f) depth = 0.0f;
#if PREFER_FIXEDPOINT
    depth_q = q1_14_t(depth);
    inv_depth_q = q1_14_t(1.0f) - depth_q;
#endif
  }

  float getDepth() { return depth; }

  void setFeedback(float feed) {
    feedback = feed;
    if (feedback > 1.0f) feedback = 1.0f;
    if (feedback < 0.0f) feedback = 0.0f;
#if PREFER_FIXEDPOINT
    feedback_q = q1_14_t(feedback);
#endif
  }

  float getFeedback() { return feedback; }

  void setSampleRate(int32_t sample) {
    sampleRate = sample;
    updateBufferSize();
  }

  float getSampleRate() { return sampleRate; }

  effect_t process(effect_t input) {
    if (!active()) return input;

    // Read last audio sample in each delay line
    effect_t delayed_value = buffer[delay_line_index];

#if PREFER_FIXEDPOINT
    // Mix the above with current audio and write the results back to output
    int32_t out = inv_depth_q.scale(input) + depth_q.scale(delayed_value);

    // Update each delay line
    // write = input + feedback * delayed_value (typical delay feedback);
    // scale() already rounds to nearest -- integer-only, no FPU needed
    int32_t write_int = (int32_t)input + feedback_q.scale(delayed_value);
#else
    // Mix the above with current audio and write the results back to output
    int32_t out = ((1.0f - depth) * input) + (depth * delayed_value);

    // Update each delay line
    // write = input + feedback * delayed_value (typical delay feedback)
    float write_val = (float)input + feedback * (float)delayed_value;
    // round to nearest integer to avoid truncation bias
    int32_t write_int = (int32_t)roundf(write_val);
#endif
    // clip to allowed range and store
    buffer[delay_line_index] = clip(write_int);

    // Finally, update the delay line index
    if (++delay_line_index >= delay_len_samples) {
      delay_line_index = 0;
    }
    return clip(out);
  }

  Delay* clone() { return new Delay(*this); }

 protected:
  Vector<effect_t> buffer{0};
  float feedback = 0.0f, duration = 0.0f, sampleRate = 0.0f, depth = 0.0f;
  size_t delay_len_samples = 0;
  size_t delay_line_index = 0;
#if PREFER_FIXEDPOINT
  q1_14_t feedback_q{0.0f}, depth_q{0.0f}, inv_depth_q{1.0f};
#endif

  void updateBufferSize() {
    if (sampleRate > 0 && duration > 0) {
      size_t newSampleCount = sampleRate * duration / 1000;
      if (newSampleCount != delay_len_samples) {
        delay_len_samples = newSampleCount;
        buffer.resize(delay_len_samples);
        memset(buffer.data(), 0, delay_len_samples * sizeof(effect_t));
        LOGD("sample_count: %u", (unsigned)delay_len_samples);
      }
    }
  }
};

/**
 * @brief ADSR Envelope: Attack, Decay, Sustain and Release.
 * Attack is the time taken for initial run-up oeffect_tf level from nil to
 peak, beginning when the key is pressed.
 * Decay is the time taken for the subsequent run down from the attack level to
 the designated sustainLevel level.
 * Sustain is the level during the main sequence of the sound's duration, until
 the key is released.
 * Release is the time taken for the level to decay from the sustainLevel level
 to zero after the key is released.[4]
 * @ingroup effects
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class ADSRGain : public AudioEffect {
 public:
  ADSRGain(float attack = 0.001, float decay = 0.001, float sustainLevel = 0.5,
           float release = 0.005, float boostFactor = 1.0) {
    setFactor(boostFactor);
    adsr = new ADSR(attack, decay, sustainLevel, release);
  }

  ADSRGain(const ADSRGain& ref) {
    adsr = new ADSR(*(ref.adsr));
    setFactor(ref.factor);
    copyParent((AudioEffect*)&ref);
  };

  virtual ~ADSRGain() { delete adsr; }

  void setAttackRate(float a) { adsr->setAttackRate(a); }

  float attackRate() { return adsr->attackRate(); }

  void setDecayRate(float d) { adsr->setDecayRate(d); }

  float decayRate() { return adsr->decayRate(); }

  void setSustainLevel(float s) { adsr->setSustainLevel(s); }

  float sustainLevel() { return adsr->sustainLevel(); }

  void setReleaseRate(float r) { adsr->setReleaseRate(r); }

  float releaseRate() { return adsr->releaseRate(); }

  void keyOn(float tgt = 0) { adsr->keyOn(tgt); }

  void keyOff() { adsr->keyOff(); }

  effect_t process(effect_t input) {
    if (!active()) return input;
#if PREFER_FIXEDPOINT
    q1_14_t gain = factor_q * adsr->tickFixed();
    return gain.scale(input);
#else
    effect_t result = factor * adsr->tick() * input;
    return result;
#endif
  }

  bool isActive() { return adsr->isActive(); }

  ADSRGain* clone() { return new ADSRGain(*this); }

 protected:
  ADSR* adsr;
  float factor;
#if PREFER_FIXEDPOINT
  q1_14_t factor_q{1.0f};
#endif

  void setFactor(float f) {
    factor = f;
#if PREFER_FIXEDPOINT
    factor_q = q1_14_t(f);
#endif
  }
};

/**
 * @brief Shifts the pitch by the indicated step size: e.g. 2 doubles the pitch
 * @author Phil Schatzmann
 * @ingroup effects
 * @copyright GPLv3
 */
class PitchShift : public AudioEffect {
 public:
  /// Boost Constructor: volume 0.1 - 1.0: decrease result; volume >0: increase
  /// result
  PitchShift(float shift_value = 1.0, int buffer_size = 1000) {
    effect_value = shift_value;
    size = buffer_size;
    buffer.resize(buffer_size);
    buffer.setIncrement(shift_value);
  }

  PitchShift(const PitchShift& ref) {
    size = ref.size;
    effect_value = ref.effect_value;
    buffer.resize(size);
    buffer.setIncrement(effect_value);
  };

  float value() { return effect_value; }

  void setValue(float value) {
    effect_value = value;
    buffer.setIncrement(value);
  }

  effect_t process(effect_t input) {
    if (!active()) return input;
    buffer.write(input);
    effect_t result;
    buffer.read(result);
    return result;
  }

  PitchShift* clone() { return new PitchShift(*this); }

 protected:
  VariableSpeedRingBuffer<int16_t> buffer;
  float effect_value;
  int size;
};

/**
 * @brief Compressor inspired by
 * https://github.com/YetAnotherElectronicsChannel/STM32_DSP_COMPRESSOR/blob/master/code/Src/main.c
 * @author Phil Schatzmann
 * @ingroup effects
 * @copyright GPLv3
 */

class Compressor : public AudioEffect {
 public:
  /// Copy Constructor
  Compressor(const Compressor& copy) = default;

  /// Default Constructor
  Compressor(uint32_t sampleRate = 44100, uint16_t attackMs = 30,
             uint16_t releaseMs = 20, uint16_t holdMs = 10,
             uint8_t thresholdPercent = 10, float compressionRatio = 0.5) {
    // assuming 1 sample = 1/96kHz = ~10us
    // Attack -> 30 ms -> 3000
    // Release -> 20 ms -> 2000
    // Hold -> 10ms -> 1000
    sample_rate = sampleRate;
    attack_count = sample_rate * attackMs / 1000;
    release_count = sample_rate * releaseMs / 1000;
    hold_count = sample_rate * holdMs / 1000;

    setThresholdPercent(thresholdPercent);
    // compression ratio: 6:1 -> -6dB = 0.5
    gainreduce = compressionRatio;
    // initial gain = 1.0 -> no compression
    gain = 1.0f;
    recalculate();
  }

  /// Defines the attack duration in ms
  void setAttack(uint16_t attackMs) {
    attack_count = sample_rate * attackMs / 1000;
    recalculate();
  }

  /// Defines the release duration in ms
  void setRelease(uint16_t releaseMs) {
    release_count = sample_rate * releaseMs / 1000;
    recalculate();
  }

  /// Defines the hold duration in ms
  void setHold(uint16_t holdMs) {
    hold_count = sample_rate * holdMs / 1000;
    recalculate();
  }

  /// Defines the threshod in %
  void setThresholdPercent(uint8_t thresholdPercent) {
    // exact integer fraction of full-scale -- no float needed regardless of
    // platform (threshold -20dB below limit -> 10% of 2^15)
    threshold = (int32_t)NumberConverter::maxValueT<effect_t>() *
                thresholdPercent / 100;
  }

  /// Defines the compression ratio from 0 to 1
  void setCompressionRatio(float compressionRatio) {
    if (compressionRatio < 1.0f) {
      gainreduce = compressionRatio;
    }
    recalculate();
  }

  /// Processes the sample
  effect_t process(effect_t input) {
    if (!active()) return input;
    return compress(input);
  }

  Compressor* clone() { return new Compressor(*this); }

 protected:
  enum CompStates { S_NoOperation, S_Attack, S_GainReduction, S_Release };
  enum CompStates state = S_NoOperation;

  int32_t attack_count, release_count, hold_count, timeout;
  int32_t threshold;  // exact fraction of full-scale -- pure integer
  uint32_t sample_rate;
#if PREFER_FIXEDPOINT
  // Q1.14's ~6.1e-5 resolution on gain_step_attack/release (added every
  // sample for attack_count/release_count samples) accumulates a bounded
  // gain error of a few percent under fast/extreme attack-release cycling
  // -- acceptable for this musical effect, and far cheaper than a per-sample
  // float multiply-add chain on an FPU-less MCU.
  q1_14_t gainreduce{0.5f}, gain_step_attack{0.0f}, gain_step_release{0.0f},
      gain{1.0f};
#else
  float gainreduce, gain_step_attack, gain_step_release, gain;
#endif

  void recalculate() {
    // done once per parameter change, not per sample -- float here is fine
    // even under PREFER_FIXEDPOINT (see VolumeStream::setVolume() for the
    // same pattern)
    float step_a = (1.0f - (float)gainreduce) / attack_count;
    float step_r = (1.0f - (float)gainreduce) / release_count;
#if PREFER_FIXEDPOINT
    gain_step_attack = q1_14_t(step_a);
    gain_step_release = q1_14_t(step_r);
#else
    gain_step_attack = step_a;
    gain_step_release = step_r;
#endif
  }

  effect_t compress(effect_t inSample) {
    int32_t mag = inSample < 0 ? -(int32_t)inSample : (int32_t)inSample;
    if (mag > threshold) {
      if (gain >= gainreduce) {
        if (state == S_NoOperation) {
          state = S_Attack;
          timeout = attack_count;
        } else if (state == S_Release) {
          state = S_Attack;
          timeout = attack_count;
        }
      }
      if (state == S_GainReduction) timeout = hold_count;
    }

    if (mag < threshold && gain <= 1.0f) {
      if (timeout == 0 && state == S_GainReduction) {
        state = S_Release;
        timeout = release_count;
      }
    }

    switch (state) {
      case S_Attack:
        if (timeout > 0 && gain > gainreduce) {
          gain -= gain_step_attack;
          timeout--;
        } else {
          state = S_GainReduction;
          timeout = hold_count;
        }
        break;

      case S_GainReduction:
        if (timeout > 0)
          timeout--;
        else {
          state = S_Release;
          timeout = release_count;
        }
        break;

      case S_Release:
        if (timeout > 0 && gain < 1.0f) {
          timeout--;
          gain += gain_step_release;
        } else {
          state = S_NoOperation;
        }
        break;

      case S_NoOperation:
        if (gain < 1.0f) gain = 1.0f;
        break;

      default:
        break;
    }

#if PREFER_FIXEDPOINT
    return gain.scale(inSample);
#else
    return (effect_t)(gain * inSample);
#endif
  }
};

}  // namespace audio_tools
