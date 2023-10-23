#pragma once
#include "AudioConfig.h"
#include "AudioTools/AudioTypes.h"

#ifndef PIN_I2S_MCK
#  define PIN_I2S_MCK -1
#endif

#if defined(ESP32) && ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0 , 0)
#  include "driver/i2s.h" // for I2S_CHANNEL_FMT_RIGHT_LEFT
#endif

namespace audio_tools {

/***
 * @brief I2S Signal Types: Digital, Analog, PDM
 * @ingroup platform
 */
enum I2SSignalType {
  Digital,
  Analog,
  PDM,
};

INLINE_VAR const char* i2s_signal_types[] = {"Digital","Analog","PDM","TDM"};

/**
 * @brief Configuration for ESP32 legacy i2s 
 * @ingroup platform
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class I2SConfigESP32 : public AudioInfo {
  public:

    I2SConfigESP32() {
      channels = DEFAULT_CHANNELS;
      sample_rate = DEFAULT_SAMPLE_RATE; 
      bits_per_sample = DEFAULT_BITS_PER_SAMPLE;
    }

    /// Default Copy Constructor
    I2SConfigESP32(const I2SConfigESP32 &cfg) = default;

    /// Constructor
    I2SConfigESP32(RxTxMode mode) {
        channels = DEFAULT_CHANNELS;
        sample_rate = DEFAULT_SAMPLE_RATE; 
        bits_per_sample = DEFAULT_BITS_PER_SAMPLE;
        rx_tx_mode = mode;
        switch(mode){
          case RX_MODE:
            pin_data = PIN_I2S_DATA_IN;
            auto_clear = false;
            break;
          case TX_MODE:
            pin_data = PIN_I2S_DATA_OUT;
            auto_clear = I2S_AUTO_CLEAR;
            break;
          default: 
            auto_clear = I2S_AUTO_CLEAR;
            pin_data = PIN_I2S_DATA_OUT;
            pin_data_rx = PIN_I2S_DATA_IN;
            break;
        }
    }

    /// public settings
    RxTxMode rx_tx_mode = TX_MODE;
    I2SFormat i2s_format = I2S_STD_FORMAT;
    I2SSignalType signal_type = Digital;  // e.g. the ESP32 supports analog input or output or PDM picrophones
    bool is_master = true;
    int port_no = 0;  // processor dependent port
    int pin_ws = PIN_I2S_WS;
    int pin_bck = PIN_I2S_BCK;
    int pin_data; // rx or tx pin dependent on mode: tx pin for RXTX_MODE
    int pin_data_rx; // rx pin for RXTX_MODE
    int pin_mck = PIN_I2S_MCK;
    int buffer_count = I2S_BUFFER_COUNT;
    int buffer_size = I2S_BUFFER_SIZE;
    bool auto_clear = I2S_AUTO_CLEAR;
    bool use_apll = I2S_USE_APLL; 
    uint32_t fixed_mclk = 0; 
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0 , 0)
    int channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
#endif

    void logInfo(const char* source=nullptr) {
      AudioInfo::logInfo(source);
      LOGI("rx/tx mode: %s", RxTxModeNames[rx_tx_mode]);
      LOGI("port_no: %d", port_no);
      LOGI("is_master: %s", is_master ? "Master":"Slave");
      LOGI("sample rate: %d", sample_rate);
      LOGI("bits per sample: %d", bits_per_sample);
      LOGI("number of channels: %d", channels);
      LOGI("signal_type: %s", i2s_signal_types[signal_type]);      
      if (signal_type==Digital){
        LOGI("i2s_format: %s", i2s_formats[i2s_format]);      
      } 
      LOGI("auto_clear: %s",auto_clear? "true" : "false");
      if (use_apll) {
        LOGI("use_apll: %s", use_apll ? "true" : "false");
      }
      if (fixed_mclk){
       LOGI("fixed_mclk: %d", (int) fixed_mclk);
      }
      LOGI("buffer_count:%d",buffer_count);
      LOGI("buffer_size:%d",buffer_size);

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

using I2SConfig = I2SConfigESP32;

}

