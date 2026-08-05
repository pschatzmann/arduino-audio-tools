#pragma once

#include <math.h>

#include "AudioTools/CoreAudio/AudioBasic/Collections.h"
#include "AudioTools/CoreAudio/AudioLogger.h"
#include "AudioTools/CoreAudio/AudioTypes.h"
#include "AudioTools/CoreAudio/Buffers.h"

/**
 * @defgroup generator Generators
 * @ingroup dsp
 * @brief Sound Generators
 **/

namespace audio_tools {

/**
 * @brief Base class to define the abstract interface for the sound generating
 * classes
 * @ingroup generator
 * @author Phil Schatzmann
 * @copyright GPLv3
 *
 * @tparam T
 */
template <class T = int16_t>
class SoundGenerator {
 public:
  SoundGenerator() { info.bits_per_sample = sizeof(T) * 8; }

  virtual ~SoundGenerator() { end(); }

  /// Starts the processing with the provided AudioInfo
  virtual bool begin(AudioInfo info) {
    this->info = info;
    return begin();
  }

  /// Starts the processing 
  virtual bool begin() {
    TRACED();
    active = true;
    activeWarningIssued = false;
    info.logInfo("SoundGenerator:");

    // support bytes < framesize
    ring_buffer.resize(info.channels * sizeof(T));

    return true;
  }

  /// Ends the processing
  virtual void end() { active = false; }

  /// Checks if the begin method has been called - after end() isActive is false
  virtual bool isActive() { return active; }

  /// Provides a single sample
  virtual T readSample() = 0;

  /// Provides the data as byte array with the requested number of channels
  virtual size_t readBytes(uint8_t* data, size_t len) {
    LOGD("readBytes: %d", (int)len);
    if (!active) return 0;
    int channels = audioInfo().channels;
    int frame_size = sizeof(T) * channels;
    int frames = len / frame_size;
    if (len >= frame_size) {
      return readBytesFrames(data, len, frames, channels);
    }
    return readBytesFromBuffer(data, len, frame_size, channels);
  }

  /// Provides the default configuration
  virtual AudioInfo defaultConfig() {
    AudioInfo def;
    def.bits_per_sample = sizeof(T) * 8;
    return def;
  }

  /// Abstract method: not implemented! Just provides an error message...
  virtual void setFrequency(float frequency) {
    LOGE("setFrequency not supported");
  }

  /// Provides the AudioInfo
  virtual AudioInfo audioInfo() { return info; }

  /// Defines/updates the AudioInfo
  virtual void setAudioInfo(AudioInfo info) {
    this->info = info;
    if (info.bits_per_sample != sizeof(T) * 8) {
      LOGE("invalid bits_per_sample: %d", info.channels);
    }
    recalculatePlayTime();
  }

  /// Defines the play time in ms and the ramp up and ramp down time in percent
  void setPlayTime(uint32_t playMs, uint8_t upPercent = 20,
                   uint8_t downPercent = 30) {
    LOGI("setPlayTime: playMs=%d, upPercent=%d, downPercent=%d", playMs,
         upPercent, downPercent);
    this->playMs = playMs;
    this->upPercent = upPercent;
    this->downPercent = downPercent;
    currentSample = 0;
    recalculatePlayTime();
    factor = 0.0f;
  }

 protected:
  bool active = false;
  bool activeWarningIssued = false;
  // int output_channels = 1;
  AudioInfo info;
  RingBuffer<uint8_t> ring_buffer{0};
  uint32_t playMs = 0;
  uint8_t upPercent = 5;
  uint8_t downPercent = 40;
  uint32_t playSamples = 0;
  uint32_t upSamples = 0;
  uint32_t rampDownSamples = 0;
  float rampUpInc = 0.0;
  float rampDownDec = 0.0;
  float factor = 1.0f;
  uint32_t currentSample = 0;

  void recalculatePlayTime() {
    if (upPercent + downPercent > 100) {
      downPercent = 100 - upPercent;
    }
    playSamples = info.sample_rate / 1000 * playMs;
    upSamples = (playSamples * upPercent) / 100;
    rampDownSamples = (playSamples * downPercent) / 100;
    rampUpInc = 0;
    if (upSamples > 0) {
      rampUpInc = 1.0f / upSamples;
    }
    rampDownDec = 0;
    if (rampDownSamples > 0) {
      rampDownDec = 1.0f / rampDownSamples;
    }
  }

  size_t readBytesFrames(uint8_t* buffer, size_t lengthBytes, int frames,
                         int channels) {
    T* result_buffer = (T*)buffer;
    int frames_written = 0;
    if (playMs > 0 && currentSample > playSamples) {
      return 0;
    }

    for (int j = 0; j < frames; j++) {
      T sample = readSample();

      // if we requested a play time
      if (playMs > 0) {
        currentSample++;
        sample = applyRamp(sample);
      }

      for (int ch = 0; ch < channels; ch++) {
        *result_buffer++ = sample;
      }

      frames_written++;
      // exit loop if we have reached the requested play time
      if (playMs > 0 && currentSample > playSamples) {
        break;
      }
    }
    return frames_written * sizeof(T) * channels;
  }

  // Applies ramp up and ramp down logic to the sample
  T applyRamp(T sample) {
    // Ramp up
    if (rampUpInc > 0 && currentSample <= upSamples) {
      factor += rampUpInc;
      if (factor > 1.0f) {
        factor = 1.0f;
      }
    }
    // Ramp down
    else if (rampDownDec > 0 && currentSample >= playSamples - rampDownSamples) {
      factor -= rampDownDec;
      if (factor < 0.0f) {
        factor = 0.0f;
      }
    }
    // Sustain
    else {
      factor = 1.0f;
    }
    return (T)(factor * sample);
  }

