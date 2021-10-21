#pragma once
#include "AudioConfig.h"
#ifdef USE_I2S

namespace audio_tools {

/**
 * @brief configuration for all common i2s settings
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class I2SConfig : public AudioBaseInfo {
  public:

    I2SConfig() {
      channels = DEFAULT_CHANNELS;
      sample_rate = DEFAULT_SAMPLE_RATE; 
      bits_per_sample = DEFAULT_BITS_PER_SAMPLE;
    }

    /// Default Copy Constructor
    I2SConfig(const I2SConfig &cfg) = default;

    /// Constructor
    I2SConfig(RxTxMode mode) {
        channels = DEFAULT_CHANNELS;
        sample_rate = DEFAULT_SAMPLE_RATE; 
        bits_per_sample = DEFAULT_BITS_PER_SAMPLE;
        this->rx_tx_mode = mode;
        pin_data = rx_tx_mode == TX_MODE ? PIN_I2S_DATA_OUT : PIN_I2S_DATA_IN;
    }

    /// public settings
    RxTxMode rx_tx_mode = TX_MODE;
    bool is_master = true;
    int port_no = 0;  // processor dependent port
    int pin_ws = PIN_I2S_WS;
    int pin_bck = PIN_I2S_BCK;
    int pin_data = PIN_I2S_DATA_OUT;
    I2SMode i2s_mode = I2S_STD_MODE;
    bool is_digital = true;  // e.g. the ESP32 supports analog input or output

    void logInfo() {
      LOGI("port_no: %d", port_no);
      LOGI("pin_ws: %d", pin_ws);
      LOGI("pin_bck: %d", pin_bck);
      LOGI("pin_data: %d", pin_data);
      LOGI("i2s_mode: %d", i2s_mode);
    }

};

}

#endif