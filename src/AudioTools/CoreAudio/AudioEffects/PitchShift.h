
/**
 * @file PitchShift.h
 * @brief Real-time pitch shifting audio effect implementation
 * 
 * This file contains classes for performing real-time pitch shifting on audio streams.
 * Pitch shifting changes the frequency (pitch) of audio without affecting its duration,
 * allowing for creative audio effects like chipmunk voices, deeper bass tones, or
 * musical pitch correction.
 * 
 * The implementation provides three different buffer algorithms with varying quality
 * and computational complexity:
 * 
 * 1. **VariableSpeedRingBufferSimple**: Basic implementation with potential artifacts
 *    - Fastest processing
 *    - Suitable for simple applications where quality is not critical
 *    - May produce audible artifacts during buffer overruns
 * 
 * 2. **VariableSpeedRingBuffer180**: Advanced implementation with 180° phase offset
 *    - Good quality with moderate complexity
 *    - Uses dual read pointers with cross-fading to reduce artifacts
 *    - Based on STM32 DSP techniques
 * 
 * 3. **VariableSpeedRingBuffer**: Premium implementation with interpolation
 *    - Highest quality with advanced features
 *    - Linear interpolation for smooth sample transitions
 *    - Automatic phase alignment during pointer collisions
 *    - Best for professional audio applications
 * 
 * @note Pitch shifting introduces some latency due to buffering. Buffer size affects
 *       both quality and latency - larger buffers provide better quality but more delay.
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#pragma once
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "AudioTools/CoreAudio/AudioOutput.h"
#include "AudioTools/CoreAudio/AudioTypes.h"
#include "AudioToolsConfig.h"

namespace audio_tools {

/**
 * @brief Configuration for PitchShiftOutput
 * 
 * This structure contains all the parameters needed to configure pitch shifting.
 * The pitch_shift parameter determines the amount of frequency shift applied to the audio.
 * Values > 1.0 increase pitch (higher frequency), values < 1.0 decrease pitch (lower frequency).
 * A value of 1.0 means no pitch change.
 * 
 * @note The buffer_size affects the quality and latency of the pitch shifting.
 *       Larger buffers provide better quality but increase latency.
 */
struct PitchShiftInfo : public AudioInfo {
  PitchShiftInfo() {
    channels = 2;
    sample_rate = 44100;
    bits_per_sample = 16;
  }
  
  /// Pitch shift factor: 1.0 = no change, >1.0 = higher pitch, <1.0 = lower pitch
  float pitch_shift = 1.4f;
  
  /// Size of the internal buffer used for pitch shifting (affects quality and latency)
  int buffer_size = 1000;
};

/**
 * @brief Very Simple Buffer implementation for Pitch Shift
 * 
 * This buffer writes samples at constant speed but allows reading at variable speed
 * to achieve pitch shifting. The reading speed is controlled by the increment parameter.
 * When read and write pointers overlap, some audio artifacts (noise) may be audible.
 * 
 * @warning This is a basic implementation that may produce audible artifacts during
 *          pointer overruns. For better quality, consider using VariableSpeedRingBuffer180
 *          or VariableSpeedRingBuffer.
 * 
 * @ingroup buffers
 * @tparam T The sample data type (typically int16_t or float)
 */
template <typename T = int16_t>
class VariableSpeedRingBufferSimple : public BaseBuffer<T> {
 public:
  /**
   * @brief Constructor
   * @param size Initial buffer size (0 means no allocation yet)
   * @param increment Reading speed multiplier (1.0 = normal speed, >1.0 = faster reading for higher pitch)
   */
  VariableSpeedRingBufferSimple(int size = 0, float increment = 1.0) {
    setIncrement(increment);
    if (size > 0) resize(size);
  }

  /**
   * @brief Set the reading speed increment
   * @param increment Reading speed multiplier (1.0 = normal, >1.0 = faster for higher pitch)
   */
  void setIncrement(float increment) { read_increment = increment; }

  /**
   * @brief Resize the internal buffer
   * @param size New buffer size in samples
   * @return true if successful, false on memory allocation failure
   */
  bool resize(int size) {
    buffer_size = size;
    return buffer.resize(size);
  }

  /**
   * @brief Read the next sample and advance the read pointer
   * @param result Reference to store the read sample value
   * @return true if successful
   */
  /**
   * @brief Read the next sample and advance the read pointer
   * @param result Reference to store the read sample value
   * @return true if successful
   */
  bool read(T &result) {
    peek(result);
    read_pos_float += read_increment;
    // on buffer overflow reset to beginning
    if (read_pos_float > buffer_size) {
      read_pos_float -= buffer_size;
    }
    return true;
  }

