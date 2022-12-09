#pragma once

#include "AudioConfig.h"
#if defined(USE_NANO33BLE) 

#include "AudioTools/AudioTypes.h"
#include "AudioTools/Buffers.h"
#include "AudioTools/AudioLogger.h"
#include "AudioI2S/I2SConfig.h"


namespace audio_tools {

INLINE_VAR const int i2s_buffer_size = 1024;
INLINE_VAR NBuffer<uint8_t> i2s_buffer(i2s_buffer_size, 5);

/**
 *  @brief Mapping Frequency constants to available frequencies
 */
struct Nano_BLE_freq_info {
  int id;
  double freq; // in mhz
};

INLINE_VAR const Nano_BLE_freq_info freq_table[] = {
  { I2S_CONFIG_MCKFREQ_MCKFREQ_32MDIV8, 32.0 / 8 },
  { I2S_CONFIG_MCKFREQ_MCKFREQ_32MDIV10, 32 / 10  },
  { I2S_CONFIG_MCKFREQ_MCKFREQ_32MDIV11, 32.0 / 11 },
  { I2S_CONFIG_MCKFREQ_MCKFREQ_32MDIV15, 32.0 / 15 },
  { I2S_CONFIG_MCKFREQ_MCKFREQ_32MDIV16, 32.0 / 16 },
  { I2S_CONFIG_MCKFREQ_MCKFREQ_32MDIV21, 32.0 / 21 },
  { I2S_CONFIG_MCKFREQ_MCKFREQ_32MDIV23, 32.0 / 23 },
  { I2S_CONFIG_MCKFREQ_MCKFREQ_32MDIV30, 32.0 / 30 },
  { I2S_CONFIG_MCKFREQ_MCKFREQ_32MDIV31, 32.0 / 31 },
  { I2S_CONFIG_MCKFREQ_MCKFREQ_32MDIV32, 32.0 / 32 },
  { I2S_CONFIG_MCKFREQ_MCKFREQ_32MDIV42, 32.0 / 42 },
  { I2S_CONFIG_MCKFREQ_MCKFREQ_32MDIV63, 32.0 / 63 },
  { I2S_CONFIG_MCKFREQ_MCKFREQ_32MDIV125, 32.0 / 125 }
};

/**
 *  @brief Mapping from Ratio Constants to frequency ratios
 */
struct Nano_BLE_ratio_info {
  int id;
  double ratio;
};

INLINE_VAR const Nano_BLE_ratio_info ratio_table[] = {
  { I2S_CONFIG_RATIO_RATIO_32X, 32.0  },
  { I2S_CONFIG_RATIO_RATIO_48X, 48.0  },
  { I2S_CONFIG_RATIO_RATIO_64X, 64.0 },
  { I2S_CONFIG_RATIO_RATIO_96X, 96.0  },
  { I2S_CONFIG_RATIO_RATIO_128X, 128.0  },
  { I2S_CONFIG_RATIO_RATIO_192X, 192.0  },
  { I2S_CONFIG_RATIO_RATIO_256X, 256.0  },
  { I2S_CONFIG_RATIO_RATIO_384X, 384.0  },
  { I2S_CONFIG_RATIO_RATIO_512X, 512.0 }
};

/**
 *  I2S Event handler
 */

extern "C"  void I2S_IRQHandler(void) {
    if(NRF_I2S->EVENTS_TXPTRUPD != 0) {
      // reading from buffer to pins
      NRF_I2S->TXD.PTR = (uint32_t) i2s_buffer.readEnd().address(); // last buffer was processed
      NRF_I2S->EVENTS_TXPTRUPD = 0;

    } else if(NRF_I2S->EVENTS_RXPTRUPD != 0) {
      // reading from pins writing to buffer
      NRF_I2S->RXD.PTR = (uint32_t) i2s_buffer.writeEnd().address(); // last buffer was processed
      NRF_I2S->EVENTS_RXPTRUPD = 0;
    }
} 

/**
 * @brief Basic I2S API - for the Arduino Nano BLE Sense
 * @author Phil Schatzmann
 * @ingroup platform
 * @copyright GPLv3
 */
class I2SDriverNanoBLE {
  friend class I2SStream;

  public:

    I2SDriverNanoBLE(){
      // register IRQ for I2S
      setupIRQ();
    }
    