  size_t readBytesFromBuffer(uint8_t* buffer, size_t lengthBytes,
                             int frame_size, int channels) {
    // fill ringbuffer with one frame
    if (ring_buffer.isEmpty()) {
      uint8_t tmp[frame_size];
      readBytesFrames(tmp, frame_size, 1, channels);
      ring_buffer.writeArray(tmp, frame_size);
    }
    // provide result
    return ring_buffer.readArray(buffer, lengthBytes);
  }
};

/**
 * @brief Generates a Sound with the help of sin() function.
 * If performance is of concern, I suggest to use theSineFromTable,
 * the FastSineGenerator or the FastIntSineGenerator.
 * @note Prefer SineGenerator instead of using this class directly: it
 * automatically picks this (float) or FastIntSineGenerator, whichever is
 * optimal for the target platform (see PREFER_FIXEDPOINT).
 * @ingroup generator
 * @author Phil Schatzmann
 * @copyright GPLv3
 *
 */
template <class T = int16_t>
class FloatSineGenerator : public SoundGenerator<T> {
 public:
  // the scale defines the max value which is generated
  FloatSineGenerator(float amplitude = NumberConverter::maxValueT<T>(),
                    float phase = 0.0f) {
    LOGD("FloatSineGenerator");
    m_amplitude = amplitude;
    m_phase = phase;
  }

  bool begin() override {
    TRACEI();
    SoundGenerator<T>::begin();
    this->m_deltaTime = 1.0f / SoundGenerator<T>::info.sample_rate;
    return true;
  }

  bool begin(AudioInfo info) override {
    LOGI("%s::begin(channels=%d, sample_rate=%d)", "FloatSineGenerator",
         (int)info.channels, (int)info.sample_rate);
    SoundGenerator<T>::begin(info);
    this->m_deltaTime = 1.0f / SoundGenerator<T>::info.sample_rate;
    return true;
  }

  bool begin(AudioInfo info, float frequency) {
    LOGI("%s::begin(channels=%d, sample_rate=%d, frequency=%.2f)",
         "FloatSineGenerator", (int)info.channels, (int)info.sample_rate,
         frequency);
    SoundGenerator<T>::begin(info);
    this->m_deltaTime = 1.0f / SoundGenerator<T>::info.sample_rate;
    if (frequency > 0.0f) {
      setFrequency(frequency);
    }
    return true;
  }

  bool begin(int channels, int sample_rate, float frequency) {
    SoundGenerator<T>::info.channels = channels;
    SoundGenerator<T>::info.sample_rate = sample_rate;
    return begin(SoundGenerator<T>::info, frequency);
  }

  // update m_deltaTime
  virtual void setAudioInfo(AudioInfo info) override {
    SoundGenerator<T>::setAudioInfo(info);
    this->m_deltaTime = 1.0f / SoundGenerator<T>::info.sample_rate;
  }

  virtual AudioInfo defaultConfig() override {
    return SoundGenerator<T>::defaultConfig();
  }

  /// Defines the frequency - after the processing has been started
  void setFrequency(float frequency) override {
    LOGI("setFrequency: %.2f", frequency);
    LOGI("active: %s", SoundGenerator<T>::active ? "true" : "false");
    if (m_frequency != frequency) {
      m_cycles = 0.0f;  // reset cycles to avoid phase jumps
      m_phase = 0.0f;   // reset phase to avoid jumps
    }
    m_frequency = frequency;
  }

  /// Provides a single sample
  virtual T readSample() override {
    float angle = double_Pi * m_cycles + m_phase;
    T result = m_amplitude * sinf(angle);
    m_cycles += m_frequency * m_deltaTime;
    if (m_cycles > 1.0f) {
      m_cycles -= 1.0f;
    }
    return result;
  }

  void setAmplitude(float amp) { m_amplitude = amp; }

 protected:
  volatile float m_frequency = 0.0f;
  float m_cycles = 0.0f;  // Varies between 0.0 and 1.0
  float m_amplitude = 1.0f;
  float m_deltaTime = 0.0f;
  float m_phase = 0.0f;
  const float double_Pi = 2.0f * PI;

  void logStatus() {
    SoundGenerator<T>::info.logStatus();
    LOGI("amplitude: %f", this->m_amplitude);
    LOGI("active: %s", SoundGenerator<T>::active ? "true" : "false");
  }
};

/**
 * @brief Sine wave which is based on a fast approximation function using floating point math.
 * @ingroup generator
 * @author Vivian Leigh Stewart
 * @copyright GPLv3
 * @tparam T
 */
template <class T = int16_t>
class FastSineGenerator : public FloatSineGenerator<T> {
 public:
  FastSineGenerator(float amplitude = NumberConverter::maxValueT<T>(), float phase = 0.0)
      : FloatSineGenerator<T>(amplitude, phase) {
    LOGD("FastSineGenerator");
  }

  virtual T readSample() override {
    float angle =
        FloatSineGenerator<T>::m_cycles + FloatSineGenerator<T>::m_phase;
    T result = FloatSineGenerator<T>::m_amplitude * sine(angle);
    FloatSineGenerator<T>::m_cycles +=
        FloatSineGenerator<T>::m_frequency * FloatSineGenerator<T>::m_deltaTime;
    if (FloatSineGenerator<T>::m_cycles > 1.0f) {
      FloatSineGenerator<T>::m_cycles -= 1.0f;
    }
    return result;
  }

