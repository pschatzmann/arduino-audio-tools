#pragma once
#include "AudioToolsConfig.h"
#if defined(USE_ANALOG)

#include "AnalogConfig.h"

namespace audio_tools {

class AnalogDriverBase {
public:
    virtual bool begin(AnalogConfig cfg) = 0;
    virtual void end() = 0;
    virtual size_t write(const uint8_t *src, size_t size_bytes) { return 0;}
    virtual size_t readBytes(uint8_t *dest, size_t size_bytes) = 0;
    virtual int available() = 0;
    virtual int availableForWrite() { return DEFAULT_BUFFER_SIZE; }

protected:
    /// Scales int16_t samples with adc_bits resolution to the full 16 bit range.
    /// If the samples are not centered yet, the midpoint of the ADC range is
    /// subtracted first.
    static void scaleTo16Bits(uint8_t *data, size_t size_bytes, int adc_bits,
                              bool is_centered) {
        if (adc_bits <= 0 || adc_bits >= 16) return;
        const int32_t factor = 1 << (16 - adc_bits);
        const int32_t offset = is_centered ? 0 : 1 << (adc_bits - 1);
        int16_t *samples = reinterpret_cast<int16_t *>(data);
        size_t sample_count = size_bytes / sizeof(int16_t);
        for (size_t i = 0; i < sample_count; i++) {
            int32_t value = (static_cast<int32_t>(samples[i]) - offset) * factor;
            if (value > 32767) value = 32767;
            if (value < -32768) value = -32768;
            samples[i] = static_cast<int16_t>(value);
        }
    }
};

} // ns
#endif
