
#pragma once
#include "AudioConfig.h"
#include <math.h>
#include <stdio.h>
#include <string.h>


namespace audio_tools {

/**
 * @brief Configuration for PitchShiftOutput: set the pitch_shift to define the
 * shift
 */
struct PitchShiftInfo : public AudioInfo {
  PitchShiftInfo() {
    channels = 2;
    sample_rate = 44100;
    bits_per_sample = 16;
  }
  float pitch_shift = 1.4f;
  int buffer_size = 1000;
};

/**
 * @brief Very Simple Buffer implementation for Pitch Shift. We write in
 * constant speed, but reading can be done in a variable speed. We will hear
 * some noise when the buffer read and write pointers overrun each other
 * @ingroup buffers
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

  void setIncrement(float increment) { read_increment = increment; }

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
  size_t size() {return buffer_size;}

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
 * @ingroup buffers
 * @tparam T
 */
template <typename T> class VariableSpeedRingBuffer180 : public BaseBuffer<T> {
public:
  VariableSpeedRingBuffer180(int size = 0, float increment = 1.0) {
    setIncrement(increment);
    if (size > 0)
      resize(size);
  }

  void setIncrement(float increment) { pitch_shift = increment; }

  void resize(int size) {
    buffer_size = size;
    overlap = buffer_size / 10;
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
    cross_fade = 1.0f;
    overlap = buffer_size / 10;
    memset(buffer.data(), 0, sizeof(T) * buffer_size);
  }

  virtual bool isFull() { return false; }
  virtual int available() { return buffer_size; }
  virtual int availableForWrite() { return buffer_size; }
  virtual T *address() { return nullptr; }
  size_t size() {return buffer_size;}

protected:
  Vector<T> buffer{0};
  float read_pos_float = 0.0;
  float cross_fade = 1.0;
  int write_pos = 0;
  int write_pointer = 0;
  int buffer_size = 0;
  int overlap = 0;
  float pitch_shift = 0;

  /// pitch shift for a single sample
  virtual T pitchRead() {
    TRACED();
    assert(pitch_shift > 0);
    assert(buffer_size > 0);

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
 * @ingroup buffers
 * @tparam T
 */
template <typename T> class VariableSpeedRingBuffer : public BaseBuffer<T> {
public:
  VariableSpeedRingBuffer(int size = 0, float increment = 1.0) {
    setIncrement(increment);
    if (size > 0)
      resize(size);
  }

  void setIncrement(float increment) { read_increment = increment; }

  void resize(int size) {
    buffer_size = size;
    // prevent an overrun at the start
    read_pos_float = size / 2;
    buffer.resize(size);
  }

  T read() {
    assert(read_increment != 0.0f);
    T result = peek();
    read_pos_float += read_increment;
    handleReadWriteOverrun(last_value);
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
    handleReadWriteOverrun(last_value);
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
  size_t size() {return buffer_size;}


protected:
  Vector<T> buffer{0};
  int buffer_size;
  float read_pos_float = 0.0f;
  float read_increment = 0.0f;
  int write_pos = 0;
  // used to handle overruns:
  T last_value = 0; // record last read value
  bool incrementing; // is last read increasing

  /// Calculate exact sample value for float position
  T interpolate(float read_pos) {
    int read_pos_int = read_pos;
    T value1 = getValue(read_pos_int);
    T value2 = getValue(read_pos_int + 1);
    incrementing = value2 - value1 >= 0;
 
    // make sure that value1 is smaller then value 2
    if (value2 < value1) {
      T tmp = value2;
      value2 = value1;
      value1 = tmp;
    }
    // the result must be between value 1 and value 2: linear interpolation
    float offset_in = read_pos - read_pos_int; // calculate fraction: e.g 0.5
    LOGD("read_pos=%f read_pos_int=%d, offset_in=%f", read_pos, read_pos_int,  offset_in);
    float diff_result = abs(value2 - value1); // differrence between values: e.g. 10
    float offset_result = offset_in * diff_result; // 0.5 * 10 = 5
    float result = offset_result + value1;
    LOGD("interpolate %d %d -> %f -> %f", value1, value2, offset_result, result);

    last_value = result;

    return result;
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

  /// When the read pointer is overpassing the write pointer or the write
  /// pointer is overpassing the read pointer we need to phase shift
  void handleReadWriteOverrun(T last_value) {
    // handle overflow - we need to allign the phase
    int read_pos_int = read_pos_float; // round down
    if (write_pos == read_pos_int ||
        write_pos == (buffer_size % (read_pos_int + 1))) {
      LOGD("handleReadWriteOverrun write_pos=%d read_pos_int=%d", write_pos,
           read_pos_int);
      bool found = false;

      // find the closest match for the last value
      for (int j = read_increment * 2; j < buffer_size; j++) {
        int pos = read_pos_int + j;
        float v1 = getValue(pos);
        float v2 = getValue(pos + 1);
        // find corresponging matching sample in buffer for last sample
        if (isMatching(last_value, incrementing, v1, v2)) {
          // interpolate new position
          float diff_value = abs(v1 - v2);
          float diff_last_value = abs(v1 - last_value);
          float fraction = 0;
          if (diff_value>0){
            fraction = diff_last_value / diff_value;
          }

          read_pos_float = fraction + pos;
          // move to next value
          read_pos_float += read_increment;
          // if we are at the end of the buffer we restart from 0
          if (read_pos_float > buffer_size) {
            read_pos_float -= buffer_size;
          }
          LOGD("handleReadWriteOverrun -> read_pos pos=%d  pos_float=%f", pos, read_pos_float);
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
 * @ingroup transform
 * @tparam T
 * @tparam BufferT
 */
template <typename T, class BufferT>
class PitchShiftOutput : public AudioOutput {
public:
  PitchShiftOutput(Print &out) { p_out = &out; }

  PitchShiftInfo defaultConfig() {
    PitchShiftInfo result;
    result.bits_per_sample = sizeof(T) * 8;
    return result;
  }

  bool begin(PitchShiftInfo info) {
    TRACED();
    cfg = info;
    AudioOutput::setAudioInfo(info);
    buffer.resize(info.buffer_size);
    buffer.reset();
    buffer.setIncrement(info.pitch_shift);
    active = true;
    return active;
  }

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
      LOGD("PitchShiftOutput %f -> %d", value, (int) out_value);
      T out_array[channels];
      for (int ch = 0; ch < channels; ch++) {
        out_array[ch] = out_value;
      }
      result += p_out->write((uint8_t *)out_array, sizeof(T) * channels);
    }
    return result;
  }

  void end() { active = false; }

protected:
  BufferT buffer;
  bool active;
  PitchShiftInfo cfg;
  Print *p_out = nullptr;

  // execute the pitch shift by writing one sample and returning the pitch
  // shifted result sample
  T pitchShift(T value) {
    TRACED();
    if (!active)
      return 0;
    buffer.write(value);
    T out_value = buffer.read();
    return out_value;
  }
};

} // namespace audio_tools