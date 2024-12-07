#pragma once

#include "AudioConfig.h"

#if defined(USE_NANO33BLE)

#include "AudioTools/CoreAudio/AudioI2S/I2SConfig.h"
#include "AudioTools/CoreAudio/AudioLogger.h"
#include "AudioTools/CoreAudio/AudioTypes.h"
#include "AudioTools/CoreAudio/Buffers.h"

#define IS_I2S_IMPLEMENTED 

namespace audio_tools {

static int i2s_buffer_size = 0;
static BaseBuffer<uint8_t> *p_i2s_buffer = nullptr;
static uint8_t *p_i2s_array = nullptr;    // current array
static uint8_t *p_i2s_array_1 = nullptr;  // array 1
static uint8_t *p_i2s_array_2 = nullptr;  // array 2
static uint32_t i2s_underflow_count = 0;
// alternative API
static Stream *p_nano_ble_stream = nullptr;

/**
 *  @brief Mapping Frequency constants to available frequencies
 */
struct Nano_BLE_freq_info {
  int id;
  float freq;  // in mhz
};

static const Nano_BLE_freq_info freq_table[] = {
    {I2S_CONFIG_MCKFREQ_MCKFREQ_32MDIV8, 32.0 / 8},
    {I2S_CONFIG_MCKFREQ_MCKFREQ_32MDIV10, 32 / 10},
    {I2S_CONFIG_MCKFREQ_MCKFREQ_32MDIV11, 32.0 / 11},
    {I2S_CONFIG_MCKFREQ_MCKFREQ_32MDIV15, 32.0 / 15},
    {I2S_CONFIG_MCKFREQ_MCKFREQ_32MDIV16, 32.0 / 16},
    {I2S_CONFIG_MCKFREQ_MCKFREQ_32MDIV21, 32.0 / 21},
    {I2S_CONFIG_MCKFREQ_MCKFREQ_32MDIV23, 32.0 / 23},
    {I2S_CONFIG_MCKFREQ_MCKFREQ_32MDIV30, 32.0 / 30},
    {I2S_CONFIG_MCKFREQ_MCKFREQ_32MDIV31, 32.0 / 31},
    {I2S_CONFIG_MCKFREQ_MCKFREQ_32MDIV32, 32.0 / 32},
    {I2S_CONFIG_MCKFREQ_MCKFREQ_32MDIV42, 32.0 / 42},
    {I2S_CONFIG_MCKFREQ_MCKFREQ_32MDIV63, 32.0 / 63},
    {I2S_CONFIG_MCKFREQ_MCKFREQ_32MDIV125, 32.0 / 125}};

/**
 *  @brief Mapping from Ratio Constants to frequency ratios
 */
struct Nano_BLE_ratio_info {
  int id;
  float ratio;
};

static const Nano_BLE_ratio_info ratio_table[] = {
    {I2S_CONFIG_RATIO_RATIO_32X, 32.0},   {I2S_CONFIG_RATIO_RATIO_48X, 48.0},
    {I2S_CONFIG_RATIO_RATIO_64X, 64.0},   {I2S_CONFIG_RATIO_RATIO_96X, 96.0},
    {I2S_CONFIG_RATIO_RATIO_128X, 128.0}, {I2S_CONFIG_RATIO_RATIO_192X, 192.0},
    {I2S_CONFIG_RATIO_RATIO_256X, 256.0}, {I2S_CONFIG_RATIO_RATIO_384X, 384.0},
    {I2S_CONFIG_RATIO_RATIO_512X, 512.0}};

void I2S_IRQWrite(void) {
  // Handle Wrtie
  if (NRF_I2S->EVENTS_TXPTRUPD == 1) {
    size_t eff_read = 0;

    // toggle arrays to avoid noise
    p_i2s_array = p_i2s_array == p_i2s_array_1 ? p_i2s_array_2 : p_i2s_array_1;

    if (p_nano_ble_stream != nullptr) {
      // Alternative API via Stream
      eff_read = p_nano_ble_stream->readBytes(p_i2s_array, i2s_buffer_size);
    } else {
      // Using readArray
      eff_read = p_i2s_buffer->readArray(p_i2s_array, i2s_buffer_size);
    }
    // if we did not get any valid data we provide silence
    if (eff_read < i2s_buffer_size) {
      memset(p_i2s_array, 0, i2s_buffer_size);
      // allow checking for underflows
      i2s_underflow_count++;
    }
    NRF_I2S->TXD.PTR = (uint32_t)p_i2s_array;
    NRF_I2S->EVENTS_TXPTRUPD = 0;
  }
}

void I2S_IRQRead(void) {
  // Handle Read
  if (NRF_I2S->EVENTS_RXPTRUPD == 1) {
    // reading from pins writing to buffer - overwrite oldest data on overflow
    p_i2s_buffer->writeArrayOverwrite(p_i2s_array, i2s_buffer_size);
    // switch buffer assuming that this is necessary like in the write case
    p_i2s_array = p_i2s_array == p_i2s_array_1 ? p_i2s_array_2 : p_i2s_array_1;
    NRF_I2S->RXD.PTR = (uint32_t)p_i2s_array;
    NRF_I2S->EVENTS_RXPTRUPD = 0;
  }
}

/**
 *  I2S Event handler
 */
void I2S_IRQHandler(void) {
  // prevent NPE
  if (p_i2s_buffer == nullptr || p_i2s_array == 0) {
    NRF_I2S->EVENTS_TXPTRUPD = 0;
    NRF_I2S->EVENTS_RXPTRUPD = 0;
    return;
  }

  I2S_IRQWrite();
  I2S_IRQRead();
}

/**
 * @brief Basic I2S API - for the Arduino Nano BLE Sense
 * See https://content.arduino.cc/assets/Nano_BLE_MCU-nRF52840_PS_v1.1.pdf
 * Douplex mode (RXTX_MODE) is currently not supported, but it should be quite
 * easy to implement.
 * @author Phil Schatzmann
 * @ingroup platform
 * @copyright GPLv3
 */
class I2SDriverNanoBLE {
  friend class I2SStream;

