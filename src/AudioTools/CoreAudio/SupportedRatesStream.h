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
class SupportedRatesStream : public ResampleStream {
 public:
  SupportedRatesStream() = default;

  SupportedRatesStream(std::vector<int> rates) { setSupportedSampleRates(rates); }

  /// Support for resampling via write.
  SupportedRatesStream(Print &out) { setOutput(out); }
  /// Support for resampling via write.
  SupportedRatesStream(std::vector<int> rates, Print &out) {
    setSupportedSampleRates(rates);
    setOutput(out);
  }

  /// Support for resampling via write. The audio information is copied from
  /// the io
  SupportedRatesStream(AudioOutput &out) {
    setAudioInfo(out.audioInfo());
    setOutput(out);
  }
  /// Support for resampling via write. The audio information is copied from
  /// the io
  SupportedRatesStream(std::vector<int> rates, AudioOutput &out) {
    setSupportedSampleRates(rates);
    setAudioInfo(out.audioInfo());
    setOutput(out);
  }

  /// Support for resampling via write and read.
  SupportedRatesStream(Stream &io) { setStream(io); }
  /// Support for resampling via write and read.
  SupportedRatesStream(std::vector<int> rates, Stream &io) {
    setSupportedSampleRates(rates);
    setStream(io);
  }

  /// Support for resampling via write and read. The audio information is
  /// copied from the io
  SupportedRatesStream(AudioStream &io) {
    setAudioInfo(io.audioInfo());
    setStream(io);
  }
  /// Support for resampling via write and read. The audio information is
  /// copied from the io
  SupportedRatesStream(std::vector<int> rates, AudioStream &io) {
    setSupportedSampleRates(rates);
    setAudioInfo(io.audioInfo());
    setStream(io);
  }

  /// Defines the list of sample rates that the downstream consumer supports
  void setSupportedSampleRates(std::vector<int> rates) {
    supported_rates = rates;
  }

  /// Provides the list of supported sample rates
  std::vector<int> &supportedSampleRates() { return supported_rates; }

  void setAudioInfo(AudioInfo newInfo) override {
    // pick the closest supported rate and resample to it
    setTargetSampleRate(closestSampleRate(newInfo.sample_rate));
    ResampleStream::setAudioInfo(newInfo);
  }

  bool begin(AudioInfo info) override {
    // make sure the target rate is determined before the base class
    // (re)builds the resampler from it
    setTargetSampleRate(closestSampleRate(info.sample_rate));
    return ResampleStream::begin(info);
  }

 protected:
  std::vector<int> supported_rates;

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
