#pragma once
#include "AudioConfig.h"
#ifdef USE_I2S

namespace audio_tools {

/**
 * @brief I2S Formats
 */
enum I2SFormat {
  I2S_STD_FORMAT,
  I2S_LSB_FORMAT,
  I2S_MSB_FORMAT,
  I2S_PHILIPS_FORMAT,
  I2S_RIGHT_JUSTIFIED_FORMAT,
  I2S_LEFT_JUSTIFIED_FORMAT,
  I2S_PCM,
};

/**
 * @brief I2S Signal Types: Digital, Analog, PDM
 */
enum I2SSignalType {
  Digital,
  Analog,
  PDM,
  TDM
};

INLINE_VAR const char* i2s_formats[] = {"I2S_STD_FORMAT","I2S_LSB_FORMAT","I2S_MSB_FORMAT","I2S_PHILIPS_FORMAT","I2S_RIGHT_JUSTIFIED_FORMAT","I2S_LEFT_JUSTIFIED_FORMAT","I2S_PCM"};


/**
 * @brief configuration for all common i2s settings
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class I2SConfig : public AudioInfo {
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
        rx_tx_mode = mode;
        switch(mode){
          case RX_MODE:
            pin_data = PIN_I2S_DATA_IN;
            #ifdef ESP32
               auto_clear = false;
            #endif
            break;
          case TX_MODE:
            pin_data = PIN_I2S_DATA_OUT;
            #ifdef ESP32
                auto_clear = I2S_AUTO_CLEAR;
            #endif
            break;
          default: 
            #ifdef ESP32
                auto_clear = I2S_AUTO_CLEAR;
            #endif
            pin_data = PIN_I2S_DATA_OUT;
            pin_data_rx = PIN_I2S_DATA_IN;
            break;
        }
    }

    /// public settings
    RxTxMode rx_tx_mode = TX_MODE;
    bool is_master = true;
    int port_no = 0;  // processor dependent port
    int pin_ws = PIN_I2S_WS;
    int pin_bck = PIN_I2S_BCK;
    int pin_data; // rx or tx pin dependent on mode: tx pin for RXTX_MODE
    int pin_data_rx; // rx pin for RXTX_MODE
    int pin_mck = 0;
    I2SFormat i2s_format = I2S_STD_FORMAT;

#if defined(STM32)
    int buffer_count = I2S_BUFFER_COUNT;
    int buffer_size = I2S_BUFFER_SIZE;
#elif defined(ESP32)
    int buffer_count = I2S_BUFFER_COUNT;
    int buffer_size = I2S_BUFFER_SIZE;
    I2SSignalType signal_type = Digital;  // e.g. the ESP32 supports analog input or output or PDM picrophones
    bool auto_clear = I2S_AUTO_CLEAR;
    bool use_apll = I2S_USE_APLL; 
    uint32_t fixed_mclk = 0; 

#endif

    void logInfo() {
      LOGI("rx/tx mode: %s", RxTxModeNames[rx_tx_mode]);
      LOGI("port_no: %d", port_no);
      LOGI("is_master: %s", is_master ? "Master":"Slave");
      LOGI("sample rate: %d", sample_rate);
      LOGI("bits per sample: %d", bits_per_sample);
      LOGI("number of channels: %d", channels);
      LOGI("i2s_format: %s", i2s_formats[i2s_format]);
#ifdef ESP32
      LOGI("auto_clear: %s",auto_clear? "true" : "false");
      if (use_apll) {
        LOGI("use_apll: %s", use_apll ? "true" : "false");
      }
      if (fixed_mclk){
        LOGI("fixed_mclk: %d", (int) fixed_mclk);
      }
      LOGI("buffer_count:%d",buffer_count);
      LOGI("buffer_size:%d",buffer_size);

#endif
      if (pin_mck!=-1)
        LOGI("pin_mck: %d", pin_mck);
      if (pin_bck!=-1)
        LOGI("pin_bck: %d", pin_bck);
      if (pin_ws!=-1)
        LOGI("pin_ws: %d", pin_ws);
      if (pin_data!=-1)
        LOGI("pin_data: %d", pin_data);
      if (pin_data_rx!=-1 && rx_tx_mode==RXTX_MODE){
        LOGI("pin_data_rx: %d", pin_data_rx);
      }
    }

};

}

#endif