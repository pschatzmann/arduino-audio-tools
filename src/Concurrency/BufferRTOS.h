#pragma once
#include "AudioConfig.h"
#include "AudioTools/AudioLogger.h"
#include "AudioBasic/Collections/Allocator.h"

#ifdef ESP32
#include <freertos/stream_buffer.h>

#include "freertos/FreeRTOS.h"
#else
#include "FreeRTOS.h"
#include "stream_buffer.h"
#endif

namespace audio_tools {

// #if ESP_IDF_VERSION_MAJOR >= 4

/**
 * @brief Buffer implementation which is using a FreeRTOS StreamBuffer. The
 * default allocator uses psram is available.
 * @ingroup buffers
 * @author Phil Schatzmann
 * @copyright GPLv3 *
 * @tparam T
 */
template <typename T>
class BufferRTOS : public BaseBuffer<T> {
 public:
  BufferRTOS(size_t xStreamBufferSizeBytes, size_t xTriggerLevel = 1,
             TickType_t writeMaxWait = portMAX_DELAY,
             TickType_t readMaxWait = portMAX_DELAY)
      : BaseBuffer<T>() {
    readWait = readMaxWait;
    writeWait = writeMaxWait;
    current_size = xStreamBufferSizeBytes;
    trigger_level = xTriggerLevel;

    if (current_size > 0) {
      setup();
    }
  }

  ~BufferRTOS() { end(); }

  /// Re-Allocats the memory and the queue
  void resize(size_t size) {
    if (current_size != size) {
      end();
      current_size = size;
      setup();
    }
  }

  void setReadMaxWait(TickType_t ticks) { readWait = ticks; }

  void setWriteMaxWait(TickType_t ticks) { writeWait = ticks; }

  void setWriteFromISR(bool active) { write_from_isr = active; }

  void setReadFromISR(bool active) { read_from_isr = active; }

  // reads a single value
  T read() override {
    T data = 0;
    readArray(&data, sizeof(T));
    return data;
  }

  // reads multiple values
  int readArray(T data[], int len) {
    if (read_from_isr) {
      xHigherPriorityTaskWoken = pdFALSE;
      int result = xStreamBufferReceiveFromISR(xStreamBuffer, (void *)data,
                                               sizeof(T) * len,
                                               &xHigherPriorityTaskWoken);
#ifdef ESP32X
      portYIELD_FROM_ISR();
#else
      portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
#endif
      return result;
    } else {
      return xStreamBufferReceive(xStreamBuffer, (void *)data, sizeof(T) * len,
                                  readWait);
    }
  }

  int writeArray(const T data[], int len) {
    LOGD("%s: %d", LOG_METHOD, len);
    if (write_from_isr) {
      xHigherPriorityTaskWoken = pdFALSE;
      int result =
          xStreamBufferSendFromISR(xStreamBuffer, (void *)data, sizeof(T) * len,
                                   &xHigherPriorityTaskWoken);
#ifdef ESP32X
      portYIELD_FROM_ISR();
#else
      portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
#endif
      return result;
    } else {
      return xStreamBufferSend(xStreamBuffer, (void *)data, sizeof(T) * len,
                               writeWait);
    }
  }

  // peeks the actual entry from the buffer
  T peek() override {
    LOGE("peek not implmented");
    return 0;
  }

  // checks if the buffer is full
  bool isFull() override {
    return xStreamBufferIsFull(xStreamBuffer) == pdTRUE;
  }

  bool isEmpty() { return xStreamBufferIsEmpty(xStreamBuffer) == pdTRUE; }

  // write add an entry to the buffer
  bool write(T data) override {
    int len = sizeof(T);
    return writeArray(&data, len) == len;
  }

  // clears the buffer
  void reset() override { xStreamBufferReset(xStreamBuffer); }

  // provides the number of entries that are available to read
  int available() override {
    return xStreamBufferBytesAvailable(xStreamBuffer);
  }

  // provides the number of entries that are available to write
  int availableForWrite() override {
    return xStreamBufferSpacesAvailable(xStreamBuffer);
  }

  // returns the address of the start of the physical read buffer
  T *address() override {
    LOGE("address() not implemented");
    return nullptr;
  }

  size_t size() { return current_size; }

 protected:
  StreamBufferHandle_t xStreamBuffer = nullptr;
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;  // Initialised to pdFALSE.
  int readWait = portMAX_DELAY;
  int writeWait = portMAX_DELAY;
  bool read_from_isr = false;
  bool write_from_isr = false;
  size_t current_size = 0;
  size_t trigger_level = 0;

  /// The allocation has been postponed to be done here, so that we can e.g. use
  /// psram
  bool setup() {
    if (current_size == 0) return true;

    // create stream buffer if necessary
    if (xStreamBuffer == nullptr) {
      xStreamBuffer = xStreamBufferCreate(current_size, trigger_level);
    }
    if (xStreamBuffer == nullptr) return false;
    // make sure that the data is empty
    reset();
    return true;
  }

  /// Release resurces: call resize to restart again
  void end() {
    if (xStreamBuffer != nullptr) vStreamBufferDelete(xStreamBuffer);
    current_size = 0;
    xStreamBuffer = nullptr;
  }
};
// #endif // ESP_IDF_VERSION_MAJOR >= 4

template <class T>
using SynchronizedBufferRTOS = BufferRTOS<T>;

}  // namespace audio_tools