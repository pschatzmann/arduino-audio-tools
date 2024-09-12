#pragma once

#ifdef STM32
#include "AudioI2S/I2SConfig.h"
#include "stm32-i2s.h"

#ifdef STM_I2S_PINS
#define IS_I2S_IMPLEMENTED 

namespace audio_tools {

/**
 * @brief Basic I2S API - for the STM32
 * Depends on https://github.com/pschatzmann/stm32f411-i2s
 * We provide a direct and a DMA implementation.
 * When using DMA, we just add a write and read buffer and pass some parameters
 * to the STM32 API! Alternatively we can define the input stream or the output.
 *
 * @ingroup platform
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class I2SDriverSTM32 {
  friend class I2SStream;

 public:
  /// Provides the default configuration
  I2SConfigStd defaultConfig(RxTxMode mode = TX_MODE) {
    I2SConfigStd c(mode);
    return c;
  }

  /// Potentially updates the sample rate (if supported)
  bool setAudioInfo(AudioInfo) { return false;}

  /// starts the DAC with the default config in TX Mode
  bool begin(RxTxMode mode = TX_MODE) {
    TRACED();
    return begin(defaultConfig(mode));
  }

  /// starts the DAC
  bool begin(I2SConfigStd cfg) {
    // TRACED();
    this->cfg = cfg;
    bool result = false;
    deleteBuffers();
    LOGI("buffer_size: %d", cfg.buffer_size);
    LOGI("buffer_count: %d", cfg.buffer_count);

    if (cfg.channels > 2 || cfg.channels <= 0) {
      LOGE("Channels not supported: %d", cfg.channels);
      return false;
    }

    setupDefaultI2SParameters();
    setupPins();
    result = use_dma ? startI2SDMA() : startI2S();
    this->active = result;
    return result;
  }

  /// stops the I2C and unistalls the driver
  void end() {
    // TRACED();
    i2s.end();
    deleteBuffers();
    active = false;
  }

  /// we assume the data is already available in the buffer
  int available() {
    if (!active) return 0;
    if (use_dma && p_rx_buffer == nullptr) return 0;
    return cfg.buffer_size;
  }

  /// We limit the write size to the buffer size
  int availableForWrite() {
    if (!active) return 0;
    if (use_dma && p_tx_buffer == nullptr) return 0;
    return cfg.buffer_size;
  }

  /// provides the actual configuration
  I2SConfigStd config() { return cfg; }

  /// blocking writes for the data to the I2S interface
  size_t writeBytes(const void *src, size_t size_bytes) {
    TRACED();
    size_t result = 0;
    if (!use_dma) {
      result = i2s.write((uint8_t *)src, size_bytes);
    } else {
      // if we have an input stream we do not need to fill the buffer
      if (p_dma_in != nullptr) {
        // by calling writeBytes we activate the automatic timeout handling
        // and expect further writes to continue the output
        last_write_ms = millis();
        result = size_bytes;
      } else {
        // fill buffer
        result = writeBytesDMA(src, size_bytes);
      }
    }
    return result;
  }

  size_t readBytes(void *dest, size_t size_bytes) {
    TRACED();
    if (!use_dma) {
      return i2s.readBytes((uint8_t *)dest, size_bytes);
    } else {
      if (cfg.channels == 2) {
        return p_rx_buffer->readArray((uint8_t *)dest, size_bytes);
      } else {
        return readBytesDMA(dest, size_bytes);
      }
    }
  }

  /// @brief Callback function used by
  /// https://github.com/pschatzmann/stm32f411-i2s
  static void writeFromReceive(uint8_t *buffer, uint16_t byteCount, void *ref) {
    I2SDriverSTM32 *self = (I2SDriverSTM32 *)ref;
    uint16_t written = 0;
    if (self->p_dma_out != nullptr)
      written = self->p_dma_out->write(buffer, byteCount);
    else
      written = self->p_rx_buffer->writeArray(buffer, byteCount);

    // check for overflow
    if (written != byteCount) {
      LOGW("Buffer overflow: written %d of %d", written, byteCount);
    }
  }

  /// @brief Callback function used by
  /// https://github.com/pschatzmann/stm32f411-i2s
  static void readToTransmit(uint8_t *buffer, uint16_t byteCount, void *ref) {
    I2SDriverSTM32 *self = (I2SDriverSTM32 *)ref;
    static size_t count = 0;
    size_t read = 0;
    if (self->p_dma_in != nullptr) {
      // stop reading if timout is relevant
      if (self->isWriteTimedOut()) {
        // we just provide silence
        read = byteCount;
      } else {
        // get data from stream
        read = self->p_dma_in->readBytes(buffer, byteCount);
      }
    } else {
      // get data from buffer
      if (self->stm32_write_active) {
        read = self->p_tx_buffer->readArray(buffer, byteCount);
      } 
    }
    memset(buffer+read, 0, byteCount-read);

    // check for underflow
    count++;
    if (read != byteCount) {
      LOGW("Buffer underflow at %lu: %d for %d", count, read, byteCount);
    }
  }

  /// Checks if timout has been activated and if so, if it is timed out
  bool isWriteTimedOut() {
    return last_write_ms != 0 && last_write_ms + 500 < millis();
  }

  /// activate DMA using a read and write buffer
  void setDMAActive(bool flag) { use_dma = flag; }

  /// activate DMA and defines the input
  void setDMAInputStream(Stream &in) {
    use_dma = true;
    p_dma_in = &in;
  }

  /// activate DMA and defines the output
  void setDMAOutput(Print &out) {
    use_dma = true;
    p_dma_out = &out;
  }

 protected:
  stm32_i2s::Stm32I2sClass i2s;
  stm32_i2s::I2SSettingsSTM32 i2s_stm32;
  I2SConfigStd cfg;
  bool active = false;
  bool result = true;
  BaseBuffer<uint8_t> *p_tx_buffer = nullptr;
  BaseBuffer<uint8_t> *p_rx_buffer = nullptr;
  volatile bool stm32_write_active = false;
  bool use_dma = true;
  Print *p_dma_out = nullptr;
  Stream *p_dma_in = nullptr;
  uint32_t last_write_ms = 0;

  size_t writeBytesDMA(const void *src, size_t size_bytes) {
    size_t result = 0;
    // fill the tx buffer
    int open = size_bytes;
    while (open > 0) {
      int actual_written = writeBytesExt(src, size_bytes);
      result += actual_written;
      open -= actual_written;
      if (open > 0) {
        stm32_write_active = true;
        //delay(1);
      }
    }

    // start output of data only when buffer has been filled
    if (!stm32_write_active && p_tx_buffer->availableForWrite() == 0) {
      stm32_write_active = true;
      LOGI("Buffer is full->starting i2s output");
    }

    return size_bytes;
  }

  size_t readBytesDMA(void *dest, size_t size_bytes) {
    // combine two channels to 1: so request double the amout
    int req_bytes = size_bytes * 2;
    uint8_t tmp[req_bytes];
    int16_t *tmp_16 = (int16_t *)tmp;
    int eff_bytes = p_rx_buffer->readArray((uint8_t *)tmp, req_bytes);
    // add 2 channels together
    int16_t *dest_16 = (int16_t *)dest;
    int16_t eff_samples = eff_bytes / 2;
    int16_t idx = 0;
    for (int j = 0; j < eff_samples; j += 2) {
      dest_16[idx++] = static_cast<float>(tmp_16[j]) + tmp_16[j + 1] / 2.0;
    }
    return eff_bytes / 2;
  }

  bool startI2S() {
    switch (cfg.rx_tx_mode) {
      case RX_MODE:
        result = i2s.begin(i2s_stm32, false, true);
        break;
      case TX_MODE:
        result = i2s.begin(i2s_stm32, true, false);
        break;
      case RXTX_MODE:
      default:
        result = i2s.begin(i2s_stm32, true, true);
        break;
    }
    return result;
  }

  bool startI2SDMA() {
    switch (cfg.rx_tx_mode) {
      case RX_MODE:
        if (use_dma && p_rx_buffer == nullptr)
          p_rx_buffer = allocateBuffer();
        result = i2s.beginReadDMA(i2s_stm32, writeFromReceive);
        break;
      case TX_MODE:
        stm32_write_active = false;
        if (use_dma && p_tx_buffer == nullptr)
          p_tx_buffer = allocateBuffer();
        result = i2s.beginWriteDMA(i2s_stm32, readToTransmit);
        break;

      case RXTX_MODE:
        if (use_dma) {
          stm32_write_active = false;
          if (p_rx_buffer == nullptr)
            p_rx_buffer = allocateBuffer();
          if (p_tx_buffer == nullptr)
            p_tx_buffer = allocateBuffer();
        }
        result = i2s.beginReadWriteDMA(
            i2s_stm32, readToTransmit, writeFromReceive);
        break;

      default:
        LOGE("Unsupported mode");
        return false;
    }
    return result;
  }

  uint32_t toDataFormat(int bits_per_sample) {
    switch (bits_per_sample) {
      case 16:
        return I2S_DATAFORMAT_16B;
      case 24:
        return I2S_DATAFORMAT_24B;
      case 32:
        return I2S_DATAFORMAT_32B;
    }
    return I2S_DATAFORMAT_16B;
  }

  void deleteBuffers() {
    if (p_rx_buffer != nullptr) {
      delete p_rx_buffer;
      p_rx_buffer = nullptr;
    }
    if (p_tx_buffer != nullptr) {
      delete p_tx_buffer;
      p_tx_buffer = nullptr;
    }
  }

  void setupDefaultI2SParameters() {
    i2s_stm32.sample_rate = getSampleRate(cfg);
    i2s_stm32.data_format = toDataFormat(cfg.bits_per_sample);
    i2s_stm32.mode = getMode(cfg);
    i2s_stm32.standard = getStandard(cfg);
    i2s_stm32.fullduplexmode = cfg.rx_tx_mode == RXTX_MODE
                                   ? I2S_FULLDUPLEXMODE_ENABLE
                                   : I2S_FULLDUPLEXMODE_DISABLE;
    i2s_stm32.hardware_config.buffer_size = cfg.buffer_size;
    // provide ourself as parameter to callback
    i2s_stm32.ref = this;
  }

  void setupPins(){
    if (cfg.pin_bck == -1 || cfg.pin_ws == -1 || cfg.pin_data == -1) {
      LOGW("pins ignored: used from stm32-i2s");
    } else {
      LOGI("setting up pins for stm32-i2s");
      i2s_stm32.hardware_config.pins[0].function = stm32_i2s::mclk;
      i2s_stm32.hardware_config.pins[0].pin = digitalPinToPinName(cfg.pin_mck);
      i2s_stm32.hardware_config.pins[0].altFunction = cfg.pin_alt_function;

      i2s_stm32.hardware_config.pins[1].function = stm32_i2s::bck;
      i2s_stm32.hardware_config.pins[1].pin = digitalPinToPinName(cfg.pin_bck);
      i2s_stm32.hardware_config.pins[1].altFunction = cfg.pin_alt_function;

      i2s_stm32.hardware_config.pins[2].function = stm32_i2s::ws;
      i2s_stm32.hardware_config.pins[2].pin = digitalPinToPinName(cfg.pin_ws);
      i2s_stm32.hardware_config.pins[2].altFunction = cfg.pin_alt_function;

      switch (cfg.rx_tx_mode) {
        case TX_MODE:
          i2s_stm32.hardware_config.pins[3].function = stm32_i2s::data_out;
          i2s_stm32.hardware_config.pins[3].pin = digitalPinToPinName(cfg.pin_data);
          i2s_stm32.hardware_config.pins[3].altFunction = cfg.pin_alt_function;
          break;
        case RX_MODE:
          i2s_stm32.hardware_config.pins[4].function = stm32_i2s::data_in;
          i2s_stm32.hardware_config.pins[4].pin = digitalPinToPinName(cfg.pin_data);
          i2s_stm32.hardware_config.pins[4].altFunction = cfg.pin_alt_function;
          break;
        case RXTX_MODE:
          i2s_stm32.hardware_config.pins[3].function = stm32_i2s::data_out;
          i2s_stm32.hardware_config.pins[3].pin = digitalPinToPinName(cfg.pin_data);
          i2s_stm32.hardware_config.pins[3].altFunction = cfg.pin_alt_function;

          i2s_stm32.hardware_config.pins[4].function = stm32_i2s::data_in;
          i2s_stm32.hardware_config.pins[4].pin = digitalPinToPinName(cfg.pin_data);
          i2s_stm32.hardware_config.pins[4].altFunction = cfg.pin_alt_function;
          break;
      };

    }
  }

  uint32_t getMode(I2SConfigStd &cfg) {
    if (cfg.is_master) {
      switch (cfg.rx_tx_mode) {
        case RX_MODE:
          return I2S_MODE_MASTER_RX;
        case TX_MODE:
          return I2S_MODE_MASTER_TX;
        default:
          LOGE("RXTX_MODE not supported");
          return I2S_MODE_MASTER_TX;
      }
    } else {
      switch (cfg.rx_tx_mode) {
        case RX_MODE:
          return I2S_MODE_SLAVE_RX;
        case TX_MODE:
          return I2S_MODE_SLAVE_TX;
        default:
          LOGE("RXTX_MODE not supported");
          return I2S_MODE_SLAVE_TX;
      }
    }
  }

  uint32_t getStandard(I2SConfigStd &cfg) {
    uint32_t result;
    switch (cfg.i2s_format) {
      case I2S_PHILIPS_FORMAT:
        return I2S_STANDARD_PHILIPS;
      case I2S_STD_FORMAT:
      case I2S_LSB_FORMAT:
      case I2S_RIGHT_JUSTIFIED_FORMAT:
        return I2S_STANDARD_MSB;
      case I2S_MSB_FORMAT:
      case I2S_LEFT_JUSTIFIED_FORMAT:
        return I2S_STANDARD_LSB;
    }
    return I2S_STANDARD_PHILIPS;
  }

  uint32_t getSampleRate(I2SConfigStd &cfg) {
    switch (cfg.sample_rate) {
      case I2S_AUDIOFREQ_192K:
      case I2S_AUDIOFREQ_96K:
      case I2S_AUDIOFREQ_48K:
      case I2S_AUDIOFREQ_44K:
      case I2S_AUDIOFREQ_32K:
      case I2S_AUDIOFREQ_22K:
      case I2S_AUDIOFREQ_16K:
      case I2S_AUDIOFREQ_11K:
      case I2S_AUDIOFREQ_8K:
        return cfg.sample_rate;
      default:
        LOGE("Unsupported sample rate: %u", cfg.sample_rate);
        return cfg.sample_rate;
    }
  }

  size_t writeBytesExt(const void *src, size_t size_bytes) {
    size_t result = 0;
    if (cfg.channels == 2) {
      result = p_tx_buffer->writeArray((uint8_t *)src, size_bytes);
    } else {
      // write each sample 2 times
      int samples = size_bytes / 2;
      int16_t *src_16 = (int16_t *)src;
      int16_t tmp[2];
      int result = 0;
      for (int j = 0; j < samples; j++) {
        tmp[0] = src_16[j];
        tmp[1] = src_16[j];
        if (p_tx_buffer->availableForWrite() >= 4) {
          p_tx_buffer->writeArray((uint8_t *)tmp, 4);
          result = j * 2;
        } else {
          // abort if the buffer is full
          break;
        }
      }
    }
    LOGD("writeBytesExt: %u", result)
    return result;
  }

  BaseBuffer<uint8_t>* allocateBuffer() {
      //return new RingBuffer<uint8_t>(cfg.buffer_size * cfg.buffer_count);
      return new NBuffer<uint8_t>(cfg.buffer_size, cfg.buffer_count);
  }
};

using I2SDriver = I2SDriverSTM32;

}  // namespace audio_tools

#endif
#endif
