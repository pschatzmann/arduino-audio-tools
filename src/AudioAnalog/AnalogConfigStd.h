#pragma once
#include "AudioConfig.h"
#if defined(USE_ANALOG) 

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
    AnalogConfigStd(RxTxMode rxtxMode) : AnalogConfig() {
      rx_tx_mode = rxtxMode;
    }
    int start_pin = PIN_ANALOG_START;
  
};

using AnalogConfig = AnalogConfigStd;

} // ns
#endif
