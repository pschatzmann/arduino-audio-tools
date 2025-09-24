#pragma once
#include "AudioTools/AudioCodecs/AudioCodecsBase.h"
#include "AudioTools/AudioCodecs/HeaderParserMP3.h"

namespace audio_tools {

/**
 * @brief Parses MP3 frames, extracts audio info, and outputs complete frames.
 * The frame duration is determined e.g. for RTSP streaming.
 * @ingroup codecs
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class MP3ParserEncoder : public AudioEncoder {
 public:
  MP3ParserEncoder(int bufferSize = 1024 * 2) { buffer_size = bufferSize; }
  bool begin() override {
    TRACEI();
    mp3.resize(buffer_size);  // 10KB buffer
    mp3.reset();
    return true;
  }

  void end() override {
    TRACEI();
    mp3.flush();
    mp3.reset();
    mp3.resize(0);
  }

  size_t write(const uint8_t* data, size_t len) override {
    LOGI("write: %d", (int)len);
    return mp3.write(data, len);
  }

  void setOutput(Print& out_stream) override {
    TRACEI();
    AudioEncoder::setOutput(out_stream);
    mp3.setOutput(out_stream);
  }

  AudioInfo audioInfo() override {
    AudioInfo info;
    info.sample_rate = mp3.getSampleRate();
    info.channels = mp3.getChannels();
    info.bits_per_sample = 16;
    return info;
  }

  uint32_t frameDurationUs() override { return mp3.getTimePerFrameMs() * 1000; }

  uint16_t samplesPerFrame() override { return mp3.getSamplesPerFrame(); }

  operator bool() override { return true; }

  virtual const char* mime() override { return "audio/mpeg"; }

 protected:
  HeaderParserMP3 mp3;
  int buffer_size = 0;
};

}  // namespace audio_tools