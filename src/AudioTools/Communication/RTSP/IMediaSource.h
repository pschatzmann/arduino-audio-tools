/*
 * Author: Phil Schatzmann
 *
 * Based on Micro-RTSP library:
 * https://github.com/geeksville/Micro-RTSP
 * https://github.com/Tomp0801/Micro-RTSP-Audio
 *
 */
#pragma once

#include "RTSPFormat.h"

namespace audio_tools {

/**
 * @brief Media Source Interface - Contract for Media Data Providers
 * 
 */
class IMediaSource {
 public:

  /**
   * @brief Get the media format configuration
   * 
   * Returns the RTSPFormat object that describes the media data characteristics
   * including sample rate, bit depth, number of channels, and RTP packaging parameters.
   * If no format has been explicitly set, creates a default 16-bit PCM format
   * at 16kHz mono.
   * 
   * @return Pointer to RTSPFormat describing the media characteristics
   * @note Default format: 16-bit PCM, 16000 Hz, 1 channel
   * @see setFormat(), RTSPFormatPCM
   */
  virtual RTSPFormat &getFormat() = 0;

  /**
   * @brief Read media data into provided buffer
   * 
   * This is the core method that provides media data to the streaming system.
   * Implementation should fill the destination buffer with media samples in the
   * format specified by getFormat(). The method should be non-blocking and return
   * available data immediately.
   * 
   * @param dest Pointer to buffer where media data should be written
   * @param maxBytes Maximum number of bytes that can be written to dest buffer
   * @return Actual number of bytes written to the buffer (0 if no data available)
   * 
   * @note Data format must match the RTSPFormat returned by getFormat()
   * @note Should return 0 when no media data is available (not block)
   * @note Called periodically by the streaming system at the rate specified by format
   */
  virtual int readBytes(void *dest, int maxSamples) = 0;

  /**
   * @brief Initialize media source for streaming
   * 
   * Called when streaming is about to begin. Implementations should use this
   * to initialize hardware, allocate buffers, start media capture, or perform
   * any other setup required for media data generation.
   * 
   * @note Called by RTSPMediaStreamer.start()
   * @note Default implementation does nothing (suitable for sources that need no setup)
   */
  // Default no-op so derived classes may optionally override
  virtual void start() {};

  /**
   * @brief Cleanup media source after streaming
   * 
   * Called when streaming has ended. Implementations should use this to
   * release hardware resources, deallocate buffers, stop media capture,
   * or perform cleanup operations.
   * 
   * @note Called by RTSPMediaStreamer.stop()
   * @note Default implementation does nothing (suitable for sources that need no cleanup)
   */
  // Default no-op so derived classes may optionally override
  virtual void stop() {}

  /**
   * @brief Size of the next queued, ready-to-send fragment, for sources
   * that provide already RTP-payload-ready, self-delimited fragments (e.g.
   * RFC 2435 JPEG) instead of a continuous byte stream. For these sources,
   * readBytes() must always be called with exactly packetSize() as
   * maxBytes, so each call retrieves exactly one complete fragment instead
   * of the transport having to guess where one RTP payload ends and the
   * next begins.
   *
   * Call it once before each readBytes() to size that call, and again
   * right after to tell whether the fragment just read was the last one
   * currently available - a 0 result means the caller should set the RTP
   * marker bit on the fragment it just sent (and may advance the timestamp
   * for the next frame).
   *
   * @return -1 if this source is not packetized (use the plain readBytes()
   * stream instead), 0 if packetized but nothing is queued right now,
   * otherwise the size in bytes of the next fragment
   */
  virtual int packetSize() { return -1; }
};

}  // namespace audio_tools
