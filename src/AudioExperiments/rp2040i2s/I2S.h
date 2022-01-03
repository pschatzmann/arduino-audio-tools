#pragma once

#include <queue>
#include "i2s_config.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/pio.h"
#include "i2s_master_in.h"
#include "i2s_master_out.h"
#include "i2s_slave_in.h"

#ifdef ARDUINO_ARCH_MBED_RP2040
#include "mbed_hack.h"
#endif

namespace rp2040_i2s {

void *SelfI2SMasterOut = nullptr;
void *SelfI2SMasterIn = nullptr;
void *SelfI2SSlaveIn = nullptr;

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
  uint data_pin = DEFAULT_PICO_AUDIO_I2S_DATA_PIN;        

 protected:
  friend class I2SMasterOut;
  friend class I2SMasterIn;
  friend class I2SSlaveIn;
  friend class I2SClass;
  I2SOperation op_mode;
  PIO pio;
  uint8_t state_machine;
  uint8_t dma_channel;
  uint8_t dma_irq;
  int channels = 2;
  bool active = false;
  uint clock_pin = 27;  // pin bclk
  uint ws_pin = 28;  // pin bclk
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

/**
 * @brief PIO Managment - abstract class
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class I2SMasterBase {
 public:
  virtual boolean begin(IBuffer *buffer, AudioConfig *config);
  virtual void startCopy();
  virtual bool clockFromSampleRate(){return true;}
};

/**
 * @brief I2S output: Manage DMA data transfer from buffer to PIO
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class I2SMasterOut : public I2SMasterBase {
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
    gpio_set_function(config->clock_pin, func);
    gpio_set_function(config->ws_pin, func);

    pio_sm_claim(pio, sm);
    uint offset = pio_add_program(pio, &audio_i2s_master_out_program);
    I2S_LOGI("bits_per_sample: %d", config->bits_per_sample);
    pioInit(pio, sm, offset, config->data_pin, config->clock_pin,
            config->bits_per_sample);

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

  void pioInit(PIO pio, uint sm, uint offset, uint data_pin,
               uint clock_pin, int bits_per_sample) {
    pio_sm_config sm_config = audio_i2s_master_out_program_get_default_config(offset);

    sm_config_set_out_pins(&sm_config, data_pin, 1);
    sm_config_set_sideset_pins(&sm_config, clock_pin);
    sm_config_set_out_shift(&sm_config, false, true, 32);
    sm_config_set_fifo_join(&sm_config, PIO_FIFO_JOIN_TX);

    pio_sm_init(pio, sm, offset, &sm_config);

    pio_sm_set_consecutive_pindirs(pio, sm, data_pin, 3, true); // 3 pins output
    pio_sm_set_pins(pio, sm, 0);  // clear pins

    // we do not support 24 bits - we process them as 32 bits
    int loopMax = bits_per_sample == 24 ? 32 : bits_per_sample;

    pio_sm_exec(pio, sm, pio_encode_set(pio_y, loopMax - 2));
    pio_sm_exec(pio, sm,
                pio_encode_jmp(offset + audio_i2s_master_out_offset_entry_point));
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
 * @brief I2S input: Manage DMA data transfer from PIO to the buffer
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class I2SMasterIn : public I2SMasterBase {
 public:
  I2SMasterIn() { SelfI2SMasterIn = this; };

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
    gpio_set_function(config->clock_pin, func);
    gpio_set_function(config->ws_pin, func);

    pio_sm_claim(pio, sm);
    uint offset = pio_add_program(pio, &audio_i2s_master_in_program);
    I2S_LOGI("bits_per_sample: %d", config->bits_per_sample);
    pioInit(pio, sm, offset, config->data_pin, config->clock_pin,
            config->bits_per_sample);

    dma_channel_claim(dma_channel);
    dma_channel_config dma_config = dma_channel_get_default_config(dma_channel);
    uint dreq = pio_get_dreq(pio, sm, false);  // rx => false
    channel_config_set_dreq(&dma_config, dreq);
    channel_config_set_transfer_data_size(&dma_config, DMA_SIZE_32);
    dma_channel_configure(dma_channel, &dma_config,
                          NULL,           // dest
                          &pio->rxf[sm],  // src
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
    // get next empty buffer to fill with data
    p_actual_available_buffer = p_buffer->getFreeBuffer();
    if (p_actual_available_buffer == nullptr) {
      // we reuse the oldest filled audio data
      p_actual_available_buffer = p_buffer->getFilledBuffer();
      p_actual_available_buffer->audioByteCount = 0;
    }

    // transfer to PIO
    dma_channel_config cfg = dma_get_channel_config(dma_channel);
    channel_config_set_write_increment(&cfg, true);
    dma_channel_set_config(dma_channel, &cfg, false);
    dma_channel_transfer_to_buffer_now(dma_channel,
                                       p_actual_available_buffer->data,
                                       p_buffer->availableForWrite());
  }

 protected:
  bool audio_enabled = false;
  IBuffer *p_buffer = nullptr;
  AudioConfig *p_config = nullptr;
  I2SBufferEntry *p_actual_available_buffer = nullptr;

  void pioInit(PIO pio, uint sm, uint offset, uint data_pin,
               uint clock_pin, int bits_per_sample) {
    pio_sm_config sm_config = audio_i2s_master_in_program_get_default_config(offset);

    sm_config_set_in_pins(&sm_config, data_pin);
    sm_config_set_sideset_pins(&sm_config, clock_pin);
    sm_config_set_in_shift(&sm_config, false, true, 32);
    sm_config_set_fifo_join(&sm_config, PIO_FIFO_JOIN_RX);

    pio_sm_init(pio, sm, offset, &sm_config);

    pio_sm_set_consecutive_pindirs(pio, sm, clock_pin, 2, true); // 3 pins output
    pio_sm_set_consecutive_pindirs(pio, sm, data_pin, 1, false); // 3 pins input
    pio_sm_set_pins(pio, sm, 0);  // clear pins

    // we do not support 24 bits - we process them as 32 bits
    int loopMax = bits_per_sample == 24 ? 32 : bits_per_sample;

    pio_sm_exec(pio, sm, pio_encode_set(pio_y, loopMax - 2));
    pio_sm_exec(pio, sm,
                pio_encode_jmp(offset + audio_i2s_master_in_offset_entry_point));
  }

  // irq handler for DMA
  static void __isr __time_critical_func(dma_callback)() {
    static_cast<I2SMasterIn *>(SelfI2SMasterIn)->dmaCopyPIOToBuffer();
  }

  // DMA callback method: move data from buffer to PIO
  void dmaCopyPIOToBuffer() {
    uint dma_channel = p_config->dma_channel;
    uint dma_irq = p_config->dma_irq;
    if (dma_irqn_get_channel_status(dma_irq, dma_channel)) {
      dma_irqn_acknowledge_channel(dma_irq, dma_channel);
      I2S_LOGD(__PRETTY_FUNCTION__);
      // free the buffer we just finished
      if (p_actual_available_buffer != nullptr) {
        p_actual_available_buffer->audioByteCount =
            p_buffer->availableForWrite();
        p_buffer->addFilledBuffer(p_actual_available_buffer);
      }
      startCopy();

    } else {
      I2S_LOGE("invalid channel status");
    }
  }
};

/**
 * @brief I2S input: Manage DMA data transfer from PIO to the buffer using the DMA functionality.
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class I2SSlaveIn : public I2SMasterBase {
 public:
  I2SSlaveIn() { SelfI2SSlaveIn = this; };

  // we pull data full speed
  virtual bool clockFromSampleRate() override {return false;}

  // starts the processing
  boolean begin(IBuffer *buffer, AudioConfig *config) override {
    I2S_LOGI(__PRETTY_FUNCTION__);
    p_config = config;
    p_buffer = buffer;

    uint8_t sm = config->state_machine;
    uint8_t dma_channel = config->dma_channel;
    uint8_t dma_irq = config->dma_irq;
    PIO pio = config->pio;

    // interrupt on ws change
    gpio_set_irq_enabled_with_callback(config->ws_pin,
                                       GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
                                       true, ws_callback);

    // pio pins
    gpio_function func =
        (p_config->pio == pio0) ? GPIO_FUNC_PIO0 : GPIO_FUNC_PIO1;
    gpio_set_function(config->data_pin, func);
    gpio_set_function(config->clock_pin, func);

    pio_sm_claim(pio, sm);
    uint offset = pio_add_program(pio, &i2s_slave_in_program);
    I2S_LOGI("bits_per_sample: %d", config->bits_per_sample);
    pioInit(pio, sm, offset, config->data_pin, config->clock_pin,
            config->bits_per_sample);

    dma_channel_claim(dma_channel);
    dma_channel_config dma_config = dma_channel_get_default_config(dma_channel);
    uint dreq = pio_get_dreq(pio, sm, false);  // rx => false
    channel_config_set_dreq(&dma_config, dreq);
    channel_config_set_transfer_data_size(&dma_config, DMA_SIZE_32);
    dma_channel_configure(dma_channel, &dma_config,
                          NULL,           // dest
                          &pio->rxf[sm],  // src
                          0,              // count
                          false           // trigger
    );

    irq_add_shared_handler(DMA_IRQ_0 + dma_irq, dma_callback,
                           PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
    dma_irqn_set_channel_enabled(dma_irq, dma_channel, true);
    return true;
  }

  void startCopy() override {
    uint dma_channel = p_config->dma_channel;
    // get next empty buffer to fill with data
    p_actual_available_buffer = p_buffer->getFreeBuffer();
    if (p_actual_available_buffer == nullptr) {
      // we reuse the oldest filled audio data
      p_actual_available_buffer = p_buffer->getFilledBuffer();
      p_actual_available_buffer->audioByteCount = 0;
    }

    // transfer to PIO
    dma_channel_config cfg = dma_get_channel_config(dma_channel);
    channel_config_set_write_increment(&cfg, true);
    dma_channel_set_config(dma_channel, &cfg, false);
    dma_channel_transfer_to_buffer_now(dma_channel,
                                       p_actual_available_buffer->data,
                                       p_buffer->availableForWrite());
  }

 protected:
  bool audio_enabled = false;
  IBuffer *p_buffer = nullptr;
  AudioConfig *p_config = nullptr;
  I2SBufferEntry *p_actual_available_buffer = nullptr;
  int offset;

  void pioInit(PIO pio, uint sm, uint offset, uint data_pin,
               uint clock_pin, int bits_per_sample) {
    this->offset = offset;
    pio_sm_config sm_config = audio_i2s_master_in_program_get_default_config(offset);

    sm_config_set_in_pins(&sm_config, data_pin);
    sm_config_set_sideset_pins(&sm_config, clock_pin);
    sm_config_set_in_shift(&sm_config, false, true, 32);
    sm_config_set_fifo_join(&sm_config, PIO_FIFO_JOIN_RX);

    pio_sm_init(pio, sm, offset, &sm_config);

    pio_sm_set_consecutive_pindirs(pio, sm, data_pin, 2, false); // 2 pins input
    pio_sm_set_pins(pio, sm, 0);  // clear pins

    // we do not support 24 bits - we process them as 32 bits
    int loopMax = bits_per_sample == 24 ? 32 : bits_per_sample;

    pio_sm_exec(pio, sm, pio_encode_set(pio_y, loopMax - 2));
    pio_sm_exec(pio, sm,
                pio_encode_jmp(offset + audio_i2s_master_in_offset_entry_point));
  }

  // irq handler for DMA
  static void __isr __time_critical_func(dma_callback)() {
    static_cast<I2SSlaveIn*>(SelfI2SSlaveIn)->dmaCopyPIOToBuffer();
  }

  static void __isr __time_critical_func(ws_callback)(unsigned int pin, long unsigned int mask) {
    static_cast<I2SSlaveIn*>(SelfI2SSlaveIn)->wsChange(pin, mask);
  }

  /// the state of the ws pin was changing - so we push the data
  void wsChange(unsigned int pin, long unsigned int mask) {
    pio_sm_exec(p_config->pio, p_config->state_machine, pio_encode_jmp(offset + i2s_slave_in_offset_write));
  }

  // DMA callback method: move data from buffer to PIO
  void dmaCopyPIOToBuffer() {
    uint dma_channel = p_config->dma_channel;
    uint dma_irq = p_config->dma_irq;
    if (dma_irqn_get_channel_status(dma_irq, dma_channel)) {
      dma_irqn_acknowledge_channel(dma_irq, dma_channel);
      I2S_LOGD(__PRETTY_FUNCTION__);
      // free the buffer we just finished
      if (p_actual_available_buffer != nullptr) {
        switch(p_config->bits_per_sample){
          case 8:
            convert<int8_t>();
            break;
          case 16:
            convert<int16_t>();
            break;
          case 32:
            convert<int32_t>();
            break;
        }
        p_buffer->addFilledBuffer(p_actual_available_buffer);
      }
      startCopy();

    } else {
      I2S_LOGE("invalid channel status");
    }
  }

  /// Data is provided as int32 - we need to convert it to the expected size
  template<typename T>
  void convert(){
    int32_t *p_data = (int32_t *)p_actual_available_buffer;
    T *p_result = (T*)p_actual_available_buffer;
    size_t max = p_buffer->availableForWrite()/sizeof(int32_t);
    for (int j=0;j<max;j++){
      p_result[j] = (T) p_data[j];
    }
    p_actual_available_buffer->audioByteCount=max;
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
        addFreeBuffer(p_entry);
      }
    }
  }

  ~I2SBuffer() {
    I2SBufferEntry *tmp = getFreeBuffer();
    while (tmp!=nullptr) {
      delete tmp;
      tmp = getFreeBuffer();
    }
    tmp = getFilledBuffer();
    while (tmp!=nullptr) {
      delete tmp;
      tmp = getFreeBuffer();
    }
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
    I2SBufferEntry *result = freeBuffer.front();
    freeBuffer.pop();
    return result;
  }

  /// Make entry available again
  void addFreeBuffer(I2SBufferEntry *buffer) {
    if (buffer!=nullptr){
      buffer->audioByteCount = 0;
      freeBuffer.push(buffer);
    }
  }

  /// Provides the next buffer with audio data
  I2SBufferEntry *getFilledBuffer() {
    if (filledBuffer.size() == 0) {
      return nullptr;
    }
    I2SBufferEntry *result = filledBuffer.front();
    filledBuffer.pop();
    return result;
  }

  /// Add audio data to buffer
  void addFilledBuffer(I2SBufferEntry *buffer) {
    filledBuffer.push(buffer);
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
  std::queue<I2SBufferEntry *> freeBuffer;
  std::queue<I2SBufferEntry *> filledBuffer;
  size_t buffer_size;
  size_t buffer_count;
  size_t bytes_processed = 0;
  // DMA support
  I2SBufferEntry *p_actual_write_buffer = nullptr;
  I2SBufferEntry *p_actual_read_buffer = nullptr;
  size_t actual_read_pos = 0;
  int actual_read_open = 0;
};


#ifndef ARDUINO
class Stream {
  public: 
    /// Not supported
  virtual int available()= 0;
  virtual int read()= 0;
  virtual int peek() = 0;
  virtual size_t write(uint8_t byte) = 0;
  virtual int availableForWrite()  = 0;
  size_t write(uint8_t *data, size_t len) = 0;
  size_t readBytes(uint8_t *data, size_t len) = 0;
};
#endif

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
  bool setSampleRate(uint16_t sampleRate) {
    cfg.sample_rate = sampleRate;
    return updateSampleRate();
  }

  uint16_t sampleRate() { return cfg.sample_rate; }

  /// Defines the bits per samples (supported values: 8, 16, 32)
  bool setBitsPerSample(uint16_t bits) {
    if (bits == 8 || bits == 16 || bits == 32) {
      cfg.bits_per_sample = bits;
      return true;
    }
    return false;
  }

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
  bool setMaster(bool master) {
    cfg.is_master = master;
    return master == true;  // currently we only support master
  }

  bool isMaster() { return cfg.is_master; }

  /// Defines the data pin
  bool setPinData(int pin) {
    if (isActive()) {
      LOGE("setPinData failed because I2S is active");
      return false;
    }
    if (pin > 29) {
      LOGE("setPinData failed for pin %d - use a GPIO <=29", pin);
      return false;
    }
    cfg.data_pin = pin;
    cfg.clock_pin = pin+1;
    return true;
  }

  /// provides the data GPIO number of the data pin
  int pinData() { return cfg.data_pin; }

  /// Base clock GPIO pin (=data pin + 1)
  int pinClock() { return cfg.clock_pin; }

  /// Left right select GPIO pin (data pin + 2)
  int pinLR() { return cfg.clock_pin + 1; }

  /// Starts the processing
  void begin(AudioConfig config, I2SOperation mode) {
    cfg.is_master = config.is_master;
    cfg.sample_rate = config.sample_rate;
    cfg.bits_per_sample = config.bits_per_sample;
    cfg.buffer_count = config.buffer_count;
    cfg.buffer_size = config.buffer_size;
    cfg.data_pin = config.data_pin;              //
    cfg.clock_pin = config.clock_pin;  // pin bclk
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
            p_master = p_master_out = new I2SMasterOut();
          }
          break;
        case I2SRead:
          if (p_master_in == nullptr) {
            p_master = p_master_in = new I2SMasterIn();
          }
          break;
      }
    } else {
      switch (mode) {
        case I2SWrite:
          // not supported
          I2S_LOGE("Client mode does not support write");
          break;
        case I2SRead:
          // MasterWriteReader());
          break;
      }
    }

    if (p_master != nullptr) {
      p_master->begin(p_buffer, &cfg);
      if (p_master->clockFromSampleRate()) updateSampleRate();
      setActive(true);
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

  /// Checks if i2s has been started
  bool isActive() { return cfg.active; }

  /// Checks if i2s has been started
  operator bool() { return isActive(); }

 protected:
  IBuffer *p_buffer = nullptr;
  I2SMasterOut *p_master_out = nullptr;
  I2SMasterIn *p_master_in = nullptr;
  I2SMasterBase *p_master = nullptr;
  AudioConfig cfg;
  uint8_t byte_write_temp[8];
  uint8_t byte_write_count = 0;

  bool updateSampleRate() {
    I2S_LOGI(__PRETTY_FUNCTION__);
    uint32_t system_clock_frequency = clock_get_hz(clk_sys);
    // assert(system_clock_frequency < 0x40000000);
    float divider =
        0.25 * system_clock_frequency / (cfg.sample_rate * cfg.bits_per_sample);
    I2S_LOGI("sample_rate %d -> divider %f", cfg.sample_rate, divider);
    if (divider >= 1.0) {
      pio_sm_set_clkdiv(cfg.pio, cfg.state_machine, divider);
      return true;
    }

    I2S_LOGE("sample_rate not supported: %d", cfg.sample_rate);
    return false;
  }

  void setActive(bool active) {
    I2S_LOGI(__PRETTY_FUNCTION__);
    cfg.active = active;
    I2S_LOGI("active: %s", active ? "true" : "false");
    irq_set_enabled(DMA_IRQ_0 + cfg.dma_irq, active);

    if (active && p_master != nullptr) {
      p_master->startCopy();
    }

    pio_sm_set_enabled(cfg.pio, cfg.state_machine, active);
  }
};

}  // namespace rp2040_i2s