 public:
  I2SDriverNanoBLE() = default;

  /// Provides the default configuration
  I2SConfigStd defaultConfig(RxTxMode mode) {
    I2SConfigStd c(mode);
    return c;
  }
  /// Potentially updates the sample rate (if supported)
  bool setAudioInfo(AudioInfo) { return false; }

  /// starts the I2S with the default config in TX Mode
  bool begin(RxTxMode mode = TX_MODE) { return begin(defaultConfig(mode)); }

  /// starts the I2S
  bool begin(I2SConfigStd cfg) {
    TRACEI();
    cfg.logInfo();
    this->cfg = cfg;

    if (cfg.bits_per_sample == 32) {
      LOGE("32 bits not supported");
      return false;
    }

    if (!setupBuffers()) {
      LOGE("out of memory");
      return false;
    }

    // setup IRQ
    NVIC_SetVector(I2S_IRQn, (uint32_t)I2S_IRQHandler);
    NVIC_EnableIRQ(I2S_IRQn);

    if (!setupRxTx(cfg)) {
      return false;
    }
    setupClock(cfg);
    setupBitWidth(cfg);
    setupMode(cfg);
    setupPins(cfg);

    // TX_MODE is started with first write
    if (cfg.rx_tx_mode == RX_MODE || p_nano_ble_stream != nullptr) {
      startI2SActive();
    }

    return true;
  }

  int available() {
    if (cfg.rx_tx_mode == TX_MODE) return 0;
    return p_i2s_buffer->available();
  }

  int availableForWrite() {
    if (cfg.rx_tx_mode == RX_MODE) return 0;
    return max(i2s_buffer_size, p_i2s_buffer->availableForWrite());
  }

  /// stops the I2S
  void end() {
    LOGD(__func__);
    // stop task
    NRF_I2S->TASKS_START = 0;
    // disble I2S
    NRF_I2S->ENABLE = 0;

    releaseBuffers();

    is_active = false;
  }

  /// provides the actual configuration
  I2SConfigStd config() { return cfg; }

  /// writes the data to the I2S buffer
  size_t writeBytes(const void *src, size_t size_bytes) {
    size_t result = p_i2s_buffer->writeArray((uint8_t *)src, size_bytes);

    // activate I2S when the buffer is full
    if (!is_active && result < size_bytes) {
      startI2SActive();
    }
    return result;
  }

  /// reads the data from the I2S buffer
  size_t readBytes(void *dest, size_t size_bytes) {
    size_t result = p_i2s_buffer->readArray((uint8_t *)dest, size_bytes);
    return result;
  }

  /// alternative API which provides the data directly via a Stream
  void setStream(Stream &stream) { p_nano_ble_stream = &stream; }

  /// Deactivate alternative API: don't forget to call begin()
  void clearStream() { p_nano_ble_stream = nullptr; }

  void setBufferSize(int size) { i2s_buffer_size = size; }

 protected:
  I2SConfigStd cfg;
  bool is_active = false;

