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
 * @brief RTSPOutput - Audio Output Stream for RTSP Streaming
 * 
 * Accepts PCM audio data, encodes it using the specified encoder, and makes it 
 * available for RTSP streaming via an integrated RTSPAudioStreamer.
 * 
 * Data flow: PCM input → encoder → internal queue → RTSP source → RTP packets
 * 
 * @tparam Platform Platform-specific implementation (RTSPPlatformWiFi, etc.)
 * @note Ensure codec compatibility with your audio format
 * @ingroup rtsp
 * @ingroup io
 * @author Phil Schatzmann
 */
template<typename Platform>
class RTSPOutput : public AudioOutput {
 public:  
  /**
   * @brief Construct RTSPOutput with specific encoder and format
   * 
   * @param format Format handler providing SDP configuration and timing
   * @param encoder Audio encoder for PCM compression
   * @param bufferSize Internal buffer size in bytes (default: 2KB)
   * @note Encoder and format must remain valid for RTSPOutput lifetime
   */
  RTSPOutput(RTSPFormatAudioTools &format, AudioEncoder &encoder,
             int bufferSize = 1024 * 2) {
    buffer_size = bufferSize;
    p_format = &format;
    p_encoder = &encoder;
  }

  /**
   * @brief Construct RTSPOutput with default PCM format (no encoding)
   * 
   * @param bufferSize Internal buffer size in bytes (default: 2KB)
   * @note Uses CopyEncoder (pass-through) for uncompressed streaming
   */
  RTSPOutput(int bufferSize = 1024 * 2) {
    buffer_size = bufferSize;
  }

  /**
   * @brief Get access to the underlying RTSP streamer
   * 
   * @return Pointer to the RTSPAudioStreamer instance
   */
  RTSPAudioStreamer<Platform> *streamer() { return &rtsp_streamer; }

  /**
   * @brief Initialize RTSPOutput with specific audio configuration
   * 
   * @param info Audio configuration (sample rate, channels, bits per sample)
   * @return true if initialization successful, false otherwise
   */
  bool begin(AudioInfo info) {
    cfg = info;
    return begin();
  }

  /**
   * @brief Initialize RTSPOutput with current audio configuration
   * 
   * @return true if initialization successful, false otherwise
   */

  bool begin() {
    TRACEI();
    if (p_encoder == nullptr) {
      LOGE("encoder is null");
      return false;
    }
    if (p_format == nullptr) {
      LOGE("format is null");
      return false;
    }
    if (buffer_size > 0)queue_stream.resize(buffer_size);
    p_encoder->setOutput(queue_stream);
    p_encoder->setAudioInfo(cfg);
    p_encoder->begin();
    p_format->begin(cfg);
    // setup the RTSPAudioStreamer
    rtsp_streamer.setAudioSource(&rtsp_source);
    // setup the RTSPSourceFromAudioStream
    rtsp_source.setInput(queue_stream);
    rtsp_source.setFormat(p_format);
    rtsp_source.setAudioInfo(cfg);
    rtsp_source.start();
    return true;
  }

  /**
   * @brief Stop RTSP streaming and cleanup resources
   */
  void end() { rtsp_source.stop(); }


  /** 
   * @brief Get available space for writing audio data
   * 
   * @return Number of bytes available for writing, 0 if not started
   */
  int availableForWrite() {
    return rtsp_source.isStarted() ? queue_stream.available() : 0;
  }

  /**
   * @brief Write PCM audio data for encoding and streaming
   * 
   * @param data Pointer to PCM audio data buffer
   * @param len Number of bytes to write
   * @return Number of bytes actually written and processed
   */
  size_t write(const uint8_t *data, size_t len) override {
    TRACED();
    size_t result = p_encoder->write(data, len);
    return result;
  }

  /**
   * @brief Check if RTSP streaming is active
   * 
   * @return true if the RTSP source is active and ready for streaming
   */
  operator bool() { return rtsp_source.isActive(); }

  /**
   * @brief Set custom buffer for internal audio queue
   * 
   * @param buf Reference to custom buffer implementation
   */
  void setBuffer(BaseBuffer<uint8_t> &buf) {
    queue_stream.setBuffer(buf);
  }

 protected:
  // Core Components
  CopyEncoder copy_encoder;                           ///< Pass-through encoder for PCM mode
  RTSPAudioSource rtsp_source;                        ///< Provides encoded audio to streamer
  RingBuffer<uint8_t> ring_buffer{0};                          ///< Default circular buffer for audio queue
  QueueStream<uint8_t> queue_stream{ring_buffer};     ///< Stream interface for internal buffer
  
  // Configuration
  AudioEncoder *p_encoder = &copy_encoder;            ///< Active encoder (PCM or codec-specific)
  RTSPFormatAudioToolsPCM pcm;                        ///< Default PCM format handler
  RTSPFormatAudioTools *p_format = &pcm;             ///< Active format handler
  RTSPAudioStreamer<Platform> rtsp_streamer;         ///< Handles RTP packet transmission
  int buffer_size = 1024 * 2;                        ///< Internal buffer size in bytes
};

}  // namespace audio_tools