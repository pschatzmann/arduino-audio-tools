#pragma once

#include "AudioConfig.h"
#include "AudioTools/CoreAudio/AudioOutput.h"
#include "AudioTools/CoreAudio/AudioStreams.h"
#include "AudioTools/CoreAudio/Buffers.h"
#include "AudioOutput.h"
#include "SoundData.h"

namespace audio_tools {



/**
 * @brief ESP8266Audio AudioOutput class which stores the data in a temporary
 * buffer. The buffer can be consumed e.g. by a callback function by calling
 * read();
 * Dependencies: ESP8266Audio Library
 * Dependencies: ESP32-A2DP Library
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AudioOutputWithCallback : public ::AudioOutput, public BufferedStream {
 public:
  // Default constructor
  AudioOutputWithCallback(int bufferSize, int bufferCount)
      : BufferedStream(bufferSize) {
    callback_buffer_ptr = new NBuffer<Frame>(bufferSize, bufferCount);
  }

  virtual ~AudioOutputWithCallback() { delete callback_buffer_ptr; }

  /// Activates the output
  virtual bool begin() {
    active = true;
    return true;
  }

  /// puts the sample into a buffer
  virtual bool ConsumeSample(int16_t sample[2]) {
    Frame c;
    c.channel1 = sample[0];
    c.channel2 = sample[1];
    return callback_buffer_ptr->write(c);
  };

  /// stops the processing
  virtual bool stop() {
    active = false;
    return true;
  };

  /// Provides the data from the internal buffer to the callback
  size_t read(Frame *src, size_t len) {
    return active ? this->callback_buffer_ptr->readArray(src, len) : 0;
  }

 protected:
  NBuffer<Frame> *callback_buffer_ptr;
  bool active;

  virtual size_t writeExt(const uint8_t *data, size_t len) {
    return callback_buffer_ptr->writeArray((Frame *)data, len / sizeof(Frame));
  }

  virtual size_t readExt(uint8_t *data, size_t len) {
    return callback_buffer_ptr->readArray((Frame *)data, len / sizeof(Frame));
    ;
  }
};

/**
 * @brief Stream Adapter for ESP8288-Audio AudioOutput
 *
 */
class ESP3288AudioOutput : public AudioStream {
 public:
  ESP3288AudioOutput(::AudioOutput &out, int channels) {
    p_out = &out;
    this->channels = channels;
  }

  virtual size_t write(const uint8_t *data, size_t len) {
    size_t result = 0;
    int16_t *v = (int16_t *)data;
    if (channels == 2) {
      result = p_out->ConsumeSamples(v, len / 2);
    } else {
      LOGE("Only 2 Channels are supported");
      result = 0;
    }
    return result;
  }

 protected:
  ::AudioOutput *p_out = nullptr;
  int channels;
};
}  // namespace audio_tools