  /// setup TXEN or RXEN
  bool setupRxTx(I2SConfigStd cfg) {
    TRACED();
    switch (cfg.rx_tx_mode) {
      case TX_MODE:
        // Enable transmission
        NRF_I2S->CONFIG.TXEN =
            (I2S_CONFIG_TXEN_TXEN_Enabled << I2S_CONFIG_TXEN_TXEN_Pos);
        return true;
      case RX_MODE:
        // Enable reception
        NRF_I2S->CONFIG.RXEN =
            (I2S_CONFIG_RXEN_RXEN_Enabled << I2S_CONFIG_RXEN_RXEN_Pos);
        return true;
      default:
        LOGE("rx_tx_mode not supported");
        return false;
    }
  }

  /// setup MCKFREQ and RATIO
  void setupClock(I2SConfigStd cfg) {
    TRACED();

    // Enable MCK generator if in master mode
    if (cfg.is_master) {
      NRF_I2S->CONFIG.MCKEN =
          (I2S_CONFIG_MCKEN_MCKEN_Enabled << I2S_CONFIG_MCKEN_MCKEN_Pos);
    }

    // find closest frequency for requested sample_rate
    float freq_requested = cfg.sample_rate;  // * cfg.bits_per_sample ;
    float selected_freq = 0;
    for (auto freq : freq_table) {
      for (auto div : ratio_table) {
        float freq_value = freq.freq * 1000000 / div.ratio;
        if (abs(freq_value - freq_requested) <
            abs(selected_freq - freq_requested)) {
          // MCKFREQ
          NRF_I2S->CONFIG.MCKFREQ = freq.id << I2S_CONFIG_MCKFREQ_MCKFREQ_Pos;
          // Ratio
          NRF_I2S->CONFIG.RATIO = div.id << I2S_CONFIG_RATIO_RATIO_Pos;
          selected_freq = freq_value;
          LOGD("frequency requested %f vs %f", freq_requested, selected_freq);
        }
      }
    }
    LOGI("Frequency req. %f vs eff. %f", freq_requested, selected_freq);
  }

  /// setup SWIDTH
  void setupBitWidth(I2SConfigStd cfg) {
    TRACED();
    uint16_t swidth = I2S_CONFIG_SWIDTH_SWIDTH_16Bit;
    switch (cfg.bits_per_sample) {
      case 8:
        NRF_I2S->CONFIG.SWIDTH = I2S_CONFIG_SWIDTH_SWIDTH_8Bit
                                 << I2S_CONFIG_SWIDTH_SWIDTH_Pos;
        break;
      case 16:
        NRF_I2S->CONFIG.SWIDTH = I2S_CONFIG_SWIDTH_SWIDTH_16Bit
                                 << I2S_CONFIG_SWIDTH_SWIDTH_Pos;
        break;
      case 24:
        NRF_I2S->CONFIG.SWIDTH = I2S_CONFIG_SWIDTH_SWIDTH_24Bit
                                 << I2S_CONFIG_SWIDTH_SWIDTH_Pos;
        break;
      default:
        LOGE("Unsupported bit width: %d", cfg.bits_per_sample);
    }
  }

  /// setup format and align
  void setupMode(I2SConfigStd cfg) {
    TRACED();
    // setup mode
    switch (cfg.i2s_format) {
      case I2S_STD_FORMAT:
      case I2S_PHILIPS_FORMAT:
      case I2S_MSB_FORMAT:
      case I2S_LEFT_JUSTIFIED_FORMAT:
        NRF_I2S->CONFIG.FORMAT = I2S_CONFIG_FORMAT_FORMAT_I2S
                                 << I2S_CONFIG_FORMAT_FORMAT_Pos;
        NRF_I2S->CONFIG.ALIGN = I2S_CONFIG_ALIGN_ALIGN_Left
                                << I2S_CONFIG_ALIGN_ALIGN_Pos;
        ;
        break;
      case I2S_LSB_FORMAT:
      case I2S_RIGHT_JUSTIFIED_FORMAT:
        NRF_I2S->CONFIG.FORMAT = I2S_CONFIG_FORMAT_FORMAT_I2S
                                 << I2S_CONFIG_FORMAT_FORMAT_Pos;
        NRF_I2S->CONFIG.ALIGN = I2S_CONFIG_ALIGN_ALIGN_Right
                                << I2S_CONFIG_ALIGN_ALIGN_Pos;
        ;
        break;
      default:
        LOGW("i2s_format not supported");
    }
  }
#ifdef IS_ZEPHYR
  int digitalPinToPinName(int pin) {return pin;}
#endif
  /// Provides the arduino or unconverted pin name
  int getPinName(int pin) {
#if defined(USE_ALT_PIN_SUPPORT)
    return cfg.is_arduino_pin_numbers ? digitalPinToPinName(pin) : pin;
#else
    return digitalPinToPinName(pin);
#endif
  }