 protected:
  /// sine approximation.
  inline float sine(float t) {
    float p = (t - (int)t) - 0.5f;  // 0 <= p <= 1
    float pp = p * p;
    return (p - 6.283211f * pp * p + 9.132843f * pp * pp * p) * -6.221086f;
  }
};

/**
 * @brief Sine wave generator that does not use any floating point operations
 * in the readSample() hot path: it advances a 32 bit phase accumulator with
 * an integer increment and looks up the result in a Q15 fixed point table.
 * All floating point math (converting frequency/sample_rate into the phase
 * increment) is done once in setFrequency()/setAudioInfo(), never per
 * sample. This makes it a good fit for MCUs without a hardware FPU - e.g.
 * the RP2040 - where sinf() is comparatively expensive.
 * @note Prefer SineGenerator instead of using this class directly: it
 * automatically picks this (fixed-point) or FloatSineGenerator, whichever
 * is optimal for the target platform (see PREFER_FIXEDPOINT).
 * @ingroup generator
 * @author Phil Schatzmann
 * @copyright GPLv3
 * @tparam T
 */
template <class T = int16_t>
class FastIntSineGenerator : public SoundGenerator<T> {
 public:
  FastIntSineGenerator(float amplitude = NumberConverter::maxValueT<T>(),
                        float phase = 0.0f) {
    LOGD("FastIntSineGenerator");
    setAmplitude(amplitude);
    setPhase(phase);
  }

  bool begin() override {
    TRACEI();
    SoundGenerator<T>::begin();
    updatePhaseIncrement();
    return true;
  }

  bool begin(AudioInfo info) override {
    LOGI("%s::begin(channels=%d, sample_rate=%d)", "FastIntSineGenerator",
         (int)info.channels, (int)info.sample_rate);
    SoundGenerator<T>::begin(info);
    updatePhaseIncrement();
    return true;
  }

  bool begin(AudioInfo info, float frequency) {
    SoundGenerator<T>::begin(info);
    if (frequency > 0.0f) {
      setFrequency(frequency);
    }
    return true;
  }

  bool begin(int channels, int sample_rate, float frequency) {
    SoundGenerator<T>::info.channels = channels;
    SoundGenerator<T>::info.sample_rate = sample_rate;
    return begin(SoundGenerator<T>::info, frequency);
  }

  virtual void setAudioInfo(AudioInfo info) override {
    SoundGenerator<T>::setAudioInfo(info);
    updatePhaseIncrement();
  }

  /// Defines the frequency - the only place where float math is used
  void setFrequency(float frequency) override {
    LOGI("setFrequency: %.2f", frequency);
    m_frequency = frequency;
    updatePhaseIncrement();
  }

  /// Defines the starting phase in radians
  void setPhase(float phase) {
    double turns = phase / (2.0 * PI);
    m_phase_offset = (uint32_t)(turns * 4294967296.0);
  }

  void setAmplitude(float amp) {
    m_amplitude = amp;
    m_amplitude_i = (int32_t)amp;
  }

  /// Provides a single sample - integer only, no float ops or sin() calls!
  virtual T readSample() override {
    // top kTableBits of the 32 bit accumulator select the table entry
    uint32_t index = (m_phase_acc + m_phase_offset) >> kIndexShift;
    m_phase_acc += m_phase_increment;
    int32_t sine_q15 = sine_table[index];
    return (T)(((int64_t)sine_q15 * m_amplitude_i) >> 15);
  }

 protected:
  static const int kTableBits = 8;
  static const int kTableSize = 1 << kTableBits;    // 256 entries
  static const int kIndexShift = 32 - kTableBits;

  volatile float m_frequency = 0.0f;
  float m_amplitude = 1.0f;
  int32_t m_amplitude_i = 32767;
  // fixed point (32 bit) phase: wrapping is implicit via unsigned overflow
  uint32_t m_phase_acc = 0;
  uint32_t m_phase_offset = 0;
  uint32_t m_phase_increment = 0;

  // only called from begin()/setFrequency()/setAudioInfo(), never per sample
  void updatePhaseIncrement() {
    uint32_t sample_rate = SoundGenerator<T>::info.sample_rate;
    if (sample_rate > 0) {
      m_phase_increment =
          (uint32_t)((double)m_frequency / sample_rate * 4294967296.0);
    }
  }

