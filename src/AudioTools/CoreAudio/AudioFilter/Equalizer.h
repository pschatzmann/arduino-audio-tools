#pragma once
#include <math.h>

#include "AudioToolsConfig.h"
#include "AudioTools/CoreAudio/AudioOutput.h"
#include "AudioTools/CoreAudio/AudioStreams.h"

/**
 * @defgroup equilizer Equalizer
 * @ingroup dsp
 * @brief Digital Equalizer
 **/

namespace audio_tools {

/**
 * @brief Configuration for 3 Band Equalizer: Set
 * channels,bits_per_sample,sample_rate.  Set and update gain_low, gain_medium
 * and gain_high to value between 0 and 1.0
 * @ingroup equilizer
 * @author pschatzmann
 */
struct ConfigEqualizer3Bands : public AudioInfo {
  ConfigEqualizer3Bands() {
    channels = 2;
    bits_per_sample = 16;
    sample_rate = 44100;
  }

  // Frequencies
  int freq_low = 880;
  int freq_high = 5000;

  // Gain Controls
  float gain_low = 1.0;
  float gain_medium = 1.0;
  float gain_high = 1.0;
};

/**
 * @brief 3 Band Equalizer inspired from
 * https://www.musicdsp.org/en/latest/Filters/236-3-band-equaliser.html
 * @ingroup equilizer
 * @author pschatzmann
 */
class Equalizer3Bands : public ModifyingStream {
 public:
  Equalizer3Bands(Print &out) { setOutput(out); }

  Equalizer3Bands(Stream &in) { setStream(in); }

  Equalizer3Bands(AudioOutput &out) {
    setOutput(out);
    out.addNotifyAudioChange(*this);
  }

  Equalizer3Bands(AudioStream &stream) {
    setStream(stream);
    stream.addNotifyAudioChange(*this);
  }

  ~Equalizer3Bands() {
    if (state != nullptr) delete[] state;
  }

  /// Defines/Changes the input & output
  void setStream(Stream &io) override {
    p_print = &io;
    p_stream = &io;
  };

  /// Defines/Changes the output target
  void setOutput(Print &out) override { p_print = &out; }

  ConfigEqualizer3Bands &config() { return cfg; }

  ConfigEqualizer3Bands &defaultConfig() { return config(); }

  bool begin(ConfigEqualizer3Bands &config) {
    p_cfg = &config;
    if (p_cfg->channels > max_state_count) {
      if (state != nullptr) delete[] state;
      state = new EQSTATE[p_cfg->channels];
      max_state_count = p_cfg->channels;
    }

    // Setup state
    for (int j = 0; j < max_state_count; j++) {
      memset(&state[j], 0, sizeof(EQSTATE));

      // Calculate filter cutoff frequencies
      state[j].lf =
          2 *
          sin((float)PI * ((float)p_cfg->freq_low / (float)p_cfg->sample_rate));
      state[j].hf = 2 * sin((float)PI * ((float)p_cfg->freq_high /
                                         (float)p_cfg->sample_rate));
    }
    return true;
  }

  virtual void setAudioInfo(AudioInfo info) override {
    p_cfg->sample_rate = info.sample_rate;
    p_cfg->channels = info.channels;
    p_cfg->bits_per_sample = info.bits_per_sample;
    begin(*p_cfg);
  }

  size_t write(const uint8_t *data, size_t len) override {
    filterSamples(data, len);
    return p_print->write(data, len);
  }

  int availableForWrite() override { return p_print->availableForWrite(); }

  /// Provides the data from all streams mixed together
  size_t readBytes(uint8_t *data, size_t len) override {
    size_t result = 0;
    if (p_stream != nullptr) {
      result = p_stream->readBytes(data, len);
      filterSamples(data, len);
    }
    return result;
  }

  int available() override {
    return p_stream != nullptr ? p_stream->available() : 0;
  }

 protected:
  ConfigEqualizer3Bands cfg;
  ConfigEqualizer3Bands *p_cfg = &cfg;
  const float vsa = (1.0 / 4294967295.0);  // Very small amount (Denormal Fix)
  Print *p_print = nullptr;                // support for write
  Stream *p_stream = nullptr;              // support for write
  // AudioOutput *p_out=nullptr; // support for write
  // AudioStream *p_in=nullptr; // support for readBytes
  int max_state_count = 0;

  struct EQSTATE {
    // Filter #1 (Low band)
    float lf;    // Frequency
    float f1p0;  // Poles ...
    float f1p1;
    float f1p2;
    float f1p3;

    // Filter #2 (High band)
    float hf;    // Frequency
    float f2p0;  // Poles ...
    float f2p1;
    float f2p2;
    float f2p3;

    // Sample history buffer
    float sdm1;  // Sample data minus 1
    float sdm2;  //                   2
    float sdm3;  //                   3

  } *state = nullptr;