  /**
   * @brief Peek at the current sample without advancing the read pointer
   * @param result Reference to store the sample value
   * @return true if successful
   */
  bool peek(T &result) {
    if (buffer.size() == 0) {
      LOGE("buffer has no memory");
      result = 0;
    } else {
      result = buffer[(int)read_pos_float];
    }
    return true;
  }

  /**
   * @brief Write a sample to the buffer
   * @param sample The sample value to write
   * @return true if successful, false if buffer is not allocated
   */
  bool write(T sample) {
    if (buffer.size() == 0) {
      LOGE("buffer has no memory");
      return false;
    }
    buffer[write_pos++] = sample;
    // on buffer overflow reset to 0
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
  size_t size() { return buffer_size; }

 protected:
  Vector<T> buffer{0};
  int buffer_size = 0;
  float read_pos_float = 0.0;
  float read_increment = 1.0;
  int write_pos = 0;
};

/**
 * @brief Variable speed ring buffer with 180-degree phase shifting
 * 
 * Advanced buffer implementation that reads with 0° and 180° phase offsets and
 * blends the results to prevent overrun artifacts. This technique reduces audio
 * artifacts when read and write pointers collide by cross-fading between two
 * read positions that are 180° apart (half buffer apart).
 * 
 * Based on the algorithm described at:
 * https://github.com/YetAnotherElectronicsChannel/STM32_DSP_PitchShift
 * 
 * @ingroup buffers
 * @tparam T The sample data type (typically int16_t or float)
 */
template <typename T = int16_t>
class VariableSpeedRingBuffer180 : public BaseBuffer<T> {
 public:
  /**
   * @brief Constructor
   * @param size Initial buffer size (0 means no allocation yet)
   * @param increment Pitch shift factor (1.0 = no change, >1.0 = higher pitch)
   */
  VariableSpeedRingBuffer180(int size = 0, float increment = 1.0) {
    setIncrement(increment);
    if (size > 0) resize(size);
  }

  /**
   * @brief Set the pitch shift factor
   * @param increment Pitch shift multiplier (1.0 = no change, >1.0 = higher pitch, <1.0 = lower pitch)
   */
  void setIncrement(float increment) { pitch_shift = increment; }

  /**
   * @brief Resize the internal buffer and recalculate overlap region
   * @param size New buffer size in samples
   * @return true if successful
   */
  bool resize(int size) {
    buffer_size = size;
    overlap = buffer_size / 10;
    return buffer.resize(size);
  }

  /**
   * @brief Read the next pitch-shifted sample
   * @param result Reference to store the processed sample
   * @return true if successful
   */
  bool read(T &result) {
    result = pitchRead();
    return true;
  }

  /**
   * @brief Peek operation not supported in this buffer implementation
   * @param result Unused parameter
   * @return Always returns false (not supported)
   */
  bool peek(T &result) { return false; }

  /**
   * @brief Write a sample to the buffer
   * @param sample The sample value to write
   * @return true if successful, false if buffer not allocated
   */
  bool write(T sample) {
    if (buffer.size() == 0) {
      LOGE("buffer has no memory");
      return false;
    }
    // write_pointer value is used in pitchRead()
    write_pointer = write_pos;
    buffer[write_pos++] = sample;
    // on buffer overflow reset to 0
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
  size_t size() { return buffer_size; }

 protected:
  Vector<T> buffer{0};
  float read_pos_float = 0.0;
  float cross_fade = 1.0;
  int write_pos = 0;
  int write_pointer = 0;
  int buffer_size = 0;
  int overlap = 0;
  float pitch_shift = 0;

  /**
   * @brief Core pitch shifting algorithm with 180° phase offset blending
   * 
   * This method implements the heart of the pitch shifting algorithm:
   * 1. Creates two read pointers: one at current position, one 180° (half-buffer) away
   * 2. Reads samples from both positions
   * 3. Detects when read pointers approach the write pointer
   * 4. Cross-fades between the two samples to prevent artifacts
   * 5. Advances the read position by the pitch_shift factor
   * 
   * The 180° offset ensures that when one read pointer collides with the write
   * pointer, the other is safely away, allowing smooth transition.
   * 
   * @return Pitch-shifted sample with cross-fading applied
   */
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
    if (roundf(read_pos_float) >= buffer_size) read_pos_float = 0.0f;

    return sum;
  }
};

/**
 * @brief Optimized buffer implementation for pitch shifting with interpolation
 * 
 * This is the most advanced buffer implementation that uses interpolation to
 * calculate exact sample values for fractional read positions and attempts to
 * restore phase continuity when read and write pointers overtake each other.
 * This provides the highest quality pitch shifting with minimal artifacts.
 * 
 * Key features:
 * - Linear interpolation for smooth sample transitions
 * - Phase alignment during pointer collisions
 * - Automatic phase restoration to minimize clicks and pops
 * 
 * @ingroup buffers
 * @tparam T The sample data type (typically int16_t or float)
 */
template <typename T = int16_t>
class VariableSpeedRingBuffer : public BaseBuffer<T> {
 public:
  /**
   * @brief Constructor
   * @param size Initial buffer size (0 means no allocation yet)
   * @param increment Reading speed multiplier for pitch shifting
   */
  VariableSpeedRingBuffer(int size = 0, float increment = 1.0) {
    setIncrement(increment);
    if (size > 0) resize(size);
  }

