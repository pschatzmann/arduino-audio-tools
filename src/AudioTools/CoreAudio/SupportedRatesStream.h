#pragma once

#include <vector>

#include "AudioTools/CoreAudio/ResampleStream.h"

namespace audio_tools {

/**
 * @brief Resampling stream that is restricted to a fixed list of supported
 * sample rates (e.g. the rates a DAC or codec chip can actually run at). On
 * setAudioInfo() the closest supported rate is selected, the resampler is
 * reconfigured to convert to it, and the corrected AudioInfo (with the
 * supported sample rate) is forwarded to any subscribers/output.
 * @author Phil Schatzmann
 * @ingroup transform
 * @copyright GPLv3
 */
class SupportedRatesStream : public AudioStream {
 public:
  SupportedRatesStream() = default;

  /// Support for resampling via write.
  SupportedRatesStream(Print &out) { setOutput(out); }

  /// Support for resampling via write. The audio information is copied from
  /// the io
  SupportedRatesStream(AudioOutput &out) {
    setAudioInfo(out.audioInfo());
    setOutput(out);
  }

  /// Support for resampling via write and read.
  SupportedRatesStream(Stream &io) { setStream(io); }

  /// Support for resampling via write and read. The audio information is
  /// copied from the io
  SupportedRatesStream(AudioStream &io) {
    setAudioInfo(io.audioInfo());
    setStream(io);
  }

  /// Defines the list of sample rates that the downstream consumer supports
  void setSupportedSampleRates(std::vector<int> rates) {
    supported_rates = rates;
  }

  /// Provides the list of supported sample rates
  std::vector<int> &supportedSampleRates() { return supported_rates; }

  /// Defines/Changes the output target
  void setOutput(Print &out) { resampler.setOutput(out); }
  /// Defines/Changes the output target and registers for audio change
  /// notifications
  void setOutput(AudioOutput &out) { resampler.setOutput(out); }
  /// Defines/Changes the input & output
  void setStream(Stream &io) {
    p_source = &io;
    resampler.setStream(io);
  }
  /// Defines/Changes the input & output and registers for audio change
  /// notifications
  void setStream(AudioStream &io) {
    p_source = &io;
    resampler.setStream(io);
  }

  bool begin() override { return begin(audioInfo()); }

  bool begin(AudioInfo newInfo) {
    setAudioInfo(newInfo);
    return resampler.begin(newInfo);
  }

  void end() override { resampler.end(); }

  void setAudioInfo(AudioInfo newInfo) override {
    info = newInfo;
    // pick the closest supported rate and resample to it
    int target = closestSampleRate(newInfo.sample_rate);
    // rate already supported: readBytes()/available() can bypass the
    // resampler and its extra buffering, reading straight from the source
    needs_resample = (target != newInfo.sample_rate);
    resampler.setTargetSampleRate(target);
    resampler.setAudioInfo(newInfo);
    notifyAudioChange(audioInfoOut());
  }

  AudioInfo audioInfoOut() override { return resampler.audioInfoOut(); }

  size_t write(const uint8_t *data, size_t len) override {
    return resampler.write(data, len);
  }

  size_t readBytes(uint8_t *data, size_t len) override {
    if (!needs_resample && p_source != nullptr) {
      return p_source->readBytes(data, len);
    }
    return resampler.readBytes(data, len);
  }

  // available() must take the same bypass path as readBytes(): the
  // resampler's available() pulls bytes from the source into its own
  // queue as a side effect, which would silently steal data from a
  // readBytes() call that bypasses the resampler.
  int available() override {
    if (!needs_resample && p_source != nullptr) {
      return p_source->available();
    }
    return resampler.available();
  }

  int availableForWrite() override { return resampler.availableForWrite(); }

  void flush() override { resampler.flush(); }

 protected:
  ResampleStream resampler;
  Stream *p_source = nullptr;
  std::vector<int> supported_rates;
  bool needs_resample = true;

  /// Determines the supported sample rate closest to the requested rate
  int closestSampleRate(int rate) {
    if (supported_rates.empty()) return rate;
    int result = supported_rates[0];
    long min_diff = labs((long)rate - (long)result);
    for (int candidate : supported_rates) {
      long diff = labs((long)rate - (long)candidate);
      if (diff < min_diff) {
        min_diff = diff;
        result = candidate;
      }
    }
    return result;
  }
};

}  // namespace audio_tools
