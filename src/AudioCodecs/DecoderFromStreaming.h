#pragma once

#include "AudioCodecs/AudioEncoded.h"
#include "AudioTools/Buffers.h"
#include "AudioTools/AudioStreams.h"

namespace audio_tools {

/**
 * @brief Adapter class which allows the AudioDecoder API on a StreamingDecoder
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class DecoderFromStreaming : public AudioDecoder {
public:
  /// Default Constructor
  DecoderFromStreaming(StreamingDecoder &dec, int bufferSize) {
    TRACED();
    p_dec = &dec;
    resize(bufferSize);
  }

  /// Defines the output Stream
  void setOutput(Print &out) override {
    p_dec->setInputStream(queue);
    p_dec->setOutput(out);
  }

  void begin() override {
    TRACED();
    active = true;
    p_dec->begin();
    queue.begin();
  }

  void end() override {
    TRACED();
    active = false;
  }

  /// resize the buffer
  void resize(int size){
    rbuffer.resize(size);
  }

  size_t write(const void *data, size_t byteCount) override {
    TRACED();
    size_t result = queue.write((uint8_t *)data, byteCount);
    // trigger processing - we leave byteCount in the buffer
    // while(queue.available()>byteCount){
    //   p_dec->copy(); 
    //   delay(1);
    // }
    while(p_dec->copy());
    
    return result;
  }

  operator bool() override { return active; }

protected:
  bool active = false;
  StreamingDecoder *p_dec = nullptr;
  RingBuffer<uint8_t> rbuffer{0};
  QueueStream<uint8_t> queue{rbuffer}; // convert Buffer to Stream
};

} // namespace audio_tools