  /**
   * @brief Set the reading speed increment for pitch shifting
   * @param increment Speed multiplier (1.0 = normal, >1.0 = faster for higher pitch)
   */
  void setIncrement(float increment) { read_increment = increment; }

  /**
   * @brief Resize buffer and set initial read position to prevent immediate overrun
   * @param size New buffer size in samples
   * @return true if successful
   */
  bool resize(int size) {
    buffer_size = size;
    // prevent an overrun at the start
    read_pos_float = size / 2;
    return buffer.resize(size);
  }

  bool read(T &result) {
    assert(read_increment != 0.0f);
    peek(result);
    read_pos_float += read_increment;
    handleReadWriteOverrun(last_value);
    if (read_pos_float > buffer_size) {
      read_pos_float -= buffer_size;
    }
    return true;
  }

  bool peek(T &result) {
    if (buffer.size() == 0) {
      result = 0;
    } else {
      result = interpolate(read_pos_float);
    }
    return true;
  }

  bool write(T sample) {
    if (buffer.size() == 0) return false;
    handleReadWriteOverrun(last_value);
    buffer[write_pos++] = sample;
    // on buffer overflow reset to 0
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
  size_t size() { return buffer_size; }

 protected:
  Vector<T> buffer{0};
  int buffer_size;
  float read_pos_float = 0.0f;
  float read_increment = 0.0f;
  int write_pos = 0;
  // used to handle overruns:
  T last_value = 0;   ///< Record last read value for phase alignment
  bool incrementing;  ///< Track if last read trend was increasing or decreasing

  /**
   * @brief Calculate exact sample value for fractional buffer position using linear interpolation
   * 
   * This method performs linear interpolation between two adjacent samples to provide
   * smooth audio output when reading at fractional positions. It also tracks the
   * direction of change (increasing/decreasing) for phase alignment purposes.
   * 
   * @param read_pos Fractional position in buffer (e.g., 10.5 means halfway between samples 10 and 11)
   * @return Interpolated sample value
   */
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
    float offset_in = read_pos - read_pos_int;  // calculate fraction: e.g 0.5
    LOGD("read_pos=%f read_pos_int=%d, offset_in=%f", read_pos, read_pos_int,
         offset_in);
    float diff_result =
        abs(value2 - value1);  // differrence between values: e.g. 10
    float offset_result = offset_in * diff_result;  // 0.5 * 10 = 5
    float result = offset_result + value1;
    LOGD("interpolate %d %d -> %f -> %f", value1, value2, offset_result,
         result);

    last_value = result;

    return result;
  }

  /**
   * @brief Get buffer value with wraparound support
   * @param pos Position in buffer (can exceed buffer_size, will wrap around)
   * @return Sample value at the wrapped position
   */
  T getValue(int pos) { return buffer[pos % buffer_size]; }

  /**
   * @brief Check if a value fits between two samples considering trend direction
   * 
   * This method determines if the last read value can logically fit between
   * two consecutive buffer samples, taking into account whether the audio
   * signal was increasing or decreasing at that point.
   * 
   * @param value1 The last read value to check
   * @param incrementing Whether the last read was part of an increasing trend
   * @param v1 First sample value
   * @param v2 Second sample value  
   * @return true if value1 fits logically between v1 and v2
   */
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

