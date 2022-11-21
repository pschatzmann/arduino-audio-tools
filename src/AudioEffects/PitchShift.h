
#include "Print.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

namespace audio_tools {

/**
 * @brief Configuration for PitchShift: set the pitch_shift to define the shift
 *
 */
struct PitchShiftInfo : public AudioBaseInfo {
  PitchShiftInfo() {
    channels = 2;
    sample_rate = 44100;
    bits_per_sample = 16;
  }
  float pitch_shift = 1.4f;
  int pitch_buffer_size = 1000;
};

/**
 * @brief: Shifts the frequency up or down w/o impacting the length!
 * See https://github.com/YetAnotherElectronicsChannel/STM32_DSP_PitchShift
 * */

template <typename T> class PitchShift : public AudioPrint {
public:
  PitchShift(Print &out) { p_out = &out; }

  PitchShiftInfo defaultConfig() {
    TRACED();
    PitchShiftInfo result;
    return result;
  }

  bool begin(PitchShiftInfo info) {
    TRACED();
    cfg = info;

    // setup initial values
    write_pointer = 0.0;
    read_pointer_float = 0.0f;
    cross_fade = 1.0f;
    pitch_buffer_size = cfg.pitch_buffer_size;
    overlap = pitch_buffer_size / 10;

    active = setupBuffer(pitch_buffer_size);
    if (!active){
      LOGE("begin out of memory");
    }
    return active;
  }

  void end() { active = false; }

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
  Vector<T> pitch_shift_buffer{0};
  float read_pointer_float = 0.0;
  float cross_fade = 1.0;
  PitchShiftInfo cfg;
  Print *p_out = nullptr;
  int write_pointer = 0;
  int pitch_buffer_size = 0;
  int overlap = 0;
  bool active = false;

  bool setupBuffer(int size) {
    pitch_buffer_size = size;
    pitch_shift_buffer.resize(pitch_buffer_size);
    memset(pitch_shift_buffer.data(),0,pitch_buffer_size*sizeof(T));
    return pitch_shift_buffer.data()!=nullptr;
  }

  /// pitch shift for a single sample
  T pitchShift(T sample) {
    TRACED();
    // write to ringbuffer
    pitch_shift_buffer[write_pointer] = sample;

    // read fractional readpointer and generate 0° and 180° read-pointer in
    // integer
    int read_pointer_int = roundf(read_pointer_float);
    int read_pointer_int180 = 0;
    if (read_pointer_int >= pitch_buffer_size / 2)
      read_pointer_int180 = read_pointer_int - (pitch_buffer_size / 2);
    else
      read_pointer_int180 = read_pointer_int + (pitch_buffer_size / 2);

    // read the two samples...
    float read_sample = (float)pitch_shift_buffer[read_pointer_int];
    float read_sample_180 = (float)pitch_shift_buffer[read_pointer_int180];

    // Check if first readpointer starts overlap with write pointer?
    //  if yes -> do cross-fade to second read-pointer
    if (overlap >= (write_pointer - read_pointer_int) &&
        (write_pointer - read_pointer_int) >= 0 && cfg.pitch_shift != 1.0f) {
      int rel = write_pointer - read_pointer_int;
      cross_fade = ((float)rel) / (float)overlap;
    } else if (write_pointer - read_pointer_int == 0)
      cross_fade = 0.0f;

    // Check if second readpointer starts overlap with write pointer?
    //  if yes -> do cross-fade to first read-pointer
    if (overlap >= (write_pointer - read_pointer_int180) &&
        (write_pointer - read_pointer_int180) >= 0 && cfg.pitch_shift != 1.0f) {
      int rel = write_pointer - read_pointer_int180;
      cross_fade = 1.0f - ((float)rel) / (float)overlap;
    } else if (write_pointer - read_pointer_int180 == 0)
      cross_fade = 1.0f;

    // do cross-fading and sum up
    T sum = (read_sample * cross_fade + read_sample_180 * (1.0f - cross_fade));

    // increment fractional read-pointer and write-pointer
    read_pointer_float += cfg.pitch_shift;
    write_pointer++;
    if (write_pointer == pitch_buffer_size)
      write_pointer = 0;
    if (roundf(read_pointer_float) >= pitch_buffer_size)
      read_pointer_float = 0.0f;

    return sum;
  }
};

} // namespace audio_tools