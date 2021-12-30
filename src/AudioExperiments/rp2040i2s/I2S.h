#pragma once

#include <vector>

#include "Arduino.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/pio.h"
#include "i2s_master_out.h"
//#include "i2s_master_in.h"

#define DEFAULT_PICO_AUDIO_STATE_MACHINE 1
#define DEFAULT_PICO_AUDIO_DMA_CHANNEL 0
#define DEFAULT_PICO_AUDIO_PIO_NO 0
#define DEFAULT_PICO_AUDIO_I2S_DMA_IRQ 1

/**
 * @brief Logging Support
 *
 */
#define I2S_LOGE(...)         \
  Serial.print("E:");         \
  Serial.printf(__VA_ARGS__); \
  Serial.println()
#define I2S_LOGW(...)         \
  Serial.print("W:");         \
  Serial.printf(__VA_ARGS__); \
  Serial.println()
#define I2S_LOGI(...)         \
  Serial.print("I:");         \
  Serial.printf(__VA_ARGS__); \
  Serial.println()
//#define I2S_LOGD(...) Serial.print("D:"); Serial.printf(__VA_ARGS__);
// Serial.println()

//#define I2S_LOGI(...)
#define I2S_LOGD(...)

namespace rp2040_i2s {

/**
 * @brief Defines the I2S Operation as either Read or Write
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
enum I2SOperation { I2SWrite, I2SRead };

/**
 * @brief Audio Consiguration information
 * @author Phil Schatzmann
 * @copyright GPLv3
 *
 */

class AudioConfig {
 public:
  bool is_master = true;
  uint16_t sample_rate = 44100;
  uint16_t bits_per_sample = 16;
  uint16_t buffer_count = 10;
  uint16_t buffer_size = 512;
  uint data_pin = 28;        //
  uint clock_pin_base = 26;  // pin bclk

