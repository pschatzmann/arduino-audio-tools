#pragma once

#include "AudioTools/CoreAudio/AudioStreams.h"
#include "IAudioSource.h"
#include "RTSPFormat.h"
#include "platform.h"

/**
 * @defgroup rtsp RTSP Streaming
 * @ingroup communications
 * @file RTSPAudioSource.h
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

namespace audio_tools {

/**
 * @brief Unified RTSP Audio Source - Works with both Stream and AudioStream
 *
 * This class can adapt any Arduino Stream or AudioTools AudioStream into an
 * IAudioSource for RTSP streaming. It automatically detects AudioStream
 * capabilities when available and falls back to manual configuration for
 * generic Streams.
 *
 * @section usage Usage Examples
 * @code
 * // With AudioStream (automatic audio info detection)
 * I2SStream i2s;
 * RTSPSource source(i2s);
 *
 * // With generic Stream (manual audio info required)
 * WiFiClient client;
 * AudioInfo info(44100, 2, 16);
 * RTSPSource source(client, info);
 *
 * // With custom format
 * RTSPFormatPCM customFormat(info, 1024);
 * RTSPSource source(stream, customFormat);
 * @endcode
 *
 * @ingroup rtsp
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class RTSPSource : public IAudioSource {
 public:
  RTSPSource() = default;

  /**
   * @brief Construct from AudioStream with automatic audio info detection
   * @param stream AudioStream that provides audio configuration automatically
   */
  RTSPSource(AudioStream &stream) { setInput(stream); }

  /**
   * @brief Construct from generic Stream with explicit audio info
   * @param stream Any Arduino Stream object
   * @param info Audio configuration (sample rate, channels, bit depth)
   */
  RTSPSource(Stream &stream, AudioInfo info) { setInput(stream, info); }

  /**
   * @brief Construct with custom format
   * @param stream Input stream (Stream or AudioStream)
   * @param format Custom RTSP format implementation
   */
  RTSPSource(Stream &stream, RTSPFormat &format) {
    setInput(stream);
    p_format = &format;
  }

  /**
   * @brief Set input from AudioStream (with auto-detection)
   * @param stream AudioStream that can provide its own audio configuration
   */
  void setInput(AudioStream &stream) {
    p_stream = &stream;
    p_audiostream = &stream;
  }

  /**
   * @brief Set input from generic Stream with explicit audio info
   * @param stream Generic Arduino Stream
   * @param info Audio configuration parameters
   */
  void setInput(Stream &stream, AudioInfo info) {
    p_stream = &stream;
    p_audiostream = nullptr;
    setAudioInfo(info);
  }

  /**
   * @brief Set input from generic Stream (audio info must be set separately)
   * @param stream Generic Arduino Stream
   */
  void setInput(Stream &stream) {
    p_stream = &stream;
    p_audiostream = nullptr;
  }

  /**
   * @brief Set audio configuration manually
   *
   * This is required when using generic Stream objects, but optional
   * for AudioStream objects which can provide their own configuration.
   *
   * @param info Audio configuration (sample rate, channels, bit depth)
   */
  virtual void setAudioInfo(AudioInfo info) {
    TRACEI();
    default_format.begin(info);
    if (p_audiostream) {
      p_audiostream->setAudioInfo(info);
    }
  }

  /**
   * @brief Read audio data for RTSP streaming
   * @param dest Buffer to receive audio data
   * @param byteCount Number of bytes to read
   * @return Actual number of bytes read
   */
  virtual int readBytes(void *dest, int byteCount) override {
    time_of_last_read = millis();

    int result = 0;
    LOGD("readDataTo: %d", byteCount);
    if (started && p_stream) {
      result = p_stream->readBytes((uint8_t *)dest, byteCount);
    }
    return result;
  }

  /**
   * @brief Get the audio format configuration
   * @return Pointer to RTSPFormat describing the audio characteristics
   */
  virtual RTSPFormat *getFormat() override { return p_format; }

  /**
   * @brief Start the audio source
   *
   * For AudioStreams, calls begin() to initialize the stream.
   * For generic Streams, just sets the active flag.
   */
  virtual void start() override {
    TRACEI();
    IAudioSource::start();
    if (p_audiostream) {
      p_audiostream->begin();
    }
    started = true;
  }

  /**
   * @brief Stop the audio source
   *
   * For AudioStreams, calls end() to cleanup the stream.
   * For generic Streams, just clears the active flag.
   */
  virtual void stop() override {
    TRACEI();
    IAudioSource::stop();
    started = false;
    if (p_audiostream) {
      p_audiostream->end();
    }
  }

  /// Defines the fragment size
  void setFragmentSize(int fragmentSize) {
    getFormat()->setFragmentSize(fragmentSize);
  }

  /// Defines the timer period
  void setTimerPeriod(int period) { getFormat()->setTimerPeriod(period); }

  /**
   * @brief Check if source is actively being read (AudioStream only)
   * @return true if recent read activity detected, false otherwise
   * @note Only available for AudioStream sources, always returns true for
   * generic Streams
   */
  bool isActive() { return millis() - time_of_last_read < 100; }

  /// Returns true after start() has been called.
  bool isStarted() { return started; }

 protected:
  Stream *p_stream = nullptr;
  AudioStream *p_audiostream = nullptr;
  uint32_t time_of_last_read = 0;
  bool started = false;
  RTSPFormatPCM default_format;  // Used for AudioStream sources
  RTSPFormat *p_format = &default_format;
  ;
};

// Backward compatibility aliases
using RTSPSourceFromAudioStream = RTSPSource;

// Backward compatibility alias
using RTSPSourceStream = RTSPSource;

}  // namespace audio_tools