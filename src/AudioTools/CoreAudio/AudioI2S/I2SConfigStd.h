#pragma once
#include "AudioConfig.h"
#include "AudioTools/CoreAudio/AudioTypes.h"

#ifndef PIN_I2S_MCK
#  define PIN_I2S_MCK -1
#endif

namespace audio_tools {

/**
 * @brief Configuration for i2s 
 * @ingroup platform
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class I2SConfigStd : public AudioInfo {
  public:

    I2SConfigStd() = default;
    /// Default Copy Constructor
    I2SConfigStd(const I2SConfigStd &cfg) = default;

    /// Constructor
    I2SConfigStd(RxTxMode mode) {
        channels = DEFAULT_CHANNELS;
        sample_rate = DEFAULT_SAMPLE_RATE; 
        bits_per_sample = DEFAULT_BITS_PER_SAMPLE;
        rx_tx_mode = mode;

#ifndef STM32
        switch(mode){
          case RX_MODE:
            pin_data = PIN_I2S_DATA_IN;
            break;
          case TX_MODE:
            pin_data = PIN_I2S_DATA_OUT;
            break;
          default: 
            pin_data = PIN_I2S_DATA_OUT;
            pin_data_rx = PIN_I2S_DATA_IN;
            break;
        }
#endif
    }

    /// public settings
    RxTxMode rx_tx_mode = TX_MODE;
    bool is_master = true;
    I2SFormat i2s_format = I2S_STD_FORMAT;
    int buffer_count = I2S_BUFFER_COUNT;
    int buffer_size = I2S_BUFFER_SIZE;

    int pin_ws = PIN_I2S_WS;
    int pin_bck = PIN_I2S_BCK;
    int pin_data = -1; // rx or tx pin dependent on mode: tx pin for RXTX_MODE
    int pin_data_rx = -1; // rx pin for RXTX_MODE
    int pin_mck = PIN_I2S_MCK;
#ifdef STM32
    int pin_alt_function = -1;
#endif

#if defined(RP2040_HOWER)
    /// materclock multiplier for RP2040: must be multiple of 64
    int mck_multiplier  = 64;
    I2SSignalType signal_type = Digital;  // e.g. the RP2040 supports digial or PDM 
#endif

#if defined(USE_ALT_PIN_SUPPORT)
    bool is_arduino_pin_numbers = true;
#endif

    void logInfo(const char* source="") {
      AudioInfo::logInfo(source);
      LOGI("rx/tx mode: %s", RxTxModeNames[rx_tx_mode]);
      //LOGI("port_no: %d", port_no);
      LOGI("is_master: %s", is_master ? "Master":"Slave");
      LOGI("sample rate: %d", (int)sample_rate);
      LOGI("bits per sample: %d", (int)bits_per_sample);
      LOGI("number of channels: %d", (int)channels);
      LOGI("i2s_format: %s", i2s_formats[i2s_format]);      
      LOGI("buffer_count:%d",buffer_count);
      LOGI("buffer_size:%d",buffer_size);

#ifndef STM32
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
#endif
    }

};

using I2SConfig = I2SConfigStd;

}