  /// setup pins
  void setupPins(I2SConfigStd cfg) {
    TRACED();

    // MCK
    if (cfg.is_master && cfg.pin_mck >= 0) {
      NRF_I2S->PSEL.MCK = getPinName(cfg.pin_mck) << I2S_PSEL_MCK_PIN_Pos;
    }
    // SCK - bit clock
    NRF_I2S->PSEL.SCK = getPinName(cfg.pin_bck) << I2S_PSEL_SCK_PIN_Pos;
    // LRCK
    NRF_I2S->PSEL.LRCK = getPinName(cfg.pin_ws) << I2S_PSEL_LRCK_PIN_Pos;
    // i2s Data Pins
    switch (cfg.rx_tx_mode) {
      case TX_MODE:
        NRF_I2S->PSEL.SDOUT = getPinName(cfg.pin_data)
                              << I2S_PSEL_SDOUT_PIN_Pos;
        break;
      case RX_MODE:
        NRF_I2S->PSEL.SDIN = getPinName(cfg.pin_data) << I2S_PSEL_SDIN_PIN_Pos;
        break;
      default:
        TRACEW();
    }
  }

  /// Determine the INTENSET value
  unsigned long getINTENSET() {
    unsigned long result = 0;
    switch (cfg.rx_tx_mode) {
      case TX_MODE:
        result = I2S_INTENSET_TXPTRUPD_Enabled << I2S_INTENSET_TXPTRUPD_Pos;
        break;
      case RX_MODE:
        result = I2S_INTENSET_RXPTRUPD_Enabled << I2S_INTENSET_RXPTRUPD_Pos;
        break;
      default:
        TRACEE();
    }
    return result;
  }

  /// Start IRQ and I2S
  void startI2SActive() {
    TRACED();
    // Use stereo
    NRF_I2S->CONFIG.CHANNELS = I2S_CONFIG_CHANNELS_CHANNELS_Stereo
                               << I2S_CONFIG_CHANNELS_CHANNELS_Pos;
    // Setup master or slave mode
    NRF_I2S->CONFIG.MODE =
        cfg.is_master ? I2S_CONFIG_MODE_MODE_MASTER << I2S_CONFIG_MODE_MODE_Pos
                      : I2S_CONFIG_MODE_MODE_Slave << I2S_CONFIG_MODE_MODE_Pos;

    // initial empty buffer
    NRF_I2S->TXD.PTR = (uint32_t)p_i2s_array;
    NRF_I2S->RXD.PTR = (uint32_t)p_i2s_array;
    // define copy size (always defined as number of 32 bits)
    NRF_I2S->RXTXD.MAXCNT = i2s_buffer_size / 4;

    NRF_I2S->INTENSET = getINTENSET();

    // ensble I2S
    NRF_I2S->ENABLE = 1;
    // start task
    NRF_I2S->TASKS_START = 1;

    is_active = true;
  }

  /// dynamic buffer management
  bool setupBuffers() {
    TRACED();
    i2s_buffer_size = cfg.buffer_size;

    if (p_i2s_array == nullptr) {
      p_i2s_array_1 = new uint8_t[i2s_buffer_size]{0};
      p_i2s_array_2 = new uint8_t[i2s_buffer_size]{0};
      p_i2s_array = p_i2s_array_1;
    } else {
      memset(p_i2s_array_1, 0, i2s_buffer_size);
      memset(p_i2s_array_2, 0, i2s_buffer_size);
    }

    // allocate buffer only when needed
    if (p_i2s_buffer == nullptr && p_nano_ble_stream == nullptr) {
      p_i2s_buffer = new NBuffer<uint8_t>(cfg.buffer_size, i2s_buffer_size);
    }

    // on stream option we only need to have the arrays allocated
    if (p_nano_ble_stream != nullptr) {
      return p_i2s_array_1 != nullptr && p_i2s_array_2 != nullptr;
    }
    return p_i2s_array_1 != nullptr && p_i2s_array_2 != nullptr &&
           p_i2s_buffer != nullptr;
  }

  /// Release buffers
  void releaseBuffers() {
    TRACED();
    i2s_buffer_size = 0;

    p_i2s_array = nullptr;
    delete p_i2s_array_1;
    p_i2s_array_1 = nullptr;
    delete p_i2s_array_2;
    p_i2s_array_2 = nullptr;

    delete p_i2s_buffer;
    p_i2s_buffer = nullptr;
  }
};

using I2SDriver = I2SDriverNanoBLE;

}  // namespace audio_tools

#endif