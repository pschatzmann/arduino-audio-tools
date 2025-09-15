/*
 * Author: Thomas Pfitzinger
 * github: https://github.com/Tomp0801/Micro-RTSP-Audio
 *
 * Based on Micro-RTSP library for video streaming by Kevin Hester:
 *
 * https://github.com/geeksville/Micro-RTSP
 *
 * Copyright 2018 S. Kevin Hester-Chow, kevinh@geeksville.com (MIT License)
 */

#pragma once

#include "RTSPFormat.h"

namespace audio_tools {

/**
 * @brief Audio Source Interface - Contract for Audio Data Providers
 * 
 * The IAudioSource interface defines the contract for classes that provide audio data
 * to the RTSP streaming system. This interface abstracts the source of audio data,
 * allowing the streaming system to work with various audio inputs such as:
 * 
 * - Microphone input (I2S, analog)
 * - Audio files (WAV, MP3, etc.)
 * - Synthesized audio (tone generators, music synthesis)
 * - Network audio streams
 * - Audio processing pipelines
 * 
 * @section implementation Implementation Requirements
 * Implementing classes must:
 * - Provide audio data via readBytes() method
 * - Define audio format parameters (sample rate, channels, bit depth)
 * - Handle start/stop lifecycle events
 * - Manage internal buffering and timing
 * 
 * @section usage Usage Example
 * @code
 * class MyAudioSource : public IAudioSource {
 * public:
 *   MyAudioSource() {
 *     // Set up 16-bit PCM, 44100 Hz, stereo
 *     setFormat(new RTSPFormatPCM(44100, 2, 16));
 *   }
 *   
 *   int readBytes(void* dest, int maxBytes) override {
 *     // Fill dest buffer with audio data
 *     // Return actual bytes written
 *   }
 *   
 *   void start() override {
 *     // Initialize audio hardware/resources
 *   }
 *   
 *   void stop() override {
 *     // Clean up audio hardware/resources  
 *   }
 * };
 * @endcode
 * 
 * @note The default format is 16-bit PCM, 16kHz, mono if not specified
 * @author Thomas Pfitzinger
 * @version 0.1.1
 */
class IAudioSource {
 public:
  /**
   * @brief Default constructor
   */
  IAudioSource() = default;

  /**
   * @brief Get the audio format configuration
   * 
   * Returns the RTSPFormat object that describes the audio data characteristics
   * including sample rate, bit depth, number of channels, and RTP packaging parameters.
   * If no format has been explicitly set, creates a default 16-bit PCM format
   * at 16kHz mono.
   * 
   * @return Pointer to RTSPFormat describing the audio characteristics
   * @note Default format: 16-bit PCM, 16000 Hz, 1 channel
   * @see setFormat(), RTSPFormatPCM
   */
  virtual RTSPFormat *getFormat() {
    if (p_fmt == nullptr) {
      // we assume 2 byte PCM info with 16000 samples per second on 1 channel
      static RTSPFormatPCM fmt;
      p_fmt = &fmt;
    }
    return p_fmt;
  }

  /**
   * @brief Set the audio format configuration
   * 
   * Configures the audio format that this source will provide. The format
   * object specifies sample rate, bit depth, channels, and RTP packaging
   * parameters that the streaming system needs.
   * 
   * @param fmt Pointer to RTSPFormat object defining audio characteristics.
   *            The source takes ownership and will use this for getFormat() calls.
   * @see getFormat(), RTSPFormatPCM
   */
  void setFormat(RTSPFormat &fmt) { p_fmt = &fmt; }

  /**
   * @brief Read audio data into provided buffer
   * 
   * This is the core method that provides audio data to the streaming system.
   * Implementation should fill the destination buffer with audio samples in the
   * format specified by getFormat(). The method should be non-blocking and return
   * available data immediately.
   * 
   * @param dest Pointer to buffer where audio data should be written
   * @param maxBytes Maximum number of bytes that can be written to dest buffer
   * @return Actual number of bytes written to the buffer (0 if no data available)
   * 
   * @note Data format must match the RTSPFormat returned by getFormat()
   * @note Should return 0 when no audio data is available (not block)
   * @note Called periodically by the streaming system at the rate specified by format
   */
  virtual int readBytes(void *dest, int maxSamples) = 0;

  /**
   * @brief Initialize audio source for streaming
   * 
   * Called when streaming is about to begin. Implementations should use this
   * to initialize hardware, allocate buffers, start audio capture, or perform
   * any other setup required for audio data generation.
   * 
   * @note Called by RTSPAudioStreamer.start()
   * @note Default implementation does nothing (suitable for sources that need no setup)
   */
  virtual void start() {};

  /**
   * @brief Cleanup audio source after streaming
   * 
   * Called when streaming has ended. Implementations should use this to
   * release hardware resources, deallocate buffers, stop audio capture,
   * or perform cleanup operations.
   * 
   * @note Called by RTSPAudioStreamer.stop()
   * @note Default implementation does nothing (suitable for sources that need no cleanup)
   */
  virtual void stop() {};

 protected:
  RTSPFormat *p_fmt = nullptr;
};

}  // namespace audio_tools
