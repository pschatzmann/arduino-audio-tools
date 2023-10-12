#pragma once
#include "AudioConfig.h"
#if defined(USE_ANALOG) 
#if defined(ESP32) 
#  include "AnalogConfigESP32.h"
#  include "AnalogConfigESP32V1.h"
#else
#  include "AnalogConfigStd.h"
#endif

namespace audio_tools {

class AnalogDriverBase {
public:
    virtual bool begin(AnalogConfig cfg) = 0;
    virtual void end() = 0;
    virtual size_t write(const uint8_t *src, size_t size_bytes) { return 0;}
    virtual size_t readBytes(uint8_t *dest, size_t size_bytes) = 0;
    virtual int available() = 0;
    virtual int availableForWrite() { return DEFAULT_BUFFER_SIZE; }
};

} // ns
#endif
