#pragma once
#include "AudioTools/AudioCodecs/AudioCodecsBase.h"
#include "AudioTools/AudioCodecs/HeaderParserMP3.h"

namespace audio_tools {

/***
 * @brief Parses MP3 frames, extracts audio info, and outputs complete frames.
 * The frame duration is determined e.g. for RTSP streaming.
 * @ingroup codecs
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class MP3ParserDecoder : public AudioDecoder {
 public:
  bool begin() override {
    mp3.reset();
    return true;
  }

  void end() override {
    mp3.flush();
    mp3.reset();
  }

  size_t write(const uint8_t* data, size_t len) override {
    return mp3.write(data, len);
  }

  void setOutput(Print& out_stream) override {
    AudioDecoder::setOutput(out_stream);
    mp3.setOutput(out_stream);
  }

  uint32_t frameDurationUs() override { return mp3.getTimePerFrameMs() * 1000; }

  AudioInfo audioInfo() override {
    AudioInfo info;
    info.sample_rate = mp3.getSampleRate();
    info.channels = mp3.getChannels();
    info.bits_per_sample = 16;
    return info;
  }

 protected:
  HeaderParserMP3 mp3;
};

/***
 * @brief Parses MP3 frames, extracts audio info, and outputs complete frames.
 * The frame duration is determined e.g. for RTSP streaming.
 * @ingroup codecs
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class MP3ParserEncoder : public AudioEncoder {
 public:
  bool begin() override {
    mp3.reset();
    return true;
  }

  void end() override {
    mp3.flush();
    mp3.reset();
  }

  size_t write(const uint8_t* data, size_t len) override {
    return mp3.write(data, len);
  }

  void setOutput(Print& out_stream) override {
    AudioEncoder::setOutput(out_stream);
    mp3.setOutput(out_stream);
  }

  uint32_t frameDurationUs() override { return mp3.getTimePerFrameMs() * 1000; }

  AudioInfo audioInfo() override {
    AudioInfo info;
    info.sample_rate = mp3.getSampleRate();
    info.channels = mp3.getChannels();
    info.bits_per_sample = 16;
    return info;
  }

 protected:
  HeaderParserMP3 mp3;
};

}  // namespace audio_tools