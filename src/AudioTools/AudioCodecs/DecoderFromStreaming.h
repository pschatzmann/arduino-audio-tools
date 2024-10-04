#pragma once

#include "AudioTools/AudioCodecs/AudioCodecsBase.h"
#include "AudioTools/CoreAudio/Buffers.h"
//#include "AudioTools/CoreAudio/AudioStreams.h"

namespace audio_tools {

/**
 * @brief Adapter class which allows the AudioDecoder API on a StreamingDecoder
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class DecoderAdapter : public AudioDecoder {
public:
  /// Default Constructor
  DecoderAdapter(StreamingDecoder &dec, int bufferSize) {
    TRACED();
    p_dec = &dec;
    p_dec->setInput(queue);
    resize(bufferSize);
  }

  /// Defines the output Stream
  void setOutput(Print &out) override {
    p_dec->setOutput(out);
  }

  void setInput(Stream& in){
    p_dec->setInput(in);
  }

  bool begin() override {
    TRACED();
    active = true;
    bool rc = p_dec->begin();
    return rc;
  }

  void end() override {
    TRACED();
    active = false;
  }

  /// resize the buffer
  void resize(int size){
    buffer_size = size;
    // setup the buffer only if needed
    if (is_setup) rbuffer.resize(size);
  }

  size_t write(const uint8_t *data, size_t len) override {
    TRACED();
    setupLazy();
    size_t result = queue.write((uint8_t *)data, len);
    // trigger processing - we leave byteCount in the buffer
    // while(queue.available()>byteCount){
    //   p_dec->copy(); 
    //   delay(1);
    // }
    while(p_dec->copy());
    
    return result;
  }

  StreamingDecoder* getStreamingDecoder(){
    return p_dec;
  }

  operator bool() override { return active; }

protected:
  bool active = false;
  bool is_setup = false;
  int buffer_size;
  StreamingDecoder *p_dec = nullptr;
  RingBuffer<uint8_t> rbuffer{0};
  QueueStream<uint8_t> queue{rbuffer}; // convert Buffer to Stream

  void setupLazy(){
    if (!is_setup){
      rbuffer.resize(buffer_size);
      queue.begin();
      is_setup = true;
    }
  }
};

using DecoderFromStreaming = DecoderAdapter;

} // namespace audio_tools