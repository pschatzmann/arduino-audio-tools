/*
 * Author: Phil Schatzmann
 *
 * Based on Micro-RTSP library:
 * https://github.com/geeksville/Micro-RTSP
 * https://github.com/Tomp0801/Micro-RTSP-Audio
 *
 */
#pragma once

#include "AudioTools/CoreAudio/AudioPlayer.h"
#include "AudioTools/CoreAudio/AudioStreams.h"
#include "RTSPMediaSource.h"
#include "RTSPMediaStreamer.h"
#include "RTSPFormat.h"
#include "RTSPVideoEncoder.h"

namespace audio_tools {

/**
 * @brief RTSPOutput - Audio Output Stream for RTSP Streaming
 *
 * Accepts PCM audio data, encodes it using the specified encoder, and makes it
 * available for RTSP streaming via an integrated RTSPMediaStreamer.
 *
 * Data flow: PCM input → encoder → internal queue → RTSP source → RTP packets
 *
 * @tparam Platform Platform-specific implementation (RTSPPlatformWiFi, etc.)
 * @note Ensure codec compatibility with your audio format
 * @ingroup rtsp
 * @ingroup io
 * @author Phil Schatzmann
 */
template <typename Platform>
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
  RTSPOutput(RTSPFormat &format, AudioEncoder &encoder) {
    setFormat(format);
    rtsp_streamer.setMediaSource(&rtsp_source);
    p_encoder = &encoder;
  }

  /**
   * @brief Construct RTSPOutput for video streaming via any RTSPVideoEncoder
   * (JPEGRtpEncoder, H264RtpEncoder, ...)
   *
   * A packetized video encoder performs its own RTP payload construction
   * (fragmentation, payload-format headers), so this overload wires the
   * streamer directly to the encoder instead of going through the generic
   * byte-stream (RTSPMediaSource/DynamicMemoryStream) pipeline used for
   * PCM/codec audio.
   *
   * @param format Format handler providing SDP configuration (e.g.
   * RTSPFormatMJPEG, RTSPFormatH264)
   * @param encoder Packetized video encoder matching the format
   */
  RTSPOutput(RTSPFormat &format, RTSPVideoEncoder &encoder) {
    setFormat(format);
    p_encoder = &encoder;
    p_video_encoder = &encoder;
    encoder.setFormat(format);
  }

  void setFormat(RTSPFormat &format) {
    TRACEI();
    p_format = &format;
    rtsp_source.setFormat(format);
  }

  /**
   * @brief Construct RTSPOutput with default PCM format (no encoding)
   *
   * @note Uses CopyEncoder (pass-through) for uncompressed streaming
   */
  RTSPOutput() = default;

  /**
   * @brief Get access to the underlying RTSP streamer
   *
   * @return Pointer to the RTSPMediaStreamer instance
   */
  RTSPMediaStreamer<Platform> &streamer() { return rtsp_streamer; }

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

    if (p_video_encoder != nullptr) {
      // Packetized (video) path: the encoder owns fragmentation, framing
      // and the RTP marker bit; the streamer only forwards its already
      // ready-to-send fragments.
      p_encoder->begin();
      rtsp_streamer.setMediaSource(p_video_encoder);
      return true;
    }

    // setup the RTSPMediaStreamer
    cfg.logInfo();

    p_encoder->setOutput(memory_stream);
    p_encoder->setAudioInfo(cfg);
    p_encoder->begin();
    // setup the RTSPSourceFromAudioStream
    rtsp_source.setInput(memory_stream);
    rtsp_source.setFormat(*p_format);
    rtsp_source.setAudioInfo(cfg);
    rtsp_source.start();

    memory_stream.setConsumeOnRead(true);
    memory_stream.begin();

    // Initialize format with audio info (PCM or codec-specific)
    p_format->begin(cfg);

    return true;
  }

  /**
   * @brief Stop RTSP streaming and cleanup resources
   */
  void end() {
    if (p_video_encoder != nullptr) {
      p_video_encoder->stop();
      return;
    }
    rtsp_source.stop();
    memory_stream.end();
  }

  /**
   * @brief Get available space for writing audio data
   *
   * @return Number of bytes available for writing, 0 if not started
   */
  int availableForWrite() {
    if (p_video_encoder != nullptr) {
      // Video frames are pushed on capture, not rate-limited by an
      // intermediate buffer; report "ready" whenever the encoder is started.
      return (bool)(*p_video_encoder) ? 0x7FFFFFFF : 0;
    }
    return rtsp_source.isStarted() ? memory_stream.availableForWrite() : 0;
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
  operator bool() {
    if (p_video_encoder != nullptr) return (bool)(*p_video_encoder);
    return rtsp_source.isActive() && memory_stream.availableForWrite() > 0;
  }

 protected:
  // Core Components
  CopyEncoder copy_encoder;           ///< Pass-through encoder for PCM mode
  RTSPMediaSource rtsp_source;        ///< Provides encoded audio to streamer
  DynamicMemoryStream memory_stream{false, 1024, 10};  ///< Memory stream for internal buffer
  AudioEncoder *p_encoder =
      &copy_encoder;            ///< Active encoder (PCM or codec-specific)
  RTSPVideoEncoder *p_video_encoder =
      nullptr;                  ///< Set for a packetized (video) constructor
  RTSPFormatPCM pcm;            ///< Default PCM format handler (merged class)
  RTSPFormat *p_format = &pcm;  ///< Active format handler
  RTSPMediaStreamer<Platform>
      rtsp_streamer;  ///< Handles RTP packet transmission
};

}  // namespace audio_tools