#pragma once
#include "AudioConfig.h"
#ifdef USE_I2S

namespace audio_tools {

enum I2SFormat {
  I2S_STD_FORMAT,
  I2S_LSB_FORMAT,
  I2S_MSB_FORMAT,
  I2S_PHILIPS_FORMAT,
  I2S_RIGHT_JUSTIFIED_FORMAT,
  I2S_LEFT_JUSTIFIED_FORMAT,
  I2S_PCM_LONG,
  I2S_PCM_SHORT
};

const char* i2s_formats[] = {"I2S_STD_FORMAT","I2S_LSB_FORMAT","I2S_MSB_FORMAT","I2S_PHILIPS_FORMAT","I2S_RIGHT_JUSTIFIED_FORMAT","I2S_LEFT_JUSTIFIED_FORMAT","I2S_PCM_LONG","I2S_PCM_SHORT"};


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
    I2SFormat i2s_format = I2S_STD_FORMAT;

#ifdef ESP32
    bool is_digital = true;  // e.g. the ESP32 supports analog input or output
    bool use_apll = I2S_USE_APLL; 
    uint32_t apll_frequency_factor = 0; // apll frequency = sample_rate * apll_frequency_factor
#endif

    void logInfo() {
      LOGI("rx/tx mode: %s", rx_tx_mode == TX_MODE ? "TX":"RX");
      LOGI("port_no: %d", port_no);
      LOGI("is_master: %s", is_master ? "Master":"Slave");
      LOGI("sample rate: %d", sample_rate);
      LOGI("bits per sample: %d", bits_per_sample);
      LOGI("number of channels: %d", channels);
      LOGI("i2s_format: %s", i2s_formats[i2s_format]);
#ifdef ESP32
      if (use_apll) {
        LOGI("use_apll: %s", use_apll ? "true" : "false");
        LOGI("apll_frequency_factor: %d", apll_frequency_factor);
      }
#endif

      LOGI("pin_bck: %d", pin_bck);
      LOGI("pin_ws: %d", pin_ws);
      LOGI("pin_data: %d", pin_data);
    }

};

}

#endif