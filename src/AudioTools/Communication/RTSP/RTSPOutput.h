#pragma once

#include "AudioTools/CoreAudio/AudioPlayer.h"
#include "AudioTools/CoreAudio/AudioStreams.h"
#include "RTSPAudioStreamer.h"
#include "RTSPAudioSource.h"
#include "RTSPFormatAudioTools.h"

/**
 * @defgroup rtsp RTSP Streaming
 * @ingroup communications
 * @file RTSPOutput.h
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

namespace audio_tools {

/**
 * @brief We can write PCM data to the RTSPOutput. This is encoded by the
 * indicated encoder (e.g. SBCEncoder) and can be consumed by a RTSPServer.
 * You have to make sure that the codec supports the provided audio format:
 * e.g. GSM support only 8000 samples per second with one channel.
 * Depends on the https://github.com/pschatzmann/Micro-RTSP-Audio/ library
 * @ingroup rtsp
 * @ingroup io
 * @author Phil Schatzmann
 */
template<typename Platform = DefaultRTSPPlatform>
class RTSPOutput : public AudioOutput {
 public:
  using streamer_t = RTSPAudioStreamer<Platform>;
  
  /// Default constructor
  RTSPOutput(RTSPFormatAudioTools &format, AudioEncoder &encoder,
             int buffer_size = 1024 * 2) {
    buffer.resize(buffer_size);
    p_format = &format;
    p_encoder = &encoder;
    p_encoder->setOutput(buffer);
  }

  /// Construcor using RTSPFormatPCM and no encoder
  RTSPOutput(int buffer_size = 1024) {
    buffer.resize(buffer_size);
    p_encoder->setOutput(buffer);
  }

  streamer_t *streamer() { return &rtsp_streamer; }

  bool begin(AudioInfo info) {
    cfg = info;
    return begin();
  }

  bool begin() {
    TRACEI();
    if (p_input == nullptr) {
      LOGE("input is null");
      return false;
    }
    if (p_encoder == nullptr) {
      LOGE("encoder is null");
      return false;
    }
    if (p_format == nullptr) {
      LOGE("format is null");
      return false;
    }
    p_encoder->setAudioInfo(cfg);
    p_encoder->begin();
    p_format->begin(cfg);
    // setup the RTSPAudioStreamer
    rtsp_streamer.setAudioSource(&rtps_source);
    // setup the RTSPSourceFromAudioStream
    rtps_source.setInput(*p_input);
    rtps_source.setFormat(p_format);
    rtps_source.setAudioInfo(cfg);
    rtps_source.start();
    return true;
  }

  void end() { rtps_source.stop(); }

  /** We do not know exactly how much we can write because the encoded audio
   * is using less space. But providing the available buffer should cover
   * the worst case.
   */
  int availableForWrite() {
    return rtps_source.isStarted() ? buffer.available() : 0;
  }

  /**
   * We write PCM data which is encoded on the fly by the indicated encoder.
   * This data is provided by the IAudioSource
   */
  size_t write(const uint8_t *data, size_t len) override {
    TRACED();
    size_t result = p_encoder->write(data, len);
    return result;
  }

  /// @brief Returns true if the server has been started
  operator bool() { return rtps_source.isActive(); }

 protected:
  CopyEncoder copy_encoder;
  RTSPSourceFromAudioStream rtps_source;
  RingBufferStream buffer{0};
  AudioStream *p_input = &buffer;
  AudioEncoder *p_encoder = &copy_encoder;
  RTSPFormatAudioToolsPCM pcm;
  RTSPFormatAudioTools *p_format = &pcm;
  streamer_t rtsp_streamer;
};

// Type alias for default RTSPOutput using WiFi
using DefaultRTSPOutput = RTSPOutput<DefaultRTSPPlatform>;

}  // namespace audio_tools