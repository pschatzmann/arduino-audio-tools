#pragma once
#include "AudioTools/AudioOutput.h"
#include "AudioTools/Buffers.h"
#include "stdint.h"

/**
 * @defgroup video Video
 * @ingroup main
 * @brief Video playback
 */

namespace audio_tools {

/**
 * @brief Abstract class for video playback. This class is used to assemble a
 * complete video frame in memory
 * @ingroup video
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class VideoOutput {
 public:
  virtual void beginFrame(size_t size) = 0;
  virtual size_t write(const uint8_t *data, size_t len) = 0;
  virtual uint32_t endFrame() = 0;
};

/**
 * @brief Logic to Synchronize video and audio output: This is the minimum
 * implementatin which actually does not synchronize, but directly processes the
 * data. No additinal memory is used! Provide your own optimized platform
 * specific implementation
 * @ingroup video
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class VideoAudioSync {
 public:
  /// Process the audio data
  virtual void writeAudio(Print *out, uint8_t *data, size_t size) {
    out->write(data, size);
  }

  /// Adds a delay after playing a frame to process with the correct frame rate.
  /// If the playing is too slow we return the mod to select the frames
  virtual void delayVideoFrame(int32_t microsecondsPerFrame,
                               uint32_t time_used_ms) {
    uint32_t delay_ms = microsecondsPerFrame / 1000;
    delay(delay_ms);
  }
};

/**
 * @brief Logic to Synchronize video and audio output: we use a buffer to store
 * the audio and instead of delaying the frames with delay() we play audio. The
 * bufferSize defines the audio buffer in bytes. The correctionMs is used to
 * slow down or speed up the playback of the video to prevent any audio buffer
 * underflows.
 * @ingroup video
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class VideoAudioBufferedSync : public VideoAudioSync {
 public:
  VideoAudioBufferedSync(int bufferSize, int correctionMs) {
    ring_buffer.resize(bufferSize);
    correction_ms = correctionMs;
  }

  /// Process the audio data
  void writeAudio(Print *out, uint8_t *data, size_t size) {
    p_out = out;
    if (ring_buffer.availableForWrite() < size) {
      int bytes_to_play = size - ring_buffer.availableForWrite();
      uint8_t audio[bytes_to_play];
      ring_buffer.readArray(audio, bytes_to_play);
      p_out->write(audio, bytes_to_play);
    }
    size_t written = ring_buffer.writeArray(data, size);
    assert(written = size);
  }

  /// Adds a delay after playing a frame to process with the correct frame rate.
  /// If the playing is too slow we return the mod to select the frames
  void delayVideoFrame(int32_t microsecondsPerFrame, uint32_t time_used_ms) {
    uint32_t delay_ms = microsecondsPerFrame / 1000;
    uint64_t timeout = millis() + delay_ms + correction_ms;
    uint8_t audio[8];
    // output audio
    while (millis() < timeout) {
      ring_buffer.readArray(audio, 8);
      p_out->write(audio, 8);
    }
  }

 protected:
  RingBuffer<uint8_t> ring_buffer{0};
  Print *p_out = nullptr;
  int correction_ms = 0;
};

}  // namespace audio_tools
