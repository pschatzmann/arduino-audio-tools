#pragma once
#include "AudioConfig.h"
#include "AudioTools/AudioTypes.h"

#ifndef PIN_ANALOG_START
#  define PIN_ANALOG_START -1
#endif

#ifndef ANALOG_BUFFERS
#  define ANALOG_BUFFERS 10
#endif

#ifndef ANALOG_MAX_OUT_CHANNELS
#  define ANALOG_MAX_OUT_CHANNELS 10
#endif


namespace audio_tools {

class AnalogAudioArduino;

/**
 * @brief Generic ADC and DAC configuration 
 * 
 * @author Phil Schatzmann
 * @ingroup platform
 * @copyright GPLv3
 */
class AnalogConfigStd : public AudioInfo {
  friend class AnalogAudioArduino;
  public:
    int buffer_count = ANALOG_BUFFERS;
    int buffer_size = ANALOG_BUFFER_SIZE;
    RxTxMode rx_tx_mode = RX_MODE;
    bool is_blocking_write = true;
    bool is_auto_center_read = true;
    int max_sample_rate = ANALOG_MAX_SAMPLE_RATE;
    int start_pin = PIN_ANALOG_START;

    AnalogConfigStd() = default;
    AnalogConfigStd(RxTxMode rxtxMode) : AudioInfo() {
      rx_tx_mode = rxtxMode;
    }

  /// support assignament of int array
  template <typename T, int N>
  void setPins(T (&a)[N]) {
    pins_data.clear();
    pins_data.resize(N);
    for (int i = 0; i < N; ++i) {
      pins_data[i] = a[i];  // reset all elements
    }
  }

  // /// Defines the pins and the corresponding number of channels (=number of
  // /// pins)
  // void setPins(Pins &pins) {
  //   pins_data.clear();
  //   for (int i = 0; i < pins.size(); i++) {
  //     pins_data.push_back(pins[i]);
  //   }
  // }

  /// Determines the pins (for all channels)
  Pins &pins() {
    if (pins_data.size() == 0 && start_pin >= 0) {
      pins_data.resize(channels);
      for (int j = 0; j < channels; j++) {
        pins_data[j] = start_pin + j;
      }
    }
    return pins_data;
  }

 protected:
  Pins pins_data{0};
  
};

#ifndef ANALOG_CONFIG
#define ANALOG_CONFIG
using AnalogConfig = AnalogConfigStd;
#endif

} // ns
#//endif