  void filterSamples(const uint8_t *data, size_t len) {
    if (state == nullptr){
      LOGE("You need to call begin() before using the equalizer");
      return;
    }
    switch (p_cfg->bits_per_sample) {
      case 16: {
        int16_t *p_dataT = (int16_t *)data;
        size_t sample_count = len / sizeof(int16_t);
        for (size_t j = 0; j < sample_count; j += p_cfg->channels) {
          for (int ch = 0; ch < p_cfg->channels; ch++) {
            p_dataT[j + ch] = NumberConverter::fromFloat(sample(state[ch], NumberConverter::toFloat(p_dataT[j + ch], 16)), 16);
          }
        }
      } break;
      case 24: {
        int24_t *p_dataT = (int24_t *)data;
        size_t sample_count = len / sizeof(int24_t);
        for (size_t j = 0; j < sample_count; j += p_cfg->channels) {
          for (int ch = 0; ch < p_cfg->channels; ch++) {
            p_dataT[j + ch] = NumberConverter::fromFloat(sample(state[ch], NumberConverter::toFloat(p_dataT[j + ch], 24)), 24);
          }
        }
      } break;
      case 32: {
        int32_t *p_dataT = (int32_t *)data;
        size_t sample_count = len / sizeof(int32_t);
        for (size_t j = 0; j < sample_count; j += p_cfg->channels) {
          for (int ch = 0; ch < p_cfg->channels; ch++) {
            p_dataT[j + ch] = NumberConverter::fromFloat(sample(state[ch], NumberConverter::toFloat(p_dataT[j + ch], 32)), 32);
          }
        }
      } break;

      default:
        LOGE("Only 16 bits supported: %d", p_cfg->bits_per_sample);
        break;
    }
  }


  // calculates a single sample using the indicated state
  float sample(EQSTATE &es, float sample) {
    // Locals
    float l, m, h;  // Low / Mid / High - Sample Values
    // Filter #1 (lowpass)
    es.f1p0 += (es.lf * (sample - es.f1p0)) + vsa;
    es.f1p1 += (es.lf * (es.f1p0 - es.f1p1));
    es.f1p2 += (es.lf * (es.f1p1 - es.f1p2));
    es.f1p3 += (es.lf * (es.f1p2 - es.f1p3));

    l = es.f1p3;

    // Filter #2 (highpass)
    es.f2p0 += (es.hf * (sample - es.f2p0)) + vsa;
    es.f2p1 += (es.hf * (es.f2p0 - es.f2p1));
    es.f2p2 += (es.hf * (es.f2p1 - es.f2p2));
    es.f2p3 += (es.hf * (es.f2p2 - es.f2p3));

    h = es.sdm3 - es.f2p3;
    // Calculate midrange (signal - (low + high))
    m = es.sdm3 - (h + l);
    // Scale, Combine and store
    l *= p_cfg->gain_low;
    m *= p_cfg->gain_medium;
    h *= p_cfg->gain_high;

    // Shuffle history buffer
    es.sdm3 = es.sdm2;
    es.sdm2 = es.sdm1;
    es.sdm1 = sample;

    // Return result
    return (l + m + h);
  }
};

/**
 * @brief 3 Band Equalizer with per-channel frequency and gain control
 * Allows independent frequency and gain settings for each audio channel.
 * Each channel can have different low/high frequency cutoffs and different
 * gain values for low, medium, and high frequency bands.
 * @ingroup equilizer
 * @author pschatzmann
 */
class Equalizer3BandsPerChannel : public ModifyingStream {
 public:
  /// Constructor with Print output
  /// @param out Print stream for output
  Equalizer3BandsPerChannel(Print &out) { setOutput(out); }

  /// Constructor with Stream input
  /// @param in Stream for input
  Equalizer3BandsPerChannel(Stream &in) { setStream(in); }

  /// Constructor with AudioOutput
  /// @param out AudioOutput for output with automatic audio change notifications
  Equalizer3BandsPerChannel(AudioOutput &out) {
    setOutput(out);
    out.addNotifyAudioChange(*this);
  }

  /// Constructor with AudioStream
  /// @param stream AudioStream for input/output with automatic audio change notifications
  Equalizer3BandsPerChannel(AudioStream &stream) {
    setStream(stream);
    stream.addNotifyAudioChange(*this);
  }

  ~Equalizer3BandsPerChannel() {
    if (state != nullptr) delete[] state;
  }

  /// Defines/Changes the input & output
  void setStream(Stream &io) override {
    p_print = &io;
    p_stream = &io;
  };

  /// Defines/Changes the output target
  void setOutput(Print &out) override { p_print = &out; }

  ConfigEqualizer3Bands &config() { return cfg; }

