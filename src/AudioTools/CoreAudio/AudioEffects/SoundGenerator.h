#pragma once

#include <math.h>

#include "AudioTools/CoreAudio/AudioBasic/Collections.h"
#include "AudioTools/CoreAudio/AudioLogger.h"
#include "AudioTools/CoreAudio/AudioTypes.h"

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
template <class T>
class SoundGeneratorT {
 public:
  SoundGeneratorT() { info.bits_per_sample = sizeof(T) * 8; }

  virtual ~SoundGeneratorT() { end(); }

  virtual bool begin(AudioInfo newInfo) {
    setAudioInfo(newInfo);
    return begin();
  }

  virtual bool begin() {
    TRACED();
    active = true;
    info.logInfo("SoundGeneratorT:");
    return true;
  }

  /// ends the processing
  virtual void end() { active = false; }

  /// Checks if the begin method has been called - after end() isActive returns
  /// false
  virtual bool isActive() { return active; }

  /// Provides a single sample
  virtual T readSample() = 0;

  /// Provides the default configuration
  virtual AudioInfo defaultConfig() {
    AudioInfo result;
    result.bits_per_sample = sizeof(T) * 8;
    return result;
  }

  /// Abstract method: not implemented! Just provides an error message...
  virtual void setFrequency(float frequency) {
    LOGE("setFrequency not supported");
  }

  virtual void setAmplitude(float amp) { 
    LOGE("setFrequency not supported");
  }

  /// Provides the AudioInfo
  virtual AudioInfo audioInfo() { return info; }

  /// Defines/updates the AudioInfo
  virtual void setAudioInfo(AudioInfo info) {
    this->info = info;
    if (info.bits_per_sample != sizeof(T) * 8) {
      LOGE("invalid bits_per_sample: %d", info.bits_per_sample);
    }
  }

 protected:
  bool active = false;
  AudioInfo info;
};

/**
 * @brief Generates a Sound with the help of sin() function. If you plan to
 * change the amplitude or frequency (incrementally), I suggest to use
 * SineFromTableT instead.
 * @ingroup generator
 * @author Phil Schatzmann
 * @copyright GPLv3
 *
 */
template <class T>
class SineWaveGeneratorT : public SoundGeneratorT<T> {
 public:
  // the scale defines the max value which is generated
  SineWaveGeneratorT(float amplitude = 0.9f * NumberConverter::maxValueT<T>(),
                     float phase = 0.0f) {
    LOGD("SineWaveGeneratorT");
    m_amplitude = amplitude;
    m_phase = phase;
  }

  bool begin() override {
    TRACEI();
    SoundGeneratorT<T>::begin();
    this->m_deltaTime = 1.0f / SoundGeneratorT<T>::info.sample_rate;
    return true;
  }

  bool begin(AudioInfo info) override {
    LOGI("%s::begin(channels=%d, sample_rate=%d)", "SineWaveGeneratorT",
         (int)info.channels, (int)info.sample_rate);
    SoundGeneratorT<T>::begin(info);
    this->m_deltaTime = 1.0f / SoundGeneratorT<T>::info.sample_rate;
    return true;
  }

  bool begin(AudioInfo info, float frequency) {
    LOGI("%s::begin(channels=%d, sample_rate=%d, frequency=%.2f)",
         "SineWaveGeneratorT", (int)info.channels, (int)info.sample_rate,
         frequency);
    SoundGeneratorT<T>::begin(info);
    this->m_deltaTime = 1.0f / SoundGeneratorT<T>::info.sample_rate;
    if (frequency > 0.0f) {
      setFrequency(frequency);
    }
    return true;
  }

  // bool begin(int channels, int sample_rate, float frequency) {
  //   SoundGeneratorT<T>::info.channels = channels;
  //   SoundGeneratorT<T>::info.sample_rate = sample_rate;
  //   return begin(SoundGeneratorT<T>::info, frequency);
  // }

  // update m_deltaTime
  virtual void setAudioInfo(AudioInfo info) override {
    SoundGeneratorT<T>::setAudioInfo(info);
    this->m_deltaTime = 1.0f / SoundGeneratorT<T>::info.sample_rate;
  }

  virtual AudioInfo defaultConfig() override {
    return SoundGeneratorT<T>::defaultConfig();
  }

