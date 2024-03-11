
#pragma once

#include "AudioConfig.h"
#if defined(IS_MBED) && defined(USE_ANALOG) || defined(DOXYGEN)

namespace audio_tools {

/**
 * @brief Please use AnalogAudioStream: A ADC and DAC API for the Arduino Giga.
 * @ingroup platform
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class AnalogDriverMBED : public AnalogDriverBase {
public:
  /// Default constructor
  AnalogDriverMBED() = default;

  /// Destructor
  virtual ~AnalogDriverMBED() { end(); }

  /// starts the DAC
  bool begin(AnalogConfig cfg) {
    TRACEI();
    if (cfg.bits_per_sample != 16) {
      LOGE("Only 16 bits_per_sample supported");
      return false;
    }
    if (cfg.channels > 2) {
      LOGE("max channels: 2");
      return false;
    }
    if (cfg.channels <= 0) {
      LOGE("no channels");
      return false;
    }
    info = cfg;
    auto_center.begin(config.channels, config.bits_per.sample);
    int n_samples = cfg.buffer_size / (cfg.bits_per_sample / 8);
    ring_buffer.resize(n_samples);
    switch (info.channels) {
    case 1:
      dac1.begin(AN_RESOLUTION_12, info.sample_rate, n_samples,
                 cfg.buffer_count);
      break;
    case 2:
      dac1.begin(AN_RESOLUTION_12, info.sample_rate, n_samples / 2,
                 cfg.buffer_count);
      dac2.begin(AN_RESOLUTION_12, info.sample_rate, n_samples / 2,
                 cfg.buffer_count);
      break;
    }
    return true;
  }

  /// stops the I2S and unistalls the driver
  void end() override {
    active = false;
    dac1.stop();
    dac2.stop();
    adc1.stop();
    adc2.stop();
  }

  int availableForWrite() {
    return dac1.available() ? info.buffer_size : 0;
  }

  /// writes the data to the I2S interface
  size_t write(const uint8_t *src, size_t size_bytes) override {
    TRACED();
    if (!dac1.available())
      return 0;

    // collect data in ringbuffer
    size_t result = 0;
    int sample_count = size_bytes / 2;
    Sample *data = (Sample *)src;
    for (int j = 0; j < sample_count; j++) {
      ring_buffer.write(data[j]);
      // process ringbuffer when it is full
      if (ring_buffer.isFull()) {
        result += writeBuffer();
      }
    }
    return result;
  }

  void flush() {
    const int size = info.buffer_size;
    const uint8_t data[size] = {0};
    write(data, size);
    ring_buffer.reset();
  }

  size_t readBytes(uint8_t *dest, size_t size_bytes) override {
    TRACED();
    size_t result = 0;
    int16_t *data = (int16_t *)dest;
    size_t samples = size_bytes / 2;
    switch (info.channels) {
    case 1:
      for (int j = 0; j < samples; j++) {
        data[j] = adc1.read();
        result += 2;
      }
      break;
    case 2:
      for (int j = 0; j < samples; j += 2) {
        data[j] = adc1.read();
        data[j + 1] = adc2.read();
        result += 4;
      }
      break;
    }

    // make sure that the center is at 0
    if (adc_config.is_auto_center_read){
        auto_center.convert(dest, result);
    }

    return result;
  }

  virtual int available() override { return info.buffer_size; }

protected:
  audio_tools::RingBuffer<Sample> ring_buffer{0};
  AnalogConfig info;
  ConverterAutoCenter auto_center;
  AdvancedDAC dac1{PIN_DAC_1};
  AdvancedDAC dac2{PIN_DAC_2};
  AdvancedADC adc1{PIN_ANALOG_START};
  AdvancedADC adc2{PIN_ANALOG_START + 1};
  bool active = false;

  /// The ringbuffer is used to make sure that we can write full SampleBuffers
  size_t writeBuffer() {
    size_t result = 0;
    switch (info.channels) {
    case 1: {
      SampleBuffer buf = dac1.dequeue();
      for (size_t i = 0; i < buf.size(); i++) {
        buf[i] = ring_buffer.read();
        result += 2;
      }
      dac1.write(buf);
    } break;
    case 2: {
      SampleBuffer buf1 = dac1.dequeue();
      SampleBuffer buf2 = dac2.dequeue();
      for (size_t i = 0; i < buf1.size(); i += 2) {
        buf1[i] = ring_buffer.read();
        buf2[i] = ring_buffer.read();
        result += 4;
      }
      dac1.write(buf1);
      dac2.write(buf2);
    } break;
    }
    assert(ring_buffer.isEmpty());
    return result;
  }
};

/// @brief AnalogAudioStream
using AnalogDriver = AnalogDriverMBED;

} // namespace audio_tools

#endif // USE_ANALOG