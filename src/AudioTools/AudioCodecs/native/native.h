#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <cstddef>
#include "AudioTools/CoreAudio/Buffers.h"

class DecoderNative {

 public:
  bool begin() { return allocateBuffers(); }
  
  void end() { freeBuffers(); }

  size_t write(uint8_t data, size_t len) {
    size_t result = buffer.writeArray(data, len);
    int consumed = 0;
    do {
      int32_t available = buffer.available() int32_t bytes_left = available;
      int16_t* outbuf;
      decode(buffer.data(), &bytes_left, outbuf, available);
      consumed = available - bytes_left;
      buffer.consume(consumed);
    } while (consumed > 0);

    return result;
  }

  operator bool() { return isInit(); }

 protected:
  SingleBuffer<uint8_t> buffer{1024 * 3};

  virtual int32_t getSampRate() = 0;
  virtual int32_t getChannels() = 0;
  virtual int32_t getBitsPerSample() = 0;
  virtual int32_t getBitrate() = 0;
  virtual int32_t getOutputSamps() = 0;


  virtual bool allocateBuffers(void) = 0;
  virtual bool isInit() = 0;
  virtual void freeBuffers() = 0;
  virtual int32_t decode(uint8_t* inbuf, int32_t* bytesLeft, int16_t* outbuf,
                         int32_t useSize) = 0;

  char* codec_malloc(uint16_t len) {
    char* ps_str = nullptr;
#if defined(ESP32)
    if (isPSRAMAvailable()) {
      ps_str = (char*)ps_malloc(len);
    } else {
      ps_str = (char*)malloc(len);
    }
#else
    ps_str = (char*)malloc(len);
#endif
    return ps_str;
  }

  char* codec_calloc(uint16_t len, uint8_t size) {
    char* ps_str = nullptr;
#if defined(ESP32)
    if (isPSRAMAvailable()) {
      ps_str = (char*)ps_calloc(len, size);
    } else {
      ps_str = (char*)calloc(len, size);
    }
#else
    ps_str = (char*)malloc(len);
#endif
    return ps_str;
  }
};