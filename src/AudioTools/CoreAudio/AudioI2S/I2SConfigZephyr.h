#pragma once
#include "AudioToolsConfig.h"
#include "AudioTools/CoreAudio/AudioTypes.h"
#include <zephyr/device.h>


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
    }

    /// public settings
    RxTxMode rx_tx_mode = TX_MODE;
    bool is_master = true;
    I2SFormat i2s_format = I2S_STD_FORMAT;
    int buffer_count = I2S_BUFFER_COUNT;
    int buffer_size = I2S_BUFFER_SIZE;
    device* device = nullptr;


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
      LOGI("device_name: %s", device_name);

    }

};

using I2SConfig = I2SConfigZephyr;

}