    /// Provides the default configuration
    I2SConfig defaultConfig(RxTxMode mode) {
        I2SConfig c(mode);
        return c;
    }

    /// starts the DAC with the default config in TX Mode
    bool begin(RxTxMode mode = TX_MODE) {
        return begin(defaultConfig(mode));
    }

    /// starts the DAC 
    bool begin(I2SConfig cfg) {
        LOGD(__func__);
        this->cfg = cfg;
        setupRxTx(cfg);
        setupClock(cfg);
        setupBitWidth(cfg);
        setupMode(cfg);
        setupPins(cfg);
        setupData(cfg);

        // Use stereo
        NRF_I2S->CONFIG.CHANNELS = I2S_CONFIG_CHANNELS_CHANNELS_Stereo << I2S_CONFIG_CHANNELS_CHANNELS_Pos;
        // Setup master or slave mode
        NRF_I2S->CONFIG.MODE = cfg.is_master ? 0 << I2S_CONFIG_MODE_MODE_Pos : 1 << I2S_CONFIG_MODE_MODE_Pos ;
        // ensble I2S
        NRF_I2S->ENABLE = 1;
        // start task
        NRF_I2S->TASKS_START = 1;
        return true;
    }

    int available() {
      return i2s_buffer.available();
    }

    int availableForWrite() {
      return i2s_buffer.availableForWrite();
    }

    /// stops the I2C and unistalls the driver
    void end(){
        LOGD(__func__);
        // stop task
        NRF_I2S->TASKS_START = 0;
        // ensble I2S
        NRF_I2S->ENABLE = 0;
    }

    /// provides the actual configuration
    I2SConfig config() {
      return cfg;
    }

  protected:
    I2SConfig cfg;
    
    /// writes the data to the I2S buffer
    size_t writeBytes(const void *src, size_t size_bytes){
      size_t result = i2s_buffer.writeArray((uint8_t*)src, size_bytes);          
      return result;
    }

    // reads the data from the I2S buffer
    size_t readBytes(void *dest, size_t size_bytes){
      size_t result = i2s_buffer.readArray((uint8_t*)dest, size_bytes);          
      return result;
    }

    void setupIRQ() {
        TRACED();
        NVIC_EnableIRQ(I2S_IRQn);      
    }

    void setupRxTx(I2SConfig cfg) {
        TRACED();
        if (cfg.rx_tx_mode == TX_MODE) { 
          // Enable transmission
          NRF_I2S->CONFIG.TXEN = (I2S_CONFIG_TXEN_TXEN_Enabled << I2S_CONFIG_TXEN_TXEN_Pos);
        } else {
          // Enable reception
          NRF_I2S->CONFIG.RXEN = (I2S_CONFIG_RXEN_RXEN_Enabled << I2S_CONFIG_RXEN_RXEN_Pos);
        }
    }

    void setupClock(I2SConfig cfg){
        TRACED();
        // Enable MCK generator if in master mode
        if (cfg.is_master){
          NRF_I2S->CONFIG.MCKEN = (I2S_CONFIG_MCKEN_MCKEN_Enabled << I2S_CONFIG_MCKEN_MCKEN_Pos);
        }

        // find closest frequency for requested sample_rate
        float freq_requested = cfg.sample_rate * cfg.bits_per_sample ;
        float selected_freq = 0;
        for (auto freq : freq_table) {
          for (auto div : ratio_table) {
              float freq_value = freq.freq * 1000000 / div.ratio;
              if (abs(freq_value-freq_requested) < abs(selected_freq-freq_requested)){
                // MCKFREQ
                NRF_I2S->CONFIG.MCKFREQ = freq.id << I2S_CONFIG_MCKFREQ_MCKFREQ_Pos;
                // Ratio 
                NRF_I2S->CONFIG.RATIO = div.id << I2S_CONFIG_RATIO_RATIO_Pos;
                selected_freq = freq_value;
              }
           }
           LOGI("frequency requested %f vs %f", freq_requested, selected_freq);
        }
    }

