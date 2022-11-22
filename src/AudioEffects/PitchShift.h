
#include "Print.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

namespace audio_tools {

/**
 * @brief Configuration for PitchShift: set the pitch_shift to define the
 * shift
 *
 */
struct PitchShiftInfo : public AudioBaseInfo {
  PitchShiftInfo() {
    channels = 2;
    sample_rate = 44100;
    bits_per_sample = 16;
  }
  float pitch_shift = 1.4f;
  int buffer_size = 1000;
};

/**
 * @brief Base Calss for Pitch Shifters whch defines the interface and provides
 * the common functionality
 *
 * @tparam T
 */
template <typename T> class PitchShiftInterface : public AudioPrint {
public:
  PitchShiftInfo defaultConfig() {
    PitchShiftInfo result;
    return result;
  }

  virtual bool begin(PitchShiftInfo info);

  virtual void end();

  /// Provides the input data to be pitch shifted
  /// Provides the input data to be pitch shifted
  size_t write(const uint8_t *data, size_t len) override {
    TRACED();
    if (!active)
      return 0;

    size_t result = 0;
    int channels = cfg.channels;
    T *p_in = (T *)data;
    int sample_count = len / sizeof(T);
    for (int j = 0; j < sample_count; j += channels) {
      float value = 0;
      for (int ch = 0; ch < channels; ch++) {
        value += p_in[j + ch];
      }
      // calculate avg sample value
      value /= cfg.channels;

      // output values
      T out_value = pitchShift(value);
      LOGD("pitchShift %d -> %d", value, out_value);
      T out_array[channels];
      for (int ch = 0; ch < channels; ch++) {
        out_array[ch] = out_value;
      }
      result += p_out->write((uint8_t *)out_array, sizeof(T) * channels);
    }
    return result;
  }

protected:
  virtual T pitchShift(T in);
  Print *p_out = nullptr;
  PitchShiftInfo cfg;
  bool active = false;
};

/**
 * @brief Very Simple Buffer implementation for Pitch Shift. We write in
 * constant speed, but reading can be done in a variable speed. We will hear
 * some noise when the buffer read and write pointers overrun each other
 * @tparam T
 */
template <typename T>
class VariableSpeedRingBufferSimple : public BaseBuffer<T> {
public:
  VariableSpeedRingBufferSimple(int size = 0, float increment = 1.0) {
    setIncrement(increment);
    if (size > 0)
      resize(size);
  }

  void setIncrement(int increment) { read_increment = increment; }

  void resize(int size) {
    buffer_size = size;
    buffer.resize(size);
  }

  T read() {
    T result = peek();
    read_pos_float += read_increment;
    // on buffer owerflow reset to beginning
    if (read_pos_float > buffer_size) {
      read_pos_float -= buffer_size;
    }
    return result;
  }

  T peek() {
    if (buffer.size() == 0) {
      LOGE("buffer has no memory");
      return 0;
    }
    return buffer[(int)read_pos_float];
  }

  bool write(T sample) {
    if (buffer.size() == 0) {
      LOGE("buffer has no memory");
      return false;
    }
    buffer[write_pos++] = sample;
    // on buffer owerflow reset to 0
    if (write_pos >= buffer_size) {
      write_pos = 0;
    }
    return true;
  }

  /// Reset pointer positions and clear buffer
  void reset() {
    read_pos_float = 0;
    write_pos = 0;
    memset(buffer.data(), 0, sizeof(T) * buffer_size);
  }