  // one sine period in Q15 fixed point (-32767..32767)
  static constexpr int16_t sine_table[kTableSize] = {
      0,     804,   1608,  2410,  3212,  4011,  4808,  5602,  6393,  7179,
      7962,  8739,  9512,  10278, 11039, 11793, 12539, 13279, 14010, 14732,
      15446, 16151, 16846, 17530, 18204, 18868, 19519, 20159, 20787, 21403,
      22005, 22594, 23170, 23731, 24279, 24811, 25329, 25832, 26319, 26790,
      27245, 27683, 28105, 28510, 28898, 29268, 29621, 29956, 30273, 30571,
      30852, 31113, 31356, 31580, 31785, 31971, 32137, 32285, 32412, 32521,
      32609, 32678, 32728, 32757, 32767, 32757, 32728, 32678, 32609, 32521,
      32412, 32285, 32137, 31971, 31785, 31580, 31356, 31113, 30852, 30571,
      30273, 29956, 29621, 29268, 28898, 28510, 28105, 27683, 27245, 26790,
      26319, 25832, 25329, 24811, 24279, 23731, 23170, 22594, 22005, 21403,
      20787, 20159, 19519, 18868, 18204, 17530, 16846, 16151, 15446, 14732,
      14010, 13279, 12539, 11793, 11039, 10278, 9512,  8739,  7962,  7179,
      6393,  5602,  4808,  4011,  3212,  2410,  1608,  804,   0,     -804,
      -1608, -2410, -3212, -4011, -4808, -5602, -6393, -7179, -7962, -8739,
      -9512, -10278,-11039,-11793,-12539,-13279,-14010,-14732,-15446,-16151,
      -16846,-17530,-18204,-18868,-19519,-20159,-20787,-21403,-22005,-22594,
      -23170,-23731,-24279,-24811,-25329,-25832,-26319,-26790,-27245,-27683,
      -28105,-28510,-28898,-29268,-29621,-29956,-30273,-30571,-30852,-31113,
      -31356,-31580,-31785,-31971,-32137,-32285,-32412,-32521,-32609,-32678,
      -32728,-32757,-32767,-32757,-32728,-32678,-32609,-32521,-32412,-32285,
      -32137,-31971,-31785,-31580,-31356,-31113,-30852,-30571,-30273,-29956,
      -29621,-29268,-28898,-28510,-28105,-27683,-27245,-26790,-26319,-25832,
      -25329,-24811,-24279,-23731,-23170,-22594,-22005,-21403,-20787,-20159,
      -19519,-18868,-18204,-17530,-16846,-16151,-15446,-14732,-14010,-13279,
      -12539,-11793,-11039,-10278,-9512, -8739, -7962, -7179, -6393, -5602,
      -4808, -4011, -3212, -2410, -1608, -804};
};

/// Default sine generator: FastIntSineGenerator (no FPU needed) on platforms
/// that set PREFER_FIXEDPOINT, FloatSineGenerator (sinf() based) otherwise.
#if PREFER_FIXEDPOINT
template <class T = int16_t>
using SineGenerator = FastIntSineGenerator<T>;
#else
template <class T = int16_t>
using SineGenerator = FloatSineGenerator<T>;
#endif

/// Alias for SineGenerator
template <class T = int16_t>
using SineWaveGenerator = SineGenerator<T>;

/**
 * @brief Generates a square wave sound. Uses the same 32 bit phase
 * accumulator as SawToothGenerator/FastIntSineGenerator: no floating point
 * operations or sin() calls in the readSample() hot path. A square wave
 * needs neither a sine approximation nor a lookup table - just the sign
 * bit of the ramp - so readSample() is a single comparison, making this a
 * good fit for MCUs without a hardware FPU (e.g. RP2040), where the
 * previous FastSineGenerator based implementation still did a float
 * polynomial evaluation per sample just to keep its sign.
 * @ingroup generator
 * @author Phil Schatzmann
 * @copyright GPLv3
 *
 */
template <class T = int16_t>
class SquareWaveGenerator : public SoundGenerator<T> {
 public:
  SquareWaveGenerator(float amplitude = NumberConverter::maxValueT<T>(),
                       float phase = 0.0f) {
    LOGD("SquareWaveGenerator");
    setAmplitude(amplitude);
    setPhase(phase);
  }

  bool begin() override {
    TRACEI();
    SoundGenerator<T>::begin();
    updatePhaseIncrement();
    return true;
  }

  bool begin(AudioInfo info) override {
    LOGI("%s::begin(channels=%d, sample_rate=%d)", "SquareWaveGenerator",
         (int)info.channels, (int)info.sample_rate);
    SoundGenerator<T>::begin(info);
    updatePhaseIncrement();
    return true;
  }

  bool begin(AudioInfo info, float frequency) {
    SoundGenerator<T>::begin(info);
    if (frequency > 0.0f) {
      setFrequency(frequency);
    }
    return true;
  }

  bool begin(int channels, int sample_rate, float frequency) {
    SoundGenerator<T>::info.channels = channels;
    SoundGenerator<T>::info.sample_rate = sample_rate;
    return begin(SoundGenerator<T>::info, frequency);
  }

  virtual void setAudioInfo(AudioInfo info) override {
    SoundGenerator<T>::setAudioInfo(info);
    updatePhaseIncrement();
  }

  /// Defines the frequency - the only place where float math is used
  void setFrequency(float frequency) override {
    LOGI("setFrequency: %.2f", frequency);
    m_frequency = frequency;
    updatePhaseIncrement();
  }

  /// Defines the starting phase in radians
  void setPhase(float phase) {
    double turns = phase / (2.0 * PI);
    m_phase_offset = (uint32_t)(turns * 4294967296.0);
  }

  void setAmplitude(float amp) {
    m_amplitude = amp;
    m_amplitude_i = (int32_t)amp;
  }

  /// Provides a single sample - integer only, no float ops!
  virtual T readSample() override {
    uint32_t phase = m_phase_acc + m_phase_offset;
    m_phase_acc += m_phase_increment;
    // top bit of the phase marks the half of the cycle we're in
    return (int32_t)phase >= 0 ? (T)m_amplitude_i : (T)(-m_amplitude_i);
  }

 protected:
  volatile float m_frequency = 0.0f;
  float m_amplitude = 1.0f;
  int32_t m_amplitude_i = 32767;
  // fixed point (32 bit) phase: wrapping is implicit via unsigned overflow
  uint32_t m_phase_acc = 0;
  uint32_t m_phase_offset = 0;
  uint32_t m_phase_increment = 0;