    void setupBitWidth(I2SConfig cfg) {
        TRACED();
        uint16_t swidth = I2S_CONFIG_SWIDTH_SWIDTH_16Bit;
        switch(cfg.bits_per_sample){
          case 8:
            NRF_I2S->CONFIG.SWIDTH = I2S_CONFIG_SWIDTH_SWIDTH_8Bit << I2S_CONFIG_SWIDTH_SWIDTH_Pos;
            break;
          case 16:
            NRF_I2S->CONFIG.SWIDTH = I2S_CONFIG_SWIDTH_SWIDTH_16Bit << I2S_CONFIG_SWIDTH_SWIDTH_Pos;
            break;
          case 24:
            NRF_I2S->CONFIG.SWIDTH = I2S_CONFIG_SWIDTH_SWIDTH_24Bit << I2S_CONFIG_SWIDTH_SWIDTH_Pos;
            break;  
          default:
            LOGE("Unsupported bit width: %d", cfg.bits_per_sample);
        }
    }

    void setupMode(I2SConfig cfg){
        TRACED();
        // setup mode
        switch(cfg.i2s_format){
          case I2S_STD_FORMAT:
          case I2S_PHILIPS_FORMAT:
            NRF_I2S->CONFIG.ALIGN = I2S_CONFIG_FORMAT_FORMAT_I2S << I2S_CONFIG_FORMAT_FORMAT_Pos;
            break;
          case I2S_MSB_FORMAT:
          case I2S_LEFT_JUSTIFIED_FORMAT:
            NRF_I2S->CONFIG.FORMAT = I2S_CONFIG_FORMAT_FORMAT_I2S << I2S_CONFIG_FORMAT_FORMAT_Pos;
            NRF_I2S->CONFIG.ALIGN = I2S_CONFIG_ALIGN_ALIGN_Left << I2S_CONFIG_ALIGN_ALIGN_Pos;;
            break;
          case I2S_LSB_FORMAT:
          case I2S_RIGHT_JUSTIFIED_FORMAT:
            NRF_I2S->CONFIG.FORMAT = I2S_CONFIG_FORMAT_FORMAT_I2S << I2S_CONFIG_FORMAT_FORMAT_Pos;
            NRF_I2S->CONFIG.ALIGN = I2S_CONFIG_ALIGN_ALIGN_Right << I2S_CONFIG_ALIGN_ALIGN_Pos;;
            break;
        }
    }

    // setup pins
    void setupPins(I2SConfig cfg){
        TRACED();
        // MCK routed to pin 0
        // if (cfg.is_master){
        //   NRF_I2S->PSEL.MCK = (0 << I2S_PSEL_MCK_PIN_Pos) | (I2S_PSEL_MCK_CONNECT_Connected << I2S_PSEL_MCK_CONNECT_Pos);
        // }
        // SCK - bit clock -  routed to pin 1
        NRF_I2S->PSEL.SCK = (cfg.pin_bck << I2S_PSEL_SCK_PIN_Pos) | (I2S_PSEL_SCK_CONNECT_Connected << I2S_PSEL_SCK_CONNECT_Pos);
        // LRCK routed to pin 2
        NRF_I2S->PSEL.LRCK = (cfg.pin_ws << I2S_PSEL_LRCK_PIN_Pos) | (I2S_PSEL_LRCK_CONNECT_Connected << I2S_PSEL_LRCK_CONNECT_Pos);
        if (cfg.rx_tx_mode == TX_MODE) { 
          // SDOUT routed to pin 3
          NRF_I2S->PSEL.SDOUT = (cfg.pin_data << I2S_PSEL_SDOUT_PIN_Pos) | (I2S_PSEL_SDOUT_CONNECT_Connected << I2S_PSEL_SDOUT_CONNECT_Pos);
        } else {
          // SDIN routed on pin 4
          NRF_I2S->PSEL.SDIN = (cfg.pin_data << I2S_PSEL_SDIN_PIN_Pos) |(I2S_PSEL_SDIN_CONNECT_Connected << I2S_PSEL_SDIN_CONNECT_Pos);
        }
    }

    // setup initial data pointers
    void setupData(I2SConfig cfg) {
        TRACED();
        NRF_I2S->TXD.PTR = (uint32_t) i2s_buffer.readEnd().address(); // last buffer was processed
        NRF_I2S->RXD.PTR = (uint32_t) i2s_buffer.writeEnd().address(); // last buffer was processed
        NRF_I2S->RXTXD.MAXCNT = i2s_buffer_size;
    }

};

using I2SDriver = I2SDriverNanoBLE;


} // namespace

#endif