 protected:
  friend class I2SMasterOut;
  friend class I2SClass;
  I2SOperation op_mode;
  PIO pio;
  uint8_t state_machine;
  uint8_t dma_channel;
  uint8_t dma_irq;
  int channels = 2;
  bool active = false;
};

/**
 * @brief An individual entry into the I2S buffer
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class I2SBufferEntry {
 public:
  I2SBufferEntry() = default;
  I2SBufferEntry(uint8_t *data) { this->data = data; }
  uint8_t *data = nullptr;
  int audioByteCount = 0;  // effectively used bytes
};

/**
 * @brief Public Abstract Interface for I2S Buffer
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class IBuffer {
 public:
  virtual ~IBuffer() = default;
  /// Writes the data using the DMA
  virtual size_t write(uint8_t *data, size_t len);
  virtual size_t read(uint8_t *data, size_t len);
  virtual I2SBufferEntry *getFreeBuffer() = 0;
  virtual I2SBufferEntry *getFilledBuffer() = 0;
  virtual void addFreeBuffer(I2SBufferEntry *buffer) = 0;
  virtual void addFilledBuffer(I2SBufferEntry *buffer) = 0;
  virtual size_t availableForWrite() = 0;
  virtual void printStatistics() = 0;
};

void *SelfI2SMasterOut = nullptr;

/**
 * @brief Handle DMA data transfer from buffer to PIO
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class I2SMasterOut {
 public:
  I2SMasterOut() {
    SelfI2SMasterOut = this;
    empty.data = (uint8_t *)&empty64;
    empty.audioByteCount = sizeof(empty64);
  };

  // starts the processing
  boolean begin(IBuffer *buffer, AudioConfig *config) {
    I2S_LOGI(__PRETTY_FUNCTION__);
    p_config = config;
    p_buffer = buffer;

    uint8_t sm = config->state_machine;
    uint8_t dma_channel = config->dma_channel;
    uint8_t dma_irq = config->dma_irq;
    PIO pio = config->pio;

    gpio_function func =
        (p_config->pio == pio0) ? GPIO_FUNC_PIO0 : GPIO_FUNC_PIO1;
    gpio_set_function(config->data_pin, func);
    gpio_set_function(config->clock_pin_base, func);
    gpio_set_function(config->clock_pin_base + 1, func);

    pio_sm_claim(pio, sm);
    uint offset = pio_add_program(pio, &audio_i2s_program);
    audio_i2s_program_init(pio, sm, offset, config->data_pin,
                           config->clock_pin_base);

    dma_channel_claim(dma_channel);
    dma_channel_config dma_config = dma_channel_get_default_config(dma_channel);
    uint dreq = pio_get_dreq(pio, sm, true);  // tx = true
    channel_config_set_dreq(&dma_config, dreq);
    channel_config_set_transfer_data_size(&dma_config, DMA_SIZE_32);
    dma_channel_configure(dma_channel, &dma_config,
                          &pio->txf[sm],  // dest
                          NULL,           // src
                          0,              // count
                          false           // trigger
    );

    irq_add_shared_handler(DMA_IRQ_0 + dma_irq, dma_callback,
                           PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
    dma_irqn_set_channel_enabled(dma_irq, dma_channel, true);
    return true;
  }

  void startCopy() {
    uint dma_channel = p_config->dma_channel;
    // get next buffer with data
    p_actual_playing_buffer = p_buffer->getFilledBuffer();
    if (p_actual_playing_buffer == nullptr) {
      p_actual_playing_buffer = &empty;
    }

    // transfer to PIO
    dma_channel_config cfg = dma_get_channel_config(dma_channel);
    channel_config_set_read_increment(&cfg, true);
    dma_channel_set_config(dma_channel, &cfg, false);
    dma_channel_transfer_from_buffer_now(
        dma_channel, p_actual_playing_buffer->data,
        p_actual_playing_buffer->audioByteCount);
  }

 protected:
  bool audio_enabled = false;
  IBuffer *p_buffer = nullptr;
  AudioConfig *p_config = nullptr;
  I2SBufferEntry *p_actual_playing_buffer = nullptr;
  I2SBufferEntry empty;
  uint64_t empty64;

  // irq handler for DMA
  static void __isr __time_critical_func(dma_callback)() {
    static_cast<I2SMasterOut *>(SelfI2SMasterOut)->dmaCopyBufferToPIO();
  }

  // DMA callback method: move data from buffer to PIO
  void dmaCopyBufferToPIO() {
    uint dma_channel = p_config->dma_channel;
    uint dma_irq = p_config->dma_irq;
    if (dma_irqn_get_channel_status(dma_irq, dma_channel)) {
      dma_irqn_acknowledge_channel(dma_irq, dma_channel);
      I2S_LOGD(__PRETTY_FUNCTION__);
      // free the buffer we just finished
      if (p_actual_playing_buffer != nullptr &&
          p_actual_playing_buffer != &empty) {
        p_buffer->addFreeBuffer(p_actual_playing_buffer);
      }
      startCopy();

    } else {
      I2S_LOGE("invalid channel status");
    }
  }
};

/**
 * @brief I2SBuffer to write audio data to a buffer and read the audio data back
 * from the buffer.
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class I2SBuffer : public IBuffer {
 public:
  I2SBuffer(int count, int size) {
    I2S_LOGI(__PRETTY_FUNCTION__);
    buffer_size = size;
    buffer_count = count;
    // allocate buffers
    for (int j = 0; j < count; j++) {
      uint8_t *buffer = new uint8_t[size];
      if (buffer != nullptr) {
        I2SBufferEntry *p_entry = new I2SBufferEntry(buffer);
        freeBuffer.push_back(p_entry);
      }
    }
  }

  ~I2SBuffer() {
    for (int j = 0; j < freeBuffer.size(); j++) {
      delete freeBuffer[j];
    }
    filledBuffer.clear();
    for (int j = 0; j < filledBuffer.size(); j++) {
      delete filledBuffer[j];
    }
    filledBuffer.clear();
  }

  /// the max size of an individual buffer entry
  size_t size() { return buffer_size * buffer_count; }

  size_t availableForWrite() { return buffer_size; }

  /// Writes the data using the DMA
  size_t write(uint8_t *data, size_t len) override {
    I2S_LOGD(__PRETTY_FUNCTION__);
    if (len > this->availableForWrite()) {
      I2S_LOGE("I2SDefaultWriter: len too big: %d use max %d", len,
               this->availableForWrite());
      return 0;
    }

    if (p_actual_write_buffer != nullptr &&
        p_actual_write_buffer->audioByteCount + len >=
            this->availableForWrite()) {
      // requested write does not fit into the buffer
      this->addFilledBuffer(p_actual_write_buffer);
      p_actual_write_buffer = nullptr;
    }

    if (p_actual_write_buffer == nullptr) {
      p_actual_write_buffer = this->getFreeBuffer();
      if (p_actual_write_buffer == nullptr) {
        LOGI("I2SDefaultWriter: no free buffer");
        return 0;
      }
    }

    p_actual_write_buffer->audioByteCount += len;
    memmove(p_actual_write_buffer->data + p_actual_write_buffer->audioByteCount,
            data, len);
    // update statistics
    bytes_processed += len;
    return len;
  }

  /// reads data w/o DMA
  size_t read(uint8_t *data, size_t len) override {
    I2S_LOGI(__PRETTY_FUNCTION__);
    size_t result_len = 0;
    if (p_actual_read_buffer == nullptr) {
      // get next buffer
      actual_read_pos = 0;
      p_actual_read_buffer = this->getFilledBuffer();
      actual_read_open = p_actual_read_buffer->audioByteCount;
      result_len = p_actual_read_buffer->audioByteCount < len
                       ? p_actual_read_buffer->audioByteCount
                       : len;
    } else {
      // get next data from actual buffer
      result_len = min(len, actual_read_open);
    }

    memmove(data, p_actual_read_buffer, result_len);
    actual_read_pos += result_len;
    actual_read_open -= result_len;

    // make buffer available again
    if (actual_read_open <= 0) {
      this->addFreeBuffer(p_actual_read_buffer);
      p_actual_read_buffer = nullptr;
    }

    // update statistics
    bytes_processed += result_len;
    return result_len;
  }

  /// Get the next empty buffer entry
  I2SBufferEntry *getFreeBuffer() {
    if (freeBuffer.size() == 0) {
      return nullptr;
    }
    I2SBufferEntry *result = freeBuffer.back();
    freeBuffer.pop_back();
    return result;
  }

  /// Make entry available again
  void addFreeBuffer(I2SBufferEntry *buffer) {
    buffer->audioByteCount = 0;
    freeBuffer.push_back(buffer);
  }

  /// Add audio data to buffer
  void addFilledBuffer(I2SBufferEntry *buffer) {
    filledBuffer.push_back(buffer);
  }

  /// Provides the next buffer with audio data
  I2SBufferEntry *getFilledBuffer() {
    if (filledBuffer.size() == 0) {
      return nullptr;
    }
    I2SBufferEntry *result = filledBuffer.back();
    filledBuffer.pop_back();
    return result;
  }

  /// Print some statistics so that we can monitor the progress
  void printStatistics() {
    char msg[120];
    sprintf(msg, "freeBuffer: %d  - filledBuffer: %d - bytes_processed: %zu ",
            freeBuffer.size(), filledBuffer.size(), bytes_processed);
    bytes_processed = 0;
    Serial.println(msg);
  }

 protected:
  std::vector<I2SBufferEntry *> freeBuffer;
  std::vector<I2SBufferEntry *> filledBuffer;
  size_t buffer_size;
  size_t buffer_count;
  size_t bytes_processed = 0;
  // DMA support
  I2SBufferEntry *p_actual_write_buffer = nullptr;
  I2SBufferEntry *p_actual_read_buffer = nullptr;
  size_t actual_read_pos = 0;
  int actual_read_open = 0;
};

/**
 * @brief I2S for the RP2040 using the PIO
 * The default pins are: BCLK: GPIO26, LRCLK:GPIO27, DOUT: GPIO28. The LRCLK can
 * not be defined separately and is BCLK+1!
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class I2SClass : public Stream {
 public:
  /// Default constructor
  I2SClass(PIO pio = DEFAULT_PICO_AUDIO_PIO_NO == 0 ? pio0 : pio1,
           uint sm = DEFAULT_PICO_AUDIO_STATE_MACHINE,
           int dma_channel = DEFAULT_PICO_AUDIO_DMA_CHANNEL,
           int dma_irq = DEFAULT_PICO_AUDIO_I2S_DMA_IRQ) {
    cfg.pio = pio;
    cfg.state_machine = sm;
    cfg.dma_channel = dma_channel;
    cfg.dma_irq = dma_irq;
  };

  /// Destructor
  ~I2SClass() { end(); }

  /// Defines the sample rate
  void setSampleRate(uint16_t sampleRate) {
    cfg.sample_rate = sampleRate;
    updateSampleRate();
  }

  uint16_t sampleRate() { return cfg.sample_rate; }

  /// Defines the bits per samples (supported values: 8, 16, 32)
  void setBitsPerSample(uint16_t bits) { cfg.bits_per_sample = bits; }

  uint16_t bitsPerSample() { return cfg.bits_per_sample; }

  /// defines the I2S Buffer cunt and size (default is 10 * 512 bytes)
  void setBufferSize(int count, int size) {
    cfg.buffer_count = count;
    cfg.buffer_size = size;
    if (p_buffer != nullptr) {
      delete p_buffer;
      p_buffer = nullptr;
    }
  }

  /// Defines if the I2S is running as master (default is master)
  void setMaster(bool master) { cfg.is_master = master; }

  bool isMaster() { return cfg.is_master; }

  /// Defines the data pin
  void setPinData(int pin) { cfg.data_pin = pin; }

  /// provides the data GPIO number
  int pinData() { return cfg.data_pin; }

  /// Defines the clock pin (pin) and ws pin (=pin+1)
  void setPinClockBase(int pin) { cfg.clock_pin_base = pin; }

  /// Base clock GPIO pin
  int pinClock() { return cfg.clock_pin_base; }

  /// Left right select GPIO pin
  int pinLR() { return cfg.clock_pin_base + 1; }

  /// Starts the processing
  void begin(AudioConfig config, I2SOperation mode) {
    cfg.is_master = config.is_master;
    cfg.sample_rate = config.sample_rate;
    cfg.bits_per_sample = config.bits_per_sample;
    cfg.buffer_count = config.buffer_count;
    cfg.buffer_size = config.buffer_size;
    cfg.data_pin = config.data_pin;              //
    cfg.clock_pin_base = config.clock_pin_base;  // pin bclk
    begin(mode);
  }

  /// Starts the processing
  void begin(I2SOperation mode) {
    I2S_LOGI(__PRETTY_FUNCTION__);
    cfg.active = true;
    cfg.op_mode = mode;
    end();

    if (p_buffer == nullptr) {
      p_buffer = new I2SBuffer(cfg.buffer_count, cfg.buffer_size);
    }

    if (cfg.is_master) {
      switch (mode) {
        case I2SWrite:
          if (p_master_out == nullptr) {
            p_master_out = new I2SMasterOut();
            p_master_out->begin(p_buffer, &cfg);
          }
          updateSampleRate();
          setActive(true);
          break;
        case I2SRead:
          // setup(new PIOMasterIn(), new MasterReadWriter(pio, state_machine,
          // dma_channel), new I2SDefaultReader());
          break;
      }
    } else {
      switch (mode) {
        case I2SWrite:
          // not supported
          I2S_LOGE("Client mode does not support write");
          break;
        case I2SRead:
          // begin(new PIOSlaveIn(), new MasterWriteWriter(), new
          // MasterWriteReader());
          break;
      }
    }
  }

  /// Stops the processing and releases the memory
  virtual void end() {
    setActive(false);
    if (p_buffer != nullptr) {
      delete p_buffer;
      p_buffer = nullptr;
    }
    if (p_master_out != nullptr) {
      delete p_master_out;
      p_master_out = nullptr;
    }
  }

  /// Not supported
  virtual int available() override {
    return cfg.op_mode == I2SRead && p_buffer != nullptr
               ? p_buffer->availableForWrite()
               : -1;
  }
  /// Not supported
  virtual int read() override { return -1; }
  /// Not supported
  virtual int peek() override { return -1; }

  /// write a single byte (blocking)
  virtual size_t write(uint8_t byte) {
    byte_write_temp[byte_write_count++] = byte;
    if (byte_write_count == 8) {
      // blocking write
      int open = 8;
      int offset = 0;
      while (open > 0) {
        int written = write(byte_write_temp + offset, open);
        open -= written;
        offset += written;
      }
      byte_write_count = 0;
    }
    return 1;
  }

  virtual int availableForWrite() override {
    return cfg.op_mode == I2SWrite && p_buffer != nullptr
               ? p_buffer->availableForWrite()
               : -1;
  }

  /// Write data to the buffer (to the buffer)
  size_t write(uint8_t *data, size_t len) {
    if (p_buffer == nullptr) return 0;
    I2S_LOGD(__PRETTY_FUNCTION__);
    return p_buffer->write(data, len);
  }

  /// Read data with filled audio data (from the buffer)
  size_t readBytes(uint8_t *data, size_t len) {
    if (p_buffer == nullptr) return 0;
    I2S_LOGD(__PRETTY_FUNCTION__);
    return p_buffer->read(data, len);
  }

  // print some statistics
  void printStatistics() {
    if (p_buffer != nullptr) {
      p_buffer->printStatistics();
    }
  }

  /// Provides the default configuration values
  AudioConfig defaultConfig() {
    AudioConfig result;
    return result;
  }

  /// Provides a copy of the actual configuration
  AudioConfig config() { return cfg; }

 protected:
  IBuffer *p_buffer = nullptr;
  I2SMasterOut *p_master_out = nullptr;
  AudioConfig cfg;
  uint8_t byte_write_temp[8];
  uint8_t byte_write_count = 0;

  void updateSampleRate() {
    I2S_LOGI(__PRETTY_FUNCTION__);
    uint32_t system_clock_frequency = clock_get_hz(clk_sys);
    assert(system_clock_frequency < 0x40000000);
    int bytes = cfg.bits_per_sample / 8 * cfg.channels;
    uint32_t divider = system_clock_frequency * bytes /
                       cfg.sample_rate;  // avoid arithmetic overflow
    I2S_LOGI("sample_rate %d -> divider %u", cfg.sample_rate, divider);
    assert(divider < 0x1000000);
    pio_sm_set_clkdiv_int_frac(cfg.pio, cfg.state_machine, divider >> 8u,
                               divider & 0xffu);
  }

  void setActive(bool active) {
    I2S_LOGI(__PRETTY_FUNCTION__);
    cfg.active = active;
    I2S_LOGI("active: %s", active ? "true" : "false");
    irq_set_enabled(DMA_IRQ_0 + cfg.dma_irq, active);

    if (active) {
      p_master_out->startCopy();
    }

    pio_sm_set_enabled(cfg.pio, cfg.state_machine, active);
  }
};

}  // namespace rp2040_i2s
