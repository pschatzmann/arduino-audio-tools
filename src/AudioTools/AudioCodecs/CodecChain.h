#pragma once
#include "AudioTools/AudioCodecs/AudioCodecsBase.h"
#include "AudioTools/AudioCodecs/AudioEncoded.h"

namespace audio_tools {

/**
 * @brief CodecChain - allows to chain multiple decoders and encoders together
 * @ingroup codecs
 * @ingroup decoder
 * @ingroup encoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class CodecChain : public AudioDecoder, AudioEncoder {
 public:
  CodecChain() = default;
  CodecChain(AudioDecoder &decoder) { addDecoder(decoder); }
  CodecChain(AudioEncoder &encoder) { addEncoder(encoder); }

  /// Adds a decoder to the chain
  void addDecoder(AudioDecoder &decoder) {
    EncodedAudioStream stream;
    stream.setDecoder(&decoder);
    streams.push_back(stream);
    if (streams.size() > 1) {
      streams[streams.size() - 2].setOutput(streams[streams.size() - 1]);
    }
  }

  /// Adds an encoder to the chain
  void addEncoder(AudioEncoder &encoder) {
    EncodedAudioStream stream;
    stream.setEncoder(&encoder);
    streams.push_back(stream);
    if (streams.size() > 1) {
      streams[streams.size() - 2].setOutput(streams[streams.size() - 1]);
    }
  }

  void setOutput(Print &out_stream) override {
    p_print = &out_stream;
    if (streams.size() > 0) streams[streams.size() - 1].setOutput(out_stream);
  }

  void setOutput(AudioStream &out_stream) override {
    p_print = &out_stream;
    if (streams.size() > 0) streams[streams.size() - 1].setOutput(out_stream);
  }

  void setOutput(AudioOutput &out_stream) override {
    p_print = &out_stream;
    if (streams.size() > 0) streams[streams.size() - 1].setOutput(out_stream);
  }

  void setAudioInfo(AudioInfo from) override {
    AudioDecoder::setAudioInfo(from);
    for (auto &stream : streams) {
      stream.setAudioInfo(from);
    }
  }

  void addNotifyAudioChange(AudioInfoSupport &bi) override {
    for (auto &stream : streams) {
      stream.addNotifyAudioChange(bi);
    }
  }

  size_t write(const uint8_t *data, size_t len) override {
    if (streams.size() == 0) return 0;
    return streams[0].write(data, len);
  }

  operator bool() { return is_active; }

  bool begin() {
    is_active = true;
    for (auto &stream : streams) {
      stream.begin();
    }
    return true;
  }

  void end() override {
    is_active = false;
    for (auto &stream : streams) {
      stream.end();
    }
  };

  /// Returns nullptr
  const char *mime() { return nullptr; }

 protected:
  Vector<EncodedAudioStream> streams;
  bool is_active = false;
};

}  // namespace audio_tools