  /// Defines the frequency - after the processing has been started
  void setFrequency(float frequency) override {
    LOGI("setFrequency: %.2f", frequency);
    LOGI("active: %s", SoundGeneratorT<T>::active ? "true" : "false");
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
  float m_frequency = 0.0f;
  float m_cycles = 0.0f;  // Varies between 0.0 and 1.0
  float m_amplitude = 1.0f;
  float m_deltaTime = 0.0f;
  float m_phase = 0.0f;
  const float double_Pi = 2.0f * PI;

  void logStatus() {
    SoundGeneratorT<T>::info.logStatus();
    LOGI("amplitude: %f", this->m_amplitude);
    LOGI("active: %s", SoundGeneratorT<T>::active ? "true" : "false");
  }
};

/**
 * @brief Sine wave which is based on a fast approximation function.
 * @ingroup generator
 * @author Vivian Leigh Stewart
 * @copyright GPLv3
 * @tparam T
 */
template <class T>
class FastSineGeneratorT : public SineWaveGeneratorT<T> {
 public:
  FastSineGeneratorT(float amplitude = 0.9f * NumberConverter::maxValueT<T>(),
                     float phase = 0.0)
      : SineWaveGeneratorT<T>(amplitude, phase) {
    LOGD("FastSineGeneratorT");
  }