  ConfigEqualizer3Bands &defaultConfig() { return config(); }

  /// Initialize the equalizer with the given configuration
  /// @param config Configuration settings for the equalizer
  /// @return true if initialization was successful
  bool begin(ConfigEqualizer3Bands config) {
    p_cfg = &config;
    return begin();
  }

  /// Initialize the equalizer using the current configuration
  /// @return true if initialization was successful
  bool begin(){ 
    // Ensure per-channel arrays are allocated
    ensureChannelArraysAllocated();

    // Ensure that EQSTATE is allocated 
    if (p_cfg->channels > max_state_count) {
      if (state != nullptr) delete[] state;
      state = new EQSTATE[p_cfg->channels];
      max_state_count = p_cfg->channels;
    }

    // Setup state for each channel with its own parameters
    for (int j = 0; j < p_cfg->channels; j++) {
      memset(&state[j], 0, sizeof(EQSTATE));

      // Calculate filter cutoff frequencies per channel
      state[j].lf = 2 * sin((float)PI * ((float)freq_low[j] / (float)p_cfg->sample_rate));
      state[j].hf = 2 * sin((float)PI * ((float)freq_high[j] / (float)p_cfg->sample_rate));
    }
    return true;
  }

  virtual void setAudioInfo(AudioInfo info) override {
    p_cfg->sample_rate = info.sample_rate;
    p_cfg->channels = info.channels;
    p_cfg->bits_per_sample = info.bits_per_sample;
    begin(*p_cfg);
  }

  /// Set frequency parameters for a specific channel
  void setChannelFrequencies(int channel, int freq_low_val, int freq_high_val) {
    ensureChannelArraysAllocated();
    if (channel >= 0 && channel < p_cfg->channels && !freq_low.empty()) {
      freq_low[channel] = freq_low_val;
      freq_high[channel] = freq_high_val;
      
      // Recalculate filter coefficients for this channel
      if (state != nullptr) {
        state[channel].lf = 2 * sin((float)PI * ((float)freq_low_val / (float)p_cfg->sample_rate));
        state[channel].hf = 2 * sin((float)PI * ((float)freq_high_val / (float)p_cfg->sample_rate));
      }
    }
  }

  /// Set gain parameters for a specific channel
  void setChannelGains(int channel, float gain_low_val, float gain_medium_val, float gain_high_val) {
    ensureChannelArraysAllocated();
    if (channel >= 0 && channel < p_cfg->channels && !gain_low.empty()) {
      gain_low[channel] = gain_low_val;
      gain_medium[channel] = gain_medium_val;
      gain_high[channel] = gain_high_val;
    }
  }

  /// Get frequency parameters for a specific channel
  bool getChannelFrequencies(int channel, int &freq_low_val, int &freq_high_val) {
    if (channel >= 0 && channel < p_cfg->channels && !freq_low.empty()) {
      freq_low_val = freq_low[channel];
      freq_high_val = freq_high[channel];
      return true;
    }
    return false;
  }

  /// Get gain parameters for a specific channel
  bool getChannelGains(int channel, float &gain_low_val, float &gain_medium_val, float &gain_high_val) {
    if (channel >= 0 && channel < p_cfg->channels && !gain_low.empty()) {
      gain_low_val = gain_low[channel];
      gain_medium_val = gain_medium[channel];
      gain_high_val = gain_high[channel];
      return true;
    }
    return false;
  }

  size_t write(const uint8_t *data, size_t len) override {
    filterSamples(data, len);
    return p_print->write(data, len);
  }

  int availableForWrite() override { return p_print->availableForWrite(); }

  /// Provides the data from all streams mixed together
  size_t readBytes(uint8_t *data, size_t len) override {
    size_t result = 0;
    if (p_stream != nullptr) {
      result = p_stream->readBytes(data, len);
      filterSamples(data, len);
    }
    return result;
  }

  int available() override {
    return p_stream != nullptr ? p_stream->available() : 0;
  }

 protected:
  ConfigEqualizer3Bands cfg;
  ConfigEqualizer3Bands *p_cfg = &cfg;
  const float vsa = (1.0 / 4294967295.0);  // Very small amount (Denormal Fix)
  Print *p_print = nullptr;                // support for write
  Stream *p_stream = nullptr;              // support for write
  int max_state_count = 0;

  // Per-channel frequency settings using Vector
  Vector<int> freq_low;
  Vector<int> freq_high;

  // Per-channel gain controls using Vector
  Vector<float> gain_low;
  Vector<float> gain_medium;
  Vector<float> gain_high;

  struct EQSTATE {
    // Filter #1 (Low band)
    float lf;    // Frequency
    float f1p0;  // Poles ...
    float f1p1;
    float f1p2;
    float f1p3;