  // only called from begin()/setFrequency()/setAudioInfo(), never per sample
  void updatePhaseIncrement() {
    uint32_t sample_rate = SoundGenerator<T>::info.sample_rate;
    if (sample_rate > 0) {
      m_phase_increment =
          (uint32_t)((double)m_frequency / sample_rate * 4294967296.0);
    }
  }
};

/**
 * @brief Generates a saw tooth wave sound. Uses a 32 bit phase accumulator
 * with an integer increment, just like FastIntSineGenerator: no floating
 * point operations or divisions in the readSample() hot path. Unlike a
 * sine, a saw tooth needs no lookup table either - a wrapping unsigned
 * accumulator reinterpreted as signed already is a linear ramp - so this
 * is even cheaper than FastIntSineGenerator and a good fit for MCUs
 * without a hardware FPU (e.g. RP2040), where the previous sinf()/float
 * based implementation (inherited from SineGenerator) was comparatively
 * expensive.
 * @ingroup generator
 * @author Phil Schatzmann
 * @copyright GPLv3
 * @tparam T
 */
template <class T = int16_t>
class SawToothGenerator : public SoundGenerator<T> {
 public:
  SawToothGenerator(float amplitude = NumberConverter::maxValueT<T>(),
                     float phase = 0.0f) {
    LOGD("SawToothGenerator");
    setAmplitude(amplitude);
    setPhase(phase);
  }

  bool begin() override {
    TRACEI();
    SoundGenerator<T>::begin();
    updatePhaseIncrement();
    return true;
  }

  bool begin(AudioInfo info) override {
    LOGI("%s::begin(channels=%d, sample_rate=%d)", "SawToothGenerator",
         (int)info.channels, (int)info.sample_rate);
    SoundGenerator<T>::begin(info);
    updatePhaseIncrement();
    return true;
  }

  bool begin(AudioInfo info, float frequency) {
    SoundGenerator<T>::begin(info);
    if (frequency > 0.0f) {
      setFrequency(frequency);
    }
    return true;
  }

  bool begin(int channels, int sample_rate, float frequency) {
    SoundGenerator<T>::info.channels = channels;
    SoundGenerator<T>::info.sample_rate = sample_rate;
    return begin(SoundGenerator<T>::info, frequency);
  }

  virtual void setAudioInfo(AudioInfo info) override {
    SoundGenerator<T>::setAudioInfo(info);
    updatePhaseIncrement();
  }

  /// Defines the frequency - the only place where float math is used
  void setFrequency(float frequency) override {
    LOGI("setFrequency: %.2f", frequency);
    m_frequency = frequency;
    updatePhaseIncrement();
  }

  /// Defines the starting phase in radians
  void setPhase(float phase) {
    double turns = phase / (2.0 * PI);
    m_phase_offset = (uint32_t)(turns * 4294967296.0);
  }

  void setAmplitude(float amp) {
    m_amplitude = amp;
    m_amplitude_i = (int32_t)amp;
  }

  /// Provides a single sample - integer only, no float ops!
  virtual T readSample() override {
    uint32_t phase = m_phase_acc + m_phase_offset;
    m_phase_acc += m_phase_increment;
    // reinterpreting the wrapping unsigned phase as signed already gives
    // a linear ramp from -2^31 to 2^31-1: exactly a saw tooth
    int32_t ramp = (int32_t)phase;
    return (T)(((int64_t)ramp * m_amplitude_i) >> 31);
  }

 protected:
  volatile float m_frequency = 0.0f;
  float m_amplitude = 1.0f;
  int32_t m_amplitude_i = 32767;
  // fixed point (32 bit) phase: wrapping is implicit via unsigned overflow
  uint32_t m_phase_acc = 0;
  uint32_t m_phase_offset = 0;
  uint32_t m_phase_increment = 0;

  // only called from begin()/setFrequency()/setAudioInfo(), never per sample
  void updatePhaseIncrement() {
    uint32_t sample_rate = SoundGenerator<T>::info.sample_rate;
    if (sample_rate > 0) {
      m_phase_increment =
          (uint32_t)((double)m_frequency / sample_rate * 4294967296.0);
    }
  }
};

/**
 * @brief Generates a random noise sound with the help of rand() function.
 * @ingroup generator
 * @author Phil Schatzmann
 * @copyright GPLv3
 *
 */
template <class T = int16_t>
class WhiteNoiseGenerator : public SoundGenerator<T> {
 public:
  /// the scale defines the max value which is generated
  WhiteNoiseGenerator(T amplitude = 32767) { this->amplitude = amplitude; }

  /// Provides a single sample
  T readSample() { return (random(-amplitude, amplitude)); }

 protected:
  T amplitude;
  // //range : [min, max]
  int random(int min, int max) { return min + rand() % ((max + 1) - min); }
};

/**
 * @brief Generates pink noise.
 * @ingroup generator
 * @author Phil Schatzmann
 * @copyright GPLv3
 *
 */
template <class T = int16_t>
class PinkNoiseGenerator : public SoundGenerator<T> {
 public:
  /// the amplitude defines the max value which is generated
  PinkNoiseGenerator(T amplitude = 32767) {
    this->amplitude = amplitude;
    max_key = 0x1f;  // Five bits set
    key = 0;
    for (int i = 0; i < 5; i++) white_values[i] = rand() % (amplitude / 5);
  }

