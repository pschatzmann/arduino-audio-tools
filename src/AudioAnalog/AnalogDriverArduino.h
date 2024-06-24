#pragma once

#include "AudioConfig.h"
#if defined(USE_ANALOG_ARDUINO) || defined(DOXYGEN)

#include <limits.h>  // for INT_MIN and INT_MAX
#include "AudioAnalog/AnalogAudioArduino.h"
#include "AudioTimer/AudioTimer.h"
#include "AudioTools/AudioStreams.h"
#include "AudioTools/AudioTypes.h"
#include "AudioTools/Buffers.h"

namespace audio_tools {

/**
 * @brief Please use the AnalogAudioStream: Reading Analog Data using a timer
 * and the Arduino analogRead() method and writing using analogWrite();
 * @ingroup platform
 * @author Phil Schatzmann
 * @copyright GPLv3
 *
 */
class AnalogDriverArduino : public AnalogDriverBase {
 public:
  AnalogDriverArduino() = default;

  bool begin(AnalogConfig cfg) { return drv.begin(cfg); }

  void end() override { drv.end(); }

  int available() override { return drv.available(); };

  /// Provides the sampled audio data
  size_t readBytes(uint8_t *data, size_t len) override {
    return drv.write(data, len);
  }

  int availableForWrite() override { return drv.availableForWrite(); }

  size_t write(const uint8_t *data, size_t len) override {
    return drv.write(data, len);
  }

 protected:
  AnalogAudioArduino drv;
};

using AnalogDriver = AnalogDriverArduino;

}  // namespace audio_tools

#endif