    // Filter #2 (High band)
    float hf;    // Frequency
    float f2p0;  // Poles ...
    float f2p1;
    float f2p2;
    float f2p3;

    // Sample history buffer
    float sdm1;  // Sample data minus 1
    float sdm2;  //                   2
    float sdm3;  //                   3

  } *state = nullptr;

  /// Ensures that per-channel arrays are allocated and properly sized
  void ensureChannelArraysAllocated() {
    if (freq_low.size() != p_cfg->channels) {
      allocateChannelArrays(p_cfg->channels);
    }
  }

  /// Allocates and initializes per-channel frequency and gain arrays
  /// @param num_channels Number of channels to allocate arrays for
  void allocateChannelArrays(int num_channels) {
    // Resize all vectors to accommodate the number of channels
    freq_low.resize(num_channels);
    freq_high.resize(num_channels);
    gain_low.resize(num_channels);
    gain_medium.resize(num_channels);
    gain_high.resize(num_channels);

    // Initialize with config default values
    for (int i = 0; i < num_channels; i++) {
      freq_low[i] = p_cfg->freq_low;
      freq_high[i] = p_cfg->freq_high;
      gain_low[i] = p_cfg->gain_low;
      gain_medium[i] = p_cfg->gain_medium;
      gain_high[i] = p_cfg->gain_high;
    }
  }

  /// Applies per-channel 3-band equalization to audio samples
  /// @param data Pointer to audio data buffer
  /// @param len Length of the data buffer in bytes
  void filterSamples(const uint8_t *data, size_t len) {
    if (state == nullptr){
      LOGE("You need to call begin() before using the equalizer");
      return;
    }
    switch (p_cfg->bits_per_sample) {
      case 16: {
        int16_t *p_dataT = (int16_t *)data;
        size_t sample_count = len / sizeof(int16_t);
        for (size_t j = 0; j < sample_count; j += p_cfg->channels) {
          for (int ch = 0; ch < p_cfg->channels; ch++) {
            p_dataT[j + ch] = NumberConverter::fromFloat(sample(ch, NumberConverter::toFloat(p_dataT[j + ch], 16)), 16);
          }
        }
      } break;
      case 24: {
        int24_t *p_dataT = (int24_t *)data;
        size_t sample_count = len / sizeof(int24_t);
        for (size_t j = 0; j < sample_count; j += p_cfg->channels) {
          for (int ch = 0; ch < p_cfg->channels; ch++) {
            p_dataT[j + ch] = NumberConverter::fromFloat(sample(ch, NumberConverter::toFloat(p_dataT[j + ch], 24)), 24);
          }
        }
      } break;
      case 32: {
        int32_t *p_dataT = (int32_t *)data;
        size_t sample_count = len / sizeof(int32_t);
        for (size_t j = 0; j < sample_count; j += p_cfg->channels) {
          for (int ch = 0; ch < p_cfg->channels; ch++) {
            p_dataT[j + ch] = NumberConverter::fromFloat(sample(ch, NumberConverter::toFloat(p_dataT[j + ch], 32)), 32);
          }
        }
      } break;

      default:
        LOGE("Only 16 bits supported: %d", p_cfg->bits_per_sample);
        break;
    }
  }

  /// Processes a single sample through the 3-band equalizer for a specific channel
  /// @param channel The channel number to process
  /// @param sample_val The input sample value
  /// @return The processed sample value with per-channel equalization applied
  float sample(int channel, float sample_val) {
    EQSTATE &es = state[channel];
    
    // Locals
    float l, m, h;  // Low / Mid / High - Sample Values
    
    // Filter #1 (lowpass)
    es.f1p0 += (es.lf * (sample_val - es.f1p0)) + vsa;
    es.f1p1 += (es.lf * (es.f1p0 - es.f1p1));
    es.f1p2 += (es.lf * (es.f1p1 - es.f1p2));
    es.f1p3 += (es.lf * (es.f1p2 - es.f1p3));

    l = es.f1p3;

    // Filter #2 (highpass)
    es.f2p0 += (es.hf * (sample_val - es.f2p0)) + vsa;
    es.f2p1 += (es.hf * (es.f2p0 - es.f2p1));
    es.f2p2 += (es.hf * (es.f2p1 - es.f2p2));
    es.f2p3 += (es.hf * (es.f2p2 - es.f2p3));

    h = es.sdm3 - es.f2p3;
    
    // Calculate midrange (signal - (low + high))
    m = es.sdm3 - (h + l);
    
    // Scale with per-channel gains
    l *= gain_low[channel];
    m *= gain_medium[channel];
    h *= gain_high[channel];

    // Shuffle history buffer
    es.sdm3 = es.sdm2;
    es.sdm2 = es.sdm1;
    es.sdm1 = sample_val;

    // Return result
    return (l + m + h);
  }
};

}  // namespace audio_tools