  /// Provides a single sample
  T readSample() {
    T last_key = key;
    unsigned int sum;

    key++;
    if (key > max_key) key = 0;
    // Exclusive-Or previous value with current value. This gives
    // a list of bits that have changed.
    int diff = last_key ^ key;
    sum = 0;
    for (int i = 0; i < 5; i++) {
      // If bit changed get new random number for corresponding
      // white_value
      if (diff & (1 << i)) white_values[i] = rand() % (amplitude / 5);
      sum += white_values[i];
    }
    return sum;
  }

 protected:
  T max_key;
  T key;
  unsigned int white_values[5];
  unsigned int amplitude;
};

/**
 * @brief Provides a fixed value (e.g. 0) as sound data. This can be used e.g.
 * to test the output functionality which should optimally just output silence
 * and no artifacts.
 * @ingroup generator
 * @author Phil Schatzmann
 * @copyright GPLv3
 *
 */
template <class T = int16_t>
class SilenceGenerator : public SoundGenerator<T> {
 public:
  // the scale defines the max value which is generated
  SilenceGenerator(T value = 0) { this->value = value; }

  /// Provides a single sample
  T readSample() {
    return value;  // return 0
  }

 protected:
  T value;
};

/**
 * @brief An Adapter Class which lets you use any Stream as a Generator
 * @ingroup generator
 * @author Phil Schatzmann
 * @copyright GPLv3
 * @tparam T
 */
template <class T = int16_t>
class GeneratorFromStream : public SoundGenerator<T>, public VolumeSupport {
 public:
  GeneratorFromStream() {
    maxValue = NumberConverter::maxValue(sizeof(T) * 8);
  };

  /**
   * @brief Constructs a new Generator from a Stream object that can be used
   * e.g. as input for AudioEffectss.
   *
   * @param input Stream
   * @param channels number of channels of the Stream
   * @param volume factor my which the sample value is multiplied -
   * default 1.0; Use it e.g. to reduce the volume (e.g. with 0.5)
   */
  GeneratorFromStream(Stream& input, int channels = 1, float volume = 1.0) {
    maxValue = NumberConverter::maxValue(sizeof(T) * 8);
    setStream(input);
    setVolume(volume);
    setChannels(channels);
  }

  /// (Re-)Assigns a stream to the Adapter class
  void setStream(Stream& input) { this->p_stream = &input; }

  void setChannels(int channels) { this->channels = channels; }

  /// Provides a single sample from the stream
  T readSample() {
    T data = 0;
    float total = 0;
    if (p_stream != nullptr) {
      for (int j = 0; j < channels; j++) {
        p_stream->readBytes((uint8_t*)&data, sizeof(T));
        total += data;
      }
      float avg = (total / channels) * volume();
      if (avg > maxValue) {
        data = maxValue;
      } else if (avg < -maxValue) {
        data = -maxValue;
      } else {
        data = avg;
      }
    }
    return data;
  }

 protected:
  Stream* p_stream = nullptr;
  int channels = 1;
  float maxValue;
};

/**
 * @brief We generate the samples from an array which is provided in the
 * constructor
 * @ingroup generator
 * @author Phil Schatzmann
 * @copyright GPLv3
 * @tparam T
 */

template <class T = int16_t>
class GeneratorFromArray : public SoundGenerator<T> {
 public:
  GeneratorFromArray() = default;
  /**
   * @brief Construct a new Generator from an array
   *
   * @tparam array array of audio data of the the type defined as class
   * template parameter
   * @param repeat number of repetions the array should be played,
   * set to 0 for endless repeat. (default 0)
   * @param setInactiveAtEnd  defines if the generator is set inactive when
   * the array has played fully. Default is false.
   * @param startIndex  defines if the phase. Default is 0.
   */

  template <size_t arrayLen>
  GeneratorFromArray(T (&array)[arrayLen], int repeat = 0,
                     bool setInactiveAtEnd = false, size_t startIndex = 0) {
    TRACED();
    this->max_repeat = repeat;
    this->inactive_at_end = setInactiveAtEnd;
    this->sound_index = startIndex;
    setArray(array, arrayLen);
  }

  template <int arrayLen>
  void setArray(T (&array)[arrayLen]) {
    TRACED();
    setArray(array, arrayLen);
  }

  void setArray(T* array, size_t size) {
    table.resize(size);
    for (int j = 0; j < size; j++) {
      table[j] = array[j];
    }
    LOGI("table_length: %d", (int)size);
  }

  /// Starts the generation of samples with the provided AudioInfo
  bool begin(AudioInfo info) override {
    return SoundGenerator<T>::begin(info);
  }

  /// Starts the generation of samples with the provided AudioInfo and frequency
  bool begin(AudioInfo info, float frequency) {
    bool rc = begin(info);
    setFrequency(frequency);
    return rc;
  }

  /// Starts the generation of samples
  bool begin() override {
    TRACEI();
    SoundGenerator<T>::begin();
    sound_index = 0.0f;
    repeat_counter = 0;
    is_running = true;
    return true;
  }

  void end() override { table.resize(0); }

