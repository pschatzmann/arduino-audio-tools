#pragma once
#include "AudioTools/AudioOutput.h"
#include "stdint.h"

namespace audio_tools {

/**
 * Abstract class for video playback. This class is used to assemble a complete
 * video frame in memory
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class VideoOutput : public AudioOutput {
public:
  virtual void beginFrame(size_t size) = 0;
  virtual size_t write(const uint8_t *data, size_t byteCount) = 0;
  virtual uint32_t endFrame() = 0;
};

/**
 * Logic to Synchronize video and audio output: This is the minimum
 * implementatin which actually does not synchronize, but directly processes the
 * data. Provide your own optimized platform specific implementation
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class VideoAudioSync {
public:
  /// Process the audio data
  void writeAudio(Print *out, uint8_t *data, size_t size) {
    out->write(data, size);
  }

  void delayVideoFrame(int32_t microsecondsPerFrame, uint32_t time_used_ms) {
    int32_t delay_ms = microsecondsPerFrame / 1000 - time_used_ms;
    delay(delay_ms);
  }
};

} // namespace audio_tools
