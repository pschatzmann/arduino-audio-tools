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
 */
class IAudioSource {
 public:

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
  virtual RTSPFormat &getFormat() = 0;

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
  // Default no-op so derived classes may optionally override
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
  // Default no-op so derived classes may optionally override
  virtual void stop() {}

};

}  // namespace audio_tools
