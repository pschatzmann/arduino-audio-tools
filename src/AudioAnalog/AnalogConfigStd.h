#pragma once
#include "AudioConfig.h"
//#if defined(USE_ANALOG) 

#ifndef PIN_ANALOG_START
#  define PIN_ANALOG_START 1
#endif

#ifndef ANALOG_BUFFERS
#  define ANALOG_BUFFERS 10
#endif

#ifndef ANALOG_MAX_OUT_CHANNELS
#  define ANALOG_MAX_OUT_CHANNELS 10
#endif


namespace audio_tools {

/**
 * @brief Generic ADC and DAC configuration 
 * 
 * @author Phil Schatzmann
 * @ingroup platform
 * @copyright GPLv3
 */
class AnalogConfigStd : public AudioInfo {
  public:
    int buffer_count = PWM_BUFFER_COUNT;
    int buffer_size = PWM_BUFFER_SIZE;
    RxTxMode rx_tx_mode;
    bool is_blocking_write = true;
    bool is_auto_center_read = true;

    /// Copy constructor
    AnalogConfigStd(const AnalogConfigStd &cfg) = default;

    AnalogConfigStd() {
        sample_rate = 44100;
        bits_per_sample = 16;
        channels = 2;
        buffer_size = ANALOG_BUFFER_SIZE;
        buffer_count = ANALOG_BUFFERS;
        rx_tx_mode = RX_MODE;
    }
    /// Default constructor
    AnalogConfigStd(RxTxMode rxtxMode) : AudioInfo() {
      rx_tx_mode = rxtxMode;
    }
    int start_pin = PIN_ANALOG_START;
  
};

#ifndef ANALOG_CONFIG
#define ANALOG_CONFIG
using AnalogConfig = AnalogConfigStd;
#endif

} // ns
#//endif