  virtual T readSample() override {
    float angle =
        SineWaveGeneratorT<T>::m_cycles + SineWaveGeneratorT<T>::m_phase;
    T result = SineWaveGeneratorT<T>::m_amplitude * sine(angle);
    SineWaveGeneratorT<T>::m_cycles +=
        SineWaveGeneratorT<T>::m_frequency * SineWaveGeneratorT<T>::m_deltaTime;
    if (SineWaveGeneratorT<T>::m_cycles > 1.0f) {
      SineWaveGeneratorT<T>::m_cycles -= 1.0f;
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
 * @brief Generates a square wave sound.
 * @ingroup generator
 * @author Phil Schatzmann
 * @copyright GPLv3
 *
 */
template <class T>
class SquareWaveGeneratorT : public FastSineGeneratorT<T> {
 public:
  SquareWaveGeneratorT(float amplitude = 0.9f * NumberConverter::maxValueT<T>(),
                       float phase = 0.0f)
      : FastSineGeneratorT<T>(amplitude, phase) {
    LOGD("SquareWaveGeneratorT");
  }

  virtual T readSample() override {
    return value(FastSineGeneratorT<T>::readSample(),
                 FastSineGeneratorT<T>::m_amplitude);
  }

 protected:
  // returns amplitude for positive vales and -amplitude for negative values
  T value(T value, T amplitude) {
    return (value >= 0) ? amplitude : -amplitude;
  }
};

/**
 * @brief SawToothGeneratorT
 * @ingroup generator
 * @author Phil Schatzmann
 * @copyright GPLv3
 * @tparam T
 */
template <class T>
class SawToothGeneratorT : public SineWaveGeneratorT<T> {
 public:
  SawToothGeneratorT(float amplitude = 0.9f * NumberConverter::maxValueT<T>(),
                     float phase = 0.0)
      : SineWaveGeneratorT<T>(amplitude, phase) {
    LOGD("SawToothGeneratorT");
  }

  T readSample() override {
    float angle = this->m_cycles + this->m_phase;
    T result = this->m_amplitude * saw(angle);
    this->m_cycles += this->m_frequency * this->m_deltaTime;
    if (this->m_cycles > 1.0) {
      this->m_cycles -= 1.0;
    }
    return result;
  }

 protected:

  /// sine approximation.
  float saw(float t) {
    float p = (t - (int)t) - 0.5f;  // 0 <= p <= 1
    return p;
  }
};

/**
 * @brief Generates a random noise sound with the help of rand() function.
 * @ingroup generator
 * @author Phil Schatzmann
 * @copyright GPLv3
 *
 */
template <class T>
class WhiteNoiseGeneratorT : public SoundGeneratorT<T> {
 public:
  /// the scale defines the max value which is generated
  WhiteNoiseGeneratorT(T amplitude = 0.9f * NumberConverter::maxValueT<T>()) {
    this->amplitude = amplitude;
  }

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
template <class T>
class PinkNoiseGeneratorT : public SoundGeneratorT<T> {
 public:
  /// the amplitude defines the max value which is generated
  PinkNoiseGeneratorT(T amplitude = 0.9f * NumberConverter::maxValueT<T>()) {
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
template <class T>
class SilenceGeneratorT : public SoundGeneratorT<T> {
 public:
  // the scale defines the max value which is generated
  SilenceGeneratorT(T value = 0) { this->value = value; }

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
template <class T>
class GeneratorFromStreamT : public SoundGeneratorT<T>, public VolumeSupport {
 public:
  GeneratorFromStreamT() {
    maxValue = NumberConverter::maxValue(sizeof(T) * 8);
  };

  /**
   * @brief Constructs a new Generator from a Stream object that can be used
   * e.g. as input for AudioEffectss.
   *
   * @param input Stream
   * @param channels number of channels of the Stream
   * @param volume factor my which the sample value is multiplied - default 1.0;
   * Use it e.g. to reduce the volume (e.g. with 0.5)
   */
  GeneratorFromStreamT(Stream &input, int channels = 1, float volume = 1.0) {
    maxValue = NumberConverter::maxValue(sizeof(T) * 8);
    setStream(input);
    setVolume(volume);
    setChannels(channels);
  }

  /// (Re-)Assigns a stream to the Adapter class
  void setStream(Stream &input) { this->p_stream = &input; }

  void setChannels(int channels) { this->channels = channels; }

  /// Provides a single sample from the stream
  T readSample() {
    T data = 0;
    float total = 0;
    if (p_stream != nullptr) {
      for (int j = 0; j < channels; j++) {
        p_stream->readBytes((uint8_t *)&data, sizeof(T));
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
  Stream *p_stream = nullptr;
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

template <class T>
class GeneratorFromArrayT : public SoundGeneratorT<T> {
 public:
  GeneratorFromArrayT() = default;
  /**
   * @brief Construct a new Generator from an array
   *
   * @tparam array array of audio data of the the type defined as class template
   * parameter
   * @param repeat number of repetions the array should be played,
   * set to 0 for endless repeat. (default 0)
   * @param setInactiveAtEnd  defines if the generator is set inactive when the
   * array has played fully. Default is false.
   * @param startIndex  defines if the phase. Default is 0.
   */

  template <size_t arrayLen>
  GeneratorFromArrayT(T (&array)[arrayLen], int repeat = 0,
                      bool setInactiveAtEnd = false, size_t startIndex = 0) {
    TRACED();
    this->max_repeat = repeat;
    this->inactive_at_end = setInactiveAtEnd;
    this->sound_index = startIndex;
    setArray(array, arrayLen);
  }

  ~GeneratorFromArrayT() {
    if (owns_data) {
      delete[] table;
    }
  }

  template <int arrayLen>
  void setArray(T (&array)[arrayLen]) {
    TRACED();
    setArray(array, arrayLen);
  }

  void setArray(T *array, size_t size) {
    this->table_length = size;
    this->table = array;
    LOGI("table_length: %d", (int)size);
  }

  virtual bool begin(AudioInfo info) override {
    return SoundGeneratorT<T>::begin(info);
  }

  /// Starts the generation of samples
  bool begin() override {
    TRACEI();
    SoundGeneratorT<T>::begin();
    sound_index = 0;
    repeat_counter = 0;
    is_running = true;
    return true;
  }

  /// Provides a single sample
  T readSample() override {
    // at end deactivate output
    if (sound_index >= table_length) {
      // LOGD("reset index - sound_index: %d, table_length:
      // %d",sound_index,table_length);
      sound_index = 0;
      // deactivate when count has been used up
      if (max_repeat >= 1 && ++repeat_counter >= max_repeat) {
        LOGD("atEnd");
        this->is_running = false;
        if (inactive_at_end) {
          this->active = false;
        }
      }
    }

    // LOGD("index: %d - active: %d", sound_index, this->active);
    T result = 0;
    if (this->is_running) {
      result = table[sound_index];
      sound_index += index_increment;
    }

    return result;
  }

  // step size the sound index is incremented (default = 1)
  void setIncrement(int inc) { index_increment = inc; }

  // Sets up a sine table - returns the effective frequency
  int setupSine(int sampleRate, float reqFrequency, float amplitude = 1.0) {
    int sample_count =
        static_cast<float>(sampleRate) /
        reqFrequency;  // e.g.  44100 / 300hz = 147 samples per wave
    float angle = 2.0 * PI / sample_count;
    table = new T[sample_count];
    for (int j = 0; j < sample_count; j++) {
      table[j] = sinf(j * angle) * amplitude;
    }
    owns_data = true;
    table_length = sample_count;
    // calculate effective frequency
    return sampleRate / sample_count;
  }

  // Similar like is active to check if the array is still playing.
  bool isRunning() { return is_running; }

 protected:
  int sound_index = 0;
  int max_repeat = 0;
  int repeat_counter = 0;
  bool inactive_at_end;
  bool is_running = false;
  bool owns_data = false;
  T *table = nullptr;
  size_t table_length = 0;
  int index_increment = 1;
};

/**
 * @brief Just returns a constant value
 * @ingroup generator
 * @author Phil Schatzmann
 * @copyright GPLv3
 * @tparam T
 */
template <class T>
class GeneratorFixedValueT : public SoundGeneratorT<T> {
 public:
  GeneratorFixedValueT() = default;

  virtual bool begin(AudioInfo info) { return SoundGeneratorT<T>::begin(info); }

  void setValue(T value) { value_set = value; }

  /// Starts the generation of samples
  bool begin() override {
    TRACEI();
    SoundGeneratorT<T>::begin();
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
template <class T>
class SineFromTableT : public SoundGeneratorT<T> {
 public:
  SineFromTableT(float amplitude = 0.9f * NumberConverter::maxValueT<T>()) {
    this->amplitude = amplitude;
    this->amplitude_to_be = amplitude;
  }

  /// Defines the new amplitude (volume)
  void setAmplitude(float amplitude) { this->amplitude_to_be = amplitude; }

  /// To avoid pops we do not allow to big amplitude changes at once and spread
  /// them over time
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
    SoundGeneratorT<T>::begin();
    base_frequency = SoundGeneratorT<T>::audioInfo().sample_rate /
                     360.0f;  // 122.5 hz (at 44100); 61 hz (at 22050)
    return true;
  }

  bool begin(AudioInfo info, float frequency) {
    SoundGeneratorT<T>::begin(info);
    base_frequency = SoundGeneratorT<T>::audioInfo().sample_rate /
                     360.0f;  // 122.5 hz (at 44100); 61 hz (at 22050)
    setFrequency(frequency);
    return true;
  }

  bool begin(int channels, int sample_rate, uint16_t frequency = 0) {
    SoundGeneratorT<T>::info.channels = channels;
    SoundGeneratorT<T>::info.sample_rate = sample_rate;
    return begin(SoundGeneratorT<T>::info, frequency);
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
template <class T>
class GeneratorMixerT : public SoundGeneratorT<T> {
 public:
  GeneratorMixerT() = default;

  void add(SoundGeneratorT<T> &generator) { vector.push_back(&generator); }
  void add(SoundGeneratorT<T> *generator) { vector.push_back(generator); }

  void clear() { vector.clear(); }

  T readSample() {
    float total = 0.0f;
    float count = 0.0f;
    for (auto &generator : vector) {
      if (generator->isActive()) {
        T sample = generator->readSample();
        total += sample;
        count += 1.0f;
      }
    }
    return count > 0.0f ? total / count : 0;
  }

 protected:
  Vector<SoundGeneratorT<T> *> vector;
  int actualChannel = 0;
};

/**
 * @brief Generates a test signal which is easy to check because the values are
 * incremented or decremented by 1
 * @ingroup generator
 * @author Phil Schatzmann
 * @copyright GPLv3
 * @tparam T
 */
template <class T>
class TestGeneratorT : public SoundGeneratorT<T> {
 public:
  TestGeneratorT(T max = 1000, T inc = 1) { this->max = max; }

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

/// SoundGenerator for int16_t: see SoundGeneratorT
using SoundGenerator = SoundGeneratorT<int16_t>;
/// SineWaveGenerator for int16_t: see SineWaveGeneratorT 
using SineWaveGenerator = SineWaveGeneratorT<int16_t>;
/// FastSineGenerator for int16_t 
using FastSineGenerator = FastSineGeneratorT<int16_t>;
/// SquareWaveGenerator for int16_t 
using SquareWaveGenerator = SquareWaveGeneratorT<int16_t>;
/// SawToothGenerator for int16_t 
using SawToothGenerator = SawToothGeneratorT<int16_t>;
/// WhiteNoiseGenerator for int16_t 
using WhiteNoiseGenerator = WhiteNoiseGeneratorT<int16_t>;
/// PinkNoiseGenerator for int16_t 
using PinkNoiseGenerator = PinkNoiseGeneratorT<int16_t>;
/// SilenceGenerator for int16_t 
using SilenceGenerator = SilenceGeneratorT<int16_t>;
/// GeneratorFromStream for int16_t 
using GeneratorFromStream = GeneratorFromStreamT<int16_t>;
/// GeneratorFromArray for int16_t 
using GeneratorFromArray = GeneratorFromArrayT<int16_t>;
/// GeneratorFixedValue for int16_t 
using GeneratorFixedValue = GeneratorFixedValueT<int16_t>;
/// SineFromTable for int16_t 
using SineFromTable = SineFromTableT<int16_t>;
/// GeneratorMixer for int16_t 
using GeneratorMixer = GeneratorMixerT<int16_t>;
/// TestGenerator for int16_t 
using TestGenerator = TestGeneratorT<int16_t>;

}  // namespace audio_tools
