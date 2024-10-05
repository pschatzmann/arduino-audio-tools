#pragma once
#include "AudioConfig.h"
#ifndef USE_I2S
#  define USE_I2S
#  define CREATE_I2S
#endif
#include "AudioTools/CoreAudio/AudioStreams.h"
#include "AudioTools/CoreAudio/AudioI2S/I2SConfig.h"
#include "SPI.h"

namespace audio_tools {

/**
 * @brief I2S emulated with the help of the Arduion SPI api.
 * @author Phil Schatzmann
 * @ingroup io
 * @copyright GPLv3
 */
class I2SBitBang : public AudioStream {
public:
  bool begin() {
    buffer.resize(i2s_config.bits_per_sample / 8);
    uint32_t speed = i2s_config.sample_rate * i2s_config.channels * i2s_config.bits_per_sample;
    SPISettings settings{speed, MSBFIRST, SPI_MODE0};
#ifdef USE_SPI_SET_PINS
    SPI.setCS(PIN_I2S_WS);
    SPI.setSCK(PIN_I2S_BCK);
    SPI.setTX(PIN_I2S_DATA);
#endif
    SPI.begin();
    SPI.beginTransaction(settings);
    return true;
  }

  void setAudioInfo(AudioInfo cfg) {
    i2s_config.copyFrom(cfg);
    begin(i2s_config);
  }

  bool begin(I2SConfig info) {
    //AudioStream::setAudioInfo(info);
    i2s_config = info;
    return begin();
  }

  void end() { SPI.end(); }

  size_t write(const uint8_t *data, size_t len) {
    for (int j = 0; j < len; j++) {
      buffer.write(data[j]);
      if (buffer.availableForWrite() == 0) {
        digitalWrite(i2s_config.pin_ws, ws_state);
        SPI.transfer(buffer.data(), buffer.size());
        // toggle word select
        ws_state = !ws_state;
        buffer.reset();
      }
    }
    return len;
  }

protected:
  I2SConfig i2s_config;
  bool ws_state = false;
  SingleBuffer<uint8_t> buffer{0};
};

// Make sure that we have one and only one I2SStream
#ifdef CREATE_I2S
using I2SStream = I2SBitBang;
#endif

} // namespace audio_tools