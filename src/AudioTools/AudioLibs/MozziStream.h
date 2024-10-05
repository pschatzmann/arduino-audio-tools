

#include "AudioTools.h"

// prevent naming conflict with audiotools
#define AudioOutput AudioOutputMozzi
#include <Mozzi.h>
#undef AudioOutput

void updateControl();
AudioOutputMozzi updateAudio();

namespace audio_tools {

struct MozziConfig : AudioInfo {
  uint16_t control_rate = CONTROL_RATE;
  int input_range_from = 0;
  int input_range_to = 1023;
  int16_t output_volume = 10;
};

/**
 * @brief Stream that provides audio information that was generated using the
 * Mozzi API using the updateControl() and updateAudio() methods.
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class MozziStream : public AudioStream, public VolumeSupport {
 public:
  MozziStream() = default;

  MozziStream(Stream& in) { setInput(in); }

  MozziConfig defaultConfig() { return cfg; }

  void setAudioInfo(AudioInfo info) { cfg.copyFrom(info); }
  
  int audioRate() { return cfg.sample_rate; }
  
  void setInput(Stream& in) { p_input = &in; }

  AudioInfo audioInfo() { return cfg; }

  bool begin(MozziConfig cfg) {
    this->cfg = cfg;
    return begin();
  }

  bool begin() {
    if (cfg.bits_per_sample != 16) {
      LOGE("bits_per_sample must be 16 and not %d", cfg.bits_per_sample);
      return false;
    }

    // initialize range for input range determination
    input_min = 32767;
    input_max = -32768;

    // setup control counter
    control_counter_max = info.sample_rate / cfg.control_rate;
    if (control_counter_max <= 0) {
      control_counter_max = 1;
    }
    control_counter = control_counter_max;
    active = true;
    return true;
  }

  void end() { active = false; }

  /// Defines the multiplication factor to scale the Mozzi value range to int16_t
  bool setVolume(int16_t vol) { 
    cfg.output_volume = vol; 
    return VolumeSupport::setVolume(vol);
  }

  /// Provides the data filled by calling updateAudio()
  size_t readBytes(uint8_t* data, size_t len) {
    if (!active) return 0;
    int samples = len / sizeof(int16_t);
    int frames = samples / cfg.channels;
    int16_t* data16 = (int16_t*)data;
    int idx = 0;
    for (int j = 0; j < frames; j++) {
      int16_t sample = nextSample();
      for (int ch = 0; ch < cfg.channels; ch++) {
        data16[idx++] = sample;
      }
    }
    return idx * sizeof(int16_t);
  }

  /// Write data to buffer so that we can access them by calling getAudioInput()
  size_t write(const uint8_t* data, size_t len) {
    if (!active) return 0;
    if (buffer.size() == 0) {
      buffer.resize(len * 2);
    }
    return buffer.writeArray(data, len);
  }

  /// Gets the next audio value either from the assigned Input Stream or the
  /// buffer that was filled by write(). The data range is defined in
  /// MozziConfig
  int getAudioInput() {
    int16_t result[cfg.channels] = {0};
    if (p_input != nullptr) {
      p_input->readBytes((uint8_t*)&result, sizeof(int16_t) * cfg.channels);
    } else {
      buffer.readArray((uint8_t*)&result, sizeof(int16_t) * cfg.channels);
    }
    // when we have multiple channels we provide the avg value
    int sample = avg(result);
    // calculate dynamic range 
    if (sample < input_min) input_min = sample;
    if (sample > input_max) input_max = sample;
    int sample1 = map(sample, input_min, input_max, cfg.input_range_from,
                      cfg.input_range_to);
    return sample1;
  }

 protected:
  MozziConfig cfg;
  int control_counter_max;
  int control_counter;
  RingBuffer<uint8_t> buffer{0};
  Stream* p_input = nullptr;
  bool active = false;
  int input_min = 32767;
  int input_max = -32768;

  int16_t avg(int16_t* values) {
    int total = 0;
    for (int ch = 0; ch < cfg.channels; ch++) {
      total += values[ch];
    }
    return total / cfg.channels;
  }

  int16_t nextSample() {
    // control update
    if (--control_counter < 0) {
      control_counter = control_counter_max;
      LOGD("updateControl");
      updateControl();
    }

    // updateAudio() STANDARD mode is between -244 and 243 inclusive
    auto result = updateAudio() * cfg.output_volume;
    // clip result
    if (result > 32767) result = 32767;
    if (result < -32768) result = -32768;
    // Serial.println(result);
    return result;
  }
};

}  // namespace audio_tools