  /**
   * @brief Handle read/write pointer collisions with phase alignment
   * 
   * When read and write pointers collide, this method attempts to maintain
   * audio continuity by finding the best matching sample in the buffer that
   * corresponds to the last read value's phase and trend. This minimizes
   * audible clicks and pops during pointer overruns.
   * 
   * The algorithm:
   * 1. Detects when pointers are about to collide
   * 2. Searches for a matching sample pair that fits the last read value
   * 3. Interpolates the new read position within that pair
   * 4. Repositions the read pointer to maintain phase continuity
   * 
   * @param last_value The last successfully read sample value
   */
  void handleReadWriteOverrun(T last_value) {
    // handle overflow - we need to allign the phase
    int read_pos_int = read_pos_float;  // round down
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
          if (diff_value > 0) {
            fraction = diff_last_value / diff_value;
          }

          read_pos_float = fraction + pos;
          // move to next value
          read_pos_float += read_increment;
          // if we are at the end of the buffer we restart from 0
          if (read_pos_float > buffer_size) {
            read_pos_float -= buffer_size;
          }
          LOGD("handleReadWriteOverrun -> read_pos pos=%d  pos_float=%f", pos,
               read_pos_float);
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
 * @brief Real-time pitch shifting audio effect
 * 
 * This class implements real-time pitch shifting that changes the frequency of audio
 * without affecting its duration. It can shift pitch up or down while maintaining
 * the original playback speed. The implementation:
 * 
 * 1. Reduces multi-channel audio to mono for processing
 * 2. Applies pitch shifting using a configurable buffer algorithm
 * 3. Outputs the shifted audio in the original channel configuration
 * 
 * The pitch shifting is accomplished using a variable-speed ring buffer that reads
 * at different rates than it writes. Three buffer implementations are available:
 * - VariableSpeedRingBufferSimple: Basic implementation with potential artifacts
 * - VariableSpeedRingBuffer180: Uses 180° phase shifting to reduce artifacts  
 * - VariableSpeedRingBuffer: Advanced implementation with interpolation and phase restoration
 * 
 * @note Pitch shifting is a complex DSP operation that requires buffering, which
 *       introduces some latency. The buffer size affects both quality and latency.
 * 
 * @ingroup transform
 * @tparam T Sample data type (int16_t, int32_t, float, etc.)
 * @tparam BufferT Buffer implementation type (one of the VariableSpeedRingBuffer variants)
 */
template <typename T, class BufferT>
class PitchShiftOutput : public AudioOutput {
 public:
  /**
   * @brief Constructor
   * @param out Reference to the output stream where processed audio will be written
   */
  PitchShiftOutput(Print &out) { p_out = &out; }

  /**
   * @brief Get default configuration for pitch shifting
   * @return PitchShiftInfo with default values appropriate for type T
   */
  PitchShiftInfo defaultConfig() {
    PitchShiftInfo result;
    result.bits_per_sample = sizeof(T) * 8;
    return result;
  }

  /**
   * @brief Initialize pitch shifting with configuration
   * @param info Configuration containing pitch_shift factor, buffer_size, and audio format
   * @return true if initialization successful
   */
  bool begin(PitchShiftInfo info) {
    TRACED();
    cfg = info;
    return begin();
  }

  /**
   * @brief Initialize pitch shifting with current configuration
   * @return true if initialization successful
   */
  bool begin() {
    AudioOutput::setAudioInfo(cfg);
    buffer.resize(cfg.buffer_size);
    buffer.reset();
    buffer.setIncrement(cfg.pitch_shift);
    active = true;
    return active;
  }

  /**
   * @brief Process and write audio data with pitch shifting applied
   * 
   * This method processes the input audio by:
   * 1. Converting multi-channel samples to mono by averaging
   * 2. Applying pitch shifting to the mono signal
   * 3. Duplicating the processed signal to all output channels
   * 
   * @param data Pointer to input audio data
   * @param len Number of bytes to process
   * @return Number of bytes written to output
   */
  size_t write(const uint8_t *data, size_t len) override {
    LOGD("PitchShiftOutput::write %d bytes", (int)len);
    if (!active) return 0;

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
      LOGD("PitchShiftOutput %f -> %d", value, (int)out_value);
      T out_array[channels];
      for (int ch = 0; ch < channels; ch++) {
        out_array[ch] = out_value;
      }
      result += p_out->write((uint8_t *)out_array, sizeof(T) * channels);
    }
    return result;
  }

  /**
   * @brief Stop pitch shifting and deactivate the effect
   */
  void end() { active = false; }

 protected:
  BufferT buffer;           ///< Variable speed buffer for pitch shifting
  PitchShiftInfo cfg;       ///< Current configuration
  Print *p_out = nullptr;   ///< Output stream for processed audio
  bool active = false;      ///< Whether pitch shifting is currently active

  /**
   * @brief Execute pitch shift on a single sample
   * 
   * This method performs the core pitch shifting operation by writing
   * the input sample to the buffer and reading back the pitch-shifted result.
   * The time difference between write and read operations, controlled by
   * the buffer's increment factor, creates the pitch shift effect.
   * 
   * @param value Input sample value
   * @return Pitch-shifted sample value
   */
  T pitchShift(T value) {
    TRACED();
    if (!active) return 0;
    buffer.write(value);
    T result = 0;
    buffer.read(result);
    return result;
  }
};

}  // namespace audio_tools