  /// Provides a single sample
  T readSample() override {
    if (table.size() == 0) {
      return 0;
    }

    if (!this->is_running) {
      return 0;
    }

    const float table_size = static_cast<float>(table.size());

    // at end deactivate output
    while (sound_index >= table_size) {
      // LOGD("reset index - sound_index: %d, table_length:
      // %d",sound_index,table_length);
      sound_index -= table_size;
      // deactivate when count has been used up
      if (max_repeat >= 1 && ++repeat_counter >= max_repeat) {
        LOGD("atEnd");
        this->is_running = false;
        if (inactive_at_end) {
          this->active = false;
        }
        return 0;
      }
    }

    // LOGD("index: %d - active: %d", sound_index, this->active);
    T result = 0;
    if (this->is_running) {
      int idx0 = static_cast<int>(sound_index);
      int idx1 = idx0 + 1;
      if (idx1 >= static_cast<int>(table.size())) {
        idx1 = 0;
      }
      float frac = sound_index - static_cast<float>(idx0);
      float sample = static_cast<float>(table[idx0]) * (1.0f - frac) +
                     static_cast<float>(table[idx1]) * frac;
      result = static_cast<T>(sample);
      sound_index += index_increment;
    }

    return result;
  }

  // step size the sound index is incremented (default = 1)
  void setIncrement(int inc) {
    index_increment = inc;
    frequency = 0.0f;
  }

  /// Defines the output frequency based on sample rate and table size
  void setFrequency(float frequency) override {
    if (SoundGenerator<T>::audioInfo().sample_rate <= 0 || table.size() == 0) {
      LOGE("setFrequency failed: sample_rate=%d table_size=%d",
           (int)SoundGenerator<T>::audioInfo().sample_rate, (int)table.size());
      return;
    }
    if (frequency < 0.0f) {
      frequency = 0.0f;
    }
    this->frequency = frequency;
    index_increment =
        frequency * static_cast<float>(table.size()) /
        static_cast<float>(SoundGenerator<T>::audioInfo().sample_rate);
  }

  // Sets up a sine table - returns the effective frequency
  int setupSine(int sampleRate, float reqFrequency, float amplitude = 1.0) {
    int sample_count =
        static_cast<float>(sampleRate) /
        reqFrequency;  // e.g.  44100 / 300hz = 147 samples per wave
    float angle = 2.0 * PI / sample_count;
    table.resize(sample_count);
    for (int j = 0; j < sample_count; j++) {
      table[j] = sinf(j * angle) * amplitude;
    }
    // calculate effective frequency
    return sampleRate / sample_count;
  }

  // Similar like is active to check if the array is still playing.
  bool isRunning() { return is_running; }

 protected:
  float sound_index = 0.0f;
  int max_repeat = 0;
  int repeat_counter = 0;
  bool inactive_at_end = false;
  bool is_running = false;
  bool owns_data = false;
  Vector<T> table;
  float index_increment = 1.0f;
  float frequency = 0.0f;
};

/**
 * @brief Just returns a constant value
 * @ingroup generator
 * @author Phil Schatzmann
 * @copyright GPLv3
 * @tparam T
 */
template <class T = int16_t>
class GeneratorFixedValue : public SoundGenerator<T> {
 public:
  GeneratorFixedValue() = default;

  virtual bool begin(AudioInfo info) { return SoundGenerator<T>::begin(info); }

  void setValue(T value) { value_set = value; }

  /// Starts the generation of samples
  bool begin() override {
    TRACEI();
    SoundGenerator<T>::begin();
    is_running = true;
    value_return = value_set;
    return true;
  }

  /// Provides a single sample
  T readSample() override { return value_return; }

  // Similar like is active to check if the array is still playing.
  bool isRunning() { return is_running; }