  virtual bool isFull() { return false; }
  virtual int available() { return buffer_size; }
  virtual int availableForWrite() { return buffer_size; }
  virtual T *address() { return nullptr; }

protected:
  Vector<T> buffer{0};
  int buffer_size = 0;
  float read_pos_float = 0.0;
  float read_increment = 1.0;
  int write_pos = 0;
};

/**
 * @brief Varialbe speed ring buffer where we read with 0 and 180 degree and
 * blend the result to prevent overrun artifacts. See
 * https://github.com/YetAnotherElectronicsChannel/STM32_DSP_PitchShift
 * @tparam T
 */
template <typename T> class VariableSpeedRingBuffer180 : public BaseBuffer<T> {
public:
  VariableSpeedRingBuffer180(int size = 0, float increment = 1.0) {
    setIncrement(increment);
    if (size > 0)
      resize(size);
  }

  void setIncrement(int increment) { pitch_shift = increment; }

  void resize(int size) {
    buffer_size = size;
    buffer.resize(size);
  }

  T read() { return pitchRead(); }

  T peek() { return -1; }

  bool write(T sample) {
    if (buffer.size() == 0) {
      LOGE("buffer has no memory");
      return false;
    }
    // write_pointer value is used in pitchRead()
    write_pointer = write_pos;
    buffer[write_pos++] = sample;
    // on buffer owerflow reset to 0
    if (write_pos >= buffer_size) {
      write_pos = 0;
    }
    return true;
  }

  /// Reset pointer positions and clear buffer
  void reset() {
    read_pos_float = 0;
    write_pos = 0;
    memset(buffer.data(), 0, sizeof(T) * buffer_size);
  }

  virtual bool isFull() { return false; }
  virtual int available() { return buffer_size; }
  virtual int availableForWrite() { return buffer_size; }
  virtual T *address() { return nullptr; }

protected:
  Vector<T> buffer{0};
  float read_pos_float = 0.0;
  float cross_fade = 1.0;
  int write_pos = 0;
  int write_pointer;
  int buffer_size = 0;
  int overlap = 0;
  float pitch_shift;

  /// pitch shift for a single sample
  virtual T pitchRead() {
    TRACED();

    // read fractional readpointer and generate 0° and 180° read-pointer in
    // integer
    int read_pointer_int = roundf(read_pos_float);
    int read_pointer_int180 = 0;
    if (read_pointer_int >= buffer_size / 2)
      read_pointer_int180 = read_pointer_int - (buffer_size / 2);
    else
      read_pointer_int180 = read_pointer_int + (buffer_size / 2);

    // read the two samples...
    float read_sample = (float)buffer[read_pointer_int];
    float read_sample_180 = (float)buffer[read_pointer_int180];

    // Check if first readpointer starts overlap with write pointer?
    //  if yes -> do cross-fade to second read-pointer
    if (overlap >= (write_pointer - read_pointer_int) &&
        (write_pointer - read_pointer_int) >= 0 && pitch_shift != 1.0f) {
      int rel = write_pointer - read_pointer_int;
      cross_fade = ((float)rel) / (float)overlap;
    } else if (write_pointer - read_pointer_int == 0)
      cross_fade = 0.0f;

    // Check if second readpointer starts overlap with write pointer?
    //  if yes -> do cross-fade to first read-pointer
    if (overlap >= (write_pointer - read_pointer_int180) &&
        (write_pointer - read_pointer_int180) >= 0 && pitch_shift != 1.0f) {
      int rel = write_pointer - read_pointer_int180;
      cross_fade = 1.0f - ((float)rel) / (float)overlap;
    } else if (write_pointer - read_pointer_int180 == 0)
      cross_fade = 1.0f;

    // do cross-fading and sum up
    T sum = (read_sample * cross_fade + read_sample_180 * (1.0f - cross_fade));

    // increment fractional read-pointer and write-pointer
    read_pos_float += pitch_shift;
    if (roundf(read_pos_float) >= buffer_size)
      read_pos_float = 0.0f;

    return sum;
  }
};


/**
 * @brief Optimized Buffer implementation for Pitch Shift.
 * We try to interpolate the samples and restore the phase
 * when the read pointer and write pointer overtake each other
 * @tparam T
 */
template <typename T> class VariableSpeedRingBuffer : public BaseBuffer<T> {
public:
  VariableSpeedRingBuffer(int size = 0, float increment = 1.0) {
    setIncrement(increment);
    if (size > 0)
      resize(size);
  }

  void setIncrement(int increment) { read_increment = increment; }

  void resize(int size) {
    buffer_size = size;
    // prevent an overrun at the start
    read_pos_int = size / 2;
    buffer.resize(size);
  }

  T read() {
    T result = peek();
    read_pos_float += read_increment;
    handleReadWriteOverrun();
    if (read_pos_float > buffer_size) {
      read_pos_float -= buffer_size;
    }
    return result;
  }

  T peek() {
    if (buffer.size() == 0)
      return 0;
    return interpolate(read_pos_float);
  }

  bool write(T sample) {
    if (buffer.size() == 0)
      return false;
    handleReadWriteOverrun();
    buffer[write_pos++] = sample;
    // on buffer owerflow reset to 0
    if (write_pos >= buffer_size) {
      write_pos = 0;
    }
    return true;
  }

  /// Reset pointer positions and clear buffer
  void reset() {
    read_pos_float = 0;
    write_pos = 0;
    memset(buffer.data(), 0, sizeof(T) * buffer_size);
  }

  virtual bool isFull() { return false; }
  virtual int available() { return buffer_size; }
  virtual int availableForWrite() { return buffer_size; }
  virtual T *address() { return nullptr; }

protected:
  Vector<T> buffer{0};
  int buffer_size;
  float read_pos_float = 0.0;
  float read_increment;
  int write_pos = 0;
  int read_pos_int;
  T value, value1, value2;

  /// Linear interpolation
  float map(float x, float in_min, float in_max, float out_min, float out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
  }

  /// Calculate exact sample value for float position
  T interpolate(int read_pos_float) {
    read_pos_int = read_pos_float; // round down
    value1 = getValue(read_pos_int);
    value2 = getValue(read_pos_int + 1);
    // the result must be between value 1 and value 2: linear interpolation
    value = round(
        map(read_pos_float, read_pos_int, read_pos_int + 1, value1, value2));
    return value;
  }

  /// provides the value from the buffer: Allows pos > buffer_size
  T getValue(int pos) { return buffer[pos % buffer_size]; }

  /// checks if we can fit the last value between 2 existing values in the
  /// buffer
  bool isMatching(T value1, bool incrementing, T v1, T v2) {
    bool v_incrementing = v2 - v1 >= 0;
    // eff sample was ascending so we need to select a ascending value
    if (incrementing && v_incrementing && value1 >= v1 && value1 <= v2) {
      return true;
    }
    // eff sample was descending so we need to select a descending value
    if (!incrementing && !v_incrementing && value1 <= v1 && value1 >= v2) {
      return true;
    }
    return false;
  }

  /// When the read pointer is overpassing the write pointer or the write pointer is
  /// overpassing the read pointer we need to phase shift
  void handleReadWriteOverrun() {
    // handle overflow - we need to allign the phase
    if (write_pos == read_pos_int ||
        write_pos == (buffer_size % (read_pos_int + 1))) {
      bool found = false;
      // find the closest match for the last value1/value2
      bool incrementing = value2 - value1 >= 0;
      for (int j = 1; j < buffer_size; j++) {
        T v1 = getValue(read_pos_int + j);
        T v2 = getValue(read_pos_int + j + 1);
        // find corresponging matching sample in buffer for last sample
        if (isMatching(value1, incrementing, v1, v2)) {
          read_pos_float =
              map(value, v1, v2, read_pos_int + j, read_pos_int + j + 1);
          // move to next value
          read_pos_float += read_increment;
          // if we are at the end of the buffer we restart from 0
          if (read_pos_float>buffer_size){
            read_pos_float-=buffer_size;
          }
          found = true;
          break;
        }
      }
      if (!found) {
        LOGW("phase allign failed: maybe the buffer is too small")
      }
    }
  }
};

/**
 * @brief Pitch Shift: Shifts the frequency up or down w/o impacting the length!
 * We reduce the channels to 1 to calculate the pitch shift and provides the
 * pitch shifted result in the correct number of channels. The pitch shifting
 * is done with the help of a buffer that can have potentially multiple
 * implementations.
 * @tparam T
 * @tparam BufferT
 */
template <typename T, class BufferT>
class PitchShift : public PitchShiftInterface<T> {
public:
  PitchShift(Print &out) { PitchShiftInterface<T>::p_out = &out; }

  bool begin(PitchShiftInfo info) {
    TRACED();
    PitchShiftInterface<T>::cfg = info;
    buffer.resize(info.buffer_size);
    buffer.reset();
    buffer.setIncrement(info.pitch_shift);
    PitchShiftInterface<T>::active = true;
    return PitchShiftInterface<T>::active;
  }

  void end() { PitchShiftInterface<T>::active = false; }

protected:
  BufferT buffer;

  // execute the pitch shift by writing one sample and returning the pitch
  // shifted result sample
  T pitchShift(T value) {
    TRACED();
    if (!PitchShiftInterface<T>::active)
      return 0;
    buffer.write(value);
    T out_value = buffer.read();
    return out_value;
  }
};

} // namespace audio_tools