 protected:
  T value_set = 0;
  T value_return = 0;
  bool is_running = false;
};

/**
 * @brief A sine generator based on a table. The table is created using
 * degrees where one full wave is 360 degrees.
 * @ingroup generator
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
template <class T = int16_t>
class SineFromTable : public SoundGenerator<T> {
 public:
  SineFromTable(float amplitude = NumberConverter::maxValueT<T>()) {
    this->amplitude = amplitude;
    this->amplitude_to_be = amplitude;
  }

  /// Defines the new amplitude (volume)
  void setAmplitude(float amplitude) { this->amplitude_to_be = amplitude; }

  /// To avoid pops we do not allow to big amplitude changes at once and
  /// spread them over time
  void setMaxAmplitudeStep(float step) { max_amplitude_step = step; }

  T readSample() {
    // update angle
    angle += step;
    if (angle >= 360.0f) {
      while (angle >= 360.0f) {
        angle -= 360.0f;
      }
      // update frequency at start of circle (near 0 degrees)
      step = step_new;

      updateAmplitudeInSteps();
      // amplitude = amplitude_to_be;
    }
    return interpolate(angle);
  }

  bool begin() {
    is_first = true;
    SoundGenerator<T>::begin();
    base_frequency = SoundGenerator<T>::audioInfo().sample_rate /
                     360.0f;  // 122.5 hz (at 44100); 61 hz (at 22050)
    return true;
  }

  bool begin(AudioInfo info, float frequency) {
    SoundGenerator<T>::begin(info);
    base_frequency = SoundGenerator<T>::audioInfo().sample_rate /
                     360.0f;  // 122.5 hz (at 44100); 61 hz (at 22050)
    setFrequency(frequency);
    return true;
  }

  bool begin(int channels, int sample_rate, uint16_t frequency = 0) {
    SoundGenerator<T>::info.channels = channels;
    SoundGenerator<T>::info.sample_rate = sample_rate;
    return begin(SoundGenerator<T>::info, frequency);
  }

  void setFrequency(float freq) {
    step_new = freq / base_frequency;
    if (is_first) {
      step = step_new;
      is_first = false;
    }
    LOGD("step: %f", step_new);
  }

 protected:
  bool is_first = true;
  float amplitude;
  float amplitude_to_be;
  float max_amplitude_step = 50.0f;
  float base_frequency = 1.0f;
  float step = 1.0f;
  float step_new = 1.0f;
  float angle = 0.0f;
  // 122.5 hz (at 44100); 61 hz (at 22050)
  const float values[181] = {
      0,        0.0174524, 0.0348995, 0.052336, 0.0697565, 0.0871557,
      0.104528, 0.121869,  0.139173,  0.156434, 0.173648,  0.190809,
      0.207912, 0.224951,  0.241922,  0.258819, 0.275637,  0.292372,
      0.309017, 0.325568,  0.34202,   0.358368, 0.374607,  0.390731,
      0.406737, 0.422618,  0.438371,  0.45399,  0.469472,  0.48481,
      0.5,      0.515038,  0.529919,  0.544639, 0.559193,  0.573576,
      0.587785, 0.601815,  0.615661,  0.62932,  0.642788,  0.656059,
      0.669131, 0.681998,  0.694658,  0.707107, 0.71934,   0.731354,
      0.743145, 0.75471,   0.766044,  0.777146, 0.788011,  0.798636,
      0.809017, 0.819152,  0.829038,  0.838671, 0.848048,  0.857167,
      0.866025, 0.87462,   0.882948,  0.891007, 0.898794,  0.906308,
      0.913545, 0.920505,  0.927184,  0.93358,  0.939693,  0.945519,
      0.951057, 0.956305,  0.961262,  0.965926, 0.970296,  0.97437,
      0.978148, 0.981627,  0.984808,  0.987688, 0.990268,  0.992546,
      0.994522, 0.996195,  0.997564,  0.99863,  0.999391,  0.999848,
      1,        0.999848,  0.999391,  0.99863,  0.997564,  0.996195,
      0.994522, 0.992546,  0.990268,  0.987688, 0.984808,  0.981627,
      0.978148, 0.97437,   0.970296,  0.965926, 0.961262,  0.956305,
      0.951057, 0.945519,  0.939693,  0.93358,  0.927184,  0.920505,
      0.913545, 0.906308,  0.898794,  0.891007, 0.882948,  0.87462,
      0.866025, 0.857167,  0.848048,  0.838671, 0.829038,  0.819152,
      0.809017, 0.798636,  0.788011,  0.777146, 0.766044,  0.75471,
      0.743145, 0.731354,  0.71934,   0.707107, 0.694658,  0.681998,
      0.669131, 0.656059,  0.642788,  0.62932,  0.615661,  0.601815,
      0.587785, 0.573576,  0.559193,  0.544639, 0.529919,  0.515038,
      0.5,      0.48481,   0.469472,  0.45399,  0.438371,  0.422618,
      0.406737, 0.390731,  0.374607,  0.358368, 0.34202,   0.325568,
      0.309017, 0.292372,  0.275637,  0.258819, 0.241922,  0.224951,
      0.207912, 0.190809,  0.173648,  0.156434, 0.139173,  0.121869,
      0.104528, 0.0871557, 0.0697565, 0.052336, 0.0348995, 0.0174524,
      0};

  T interpolate(float angle) {
    bool positive = (angle <= 180.0f);
    float angle_positive = positive ? angle : angle - 180.0f;
    int angle_int1 = angle_positive;
    int angle_int2 = angle_int1 + 1;
    T v1 = values[angle_int1] * amplitude;
    T v2 = values[angle_int2] * amplitude;
    T result = v1 < v2 ? map(angle_positive, angle_int1, angle_int2, v1, v2)
                       : map(angle_positive, angle_int1, angle_int2, v2, v1);
    // float result = v1;
    return positive ? result : -result;
  }

  T map(T x, T in_min, T in_max, T out_min, T out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
  }

  void updateAmplitudeInSteps() {
    float diff = amplitude_to_be - amplitude;
    if (abs(diff) > max_amplitude_step) {
      diff = (diff < 0) ? -max_amplitude_step : max_amplitude_step;
    }
    if (abs(diff) >= 1.0f) {
      amplitude += diff;
    }
  }
};

/**
 * @brief Generator which combines (mixes) multiple sound generators into one
 * output
 * @ingroup generator
 * @author Phil Schatzmann
 * @copyright GPLv3
 * @tparam T
 */
template <class T = int16_t>
class GeneratorMixer : public SoundGenerator<T> {
 public:
  GeneratorMixer() = default;

  void add(SoundGenerator<T>& generator) { vector.push_back(&generator); }
  void add(SoundGenerator<T>* generator) { vector.push_back(generator); }

  void clear() { vector.clear(); }

  T readSample() {
    float total = 0.0f;
    float count = 0.0f;
    for (auto& generator : vector) {
      if (generator->isActive()) {
        T sample = generator->readSample();
        total += sample;
        count += 1.0f;
      }
    }
    return count > 0.0f ? total / count : 0;
  }

 protected:
  Vector<SoundGenerator<T>*> vector;
  int actualChannel = 0;
};

/**
 * @brief Generates a test signal which is easy to check because the values
 * are incremented or decremented by 1
 * @ingroup generator
 * @author Phil Schatzmann
 * @copyright GPLv3
 * @tparam T
 */
template <class T = int16_t>
class TestGenerator : public SoundGenerator<T> {
 public:
  TestGenerator(T max = 1000, T inc = 1) { this->max = max; }

  T readSample() override {
    value += inc;
    if (abs(value) >= max) {
      inc = -inc;
      value += (inc * 2);
    }
    return value;
  }

 protected:
  T max;
  T value = 0;
  T inc = 1;
};

}  // namespace audio_tools
