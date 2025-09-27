#pragma once
#pragma once
#include "AudioTools/CoreAudio/AudioBasic/Collections/Allocator.h"
#include "AudioTools/CoreAudio/AudioLogger.h"
#include "AudioTools/CoreAudio/Buffers.h"
#include "AudioToolsConfig.h"

#ifdef ESP32
#include <freertos/stream_buffer.h>

#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#elif defined(__linux__)
#else
#include "FreeRTOS.h"
#include "ringbuf.h"
#include "stream_buffer.h"
#endif

namespace audio_tools {

/**
 * @brief FreeRTOS-based ring buffer implementation for audio data.
 * Supports ISR-safe operations and dynamic resizing.
 * @ingroup buffers
 * @ingroup concurrency
 * @author Phil Schatzmann
 * @copyright GPLv3 *
 * @tparam T Data type stored in the buffer
 */

template <typename T>
class RingBufferRTOS : public BaseBuffer<T> {
 public:
  /**
   * @brief Construct a RingBufferRTOS with given capacity (number of elements)
   * @param capacity Number of elements to allocate
   * @param allocator Allocator to use for buffer memory
   */
  RingBufferRTOS(size_t capacity, Allocator& allocator = DefaultAllocator)
      : _capacity(capacity * sizeof(T)),
        _handle(nullptr),
        allocator(allocator),
        p_data(nullptr) {
    resize(capacity);
  }

  /**
   * @brief Destructor: deletes the ring buffer and frees memory
   */
  ~RingBufferRTOS() {
    if (_handle) {
      vRingbufferDelete(_handle);
      _handle = nullptr;
    }
    if (p_data) {
      allocator.free(p_data);
      p_data = nullptr;
    }
  }

  /**
   * @brief Resize the buffer to a new capacity (number of elements)
   * @param capacity New number of elements
   */
  bool resize(size_t capacity) {
    if (_handle) {
      vRingbufferDelete(_handle);
      _handle = nullptr;
    }
    if (p_data) {
      allocator.free(p_data);
      p_data = nullptr;
    }
    if (capacity == 0) return true;
    _capacity = capacity * sizeof(T);
    // Allocate memory for static buffer
    p_data = (uint8_t*)allocator.allocate(_capacity);
    if (!p_data) {
      LOGE("Failed to allocate memory for ring buffer");
      return false;
    }
    // Create static ring buffer
    _handle = xRingbufferCreateStatic(_capacity, RINGBUF_TYPE_NOSPLIT, p_data,
                                      &static_ring_buffer);
    if (!_handle) {
      LOGE("Failed to create FreeRTOS static ring buffer");
      return false;
    }
    return true;
  }

  /**
   * @brief Write a single value to the buffer
   * @param data Value to write
   * @return true if successful
   */
  bool write(T data) override { return writeArray(&data, 1) == 1; }

  /**
   * @brief Write multiple values to the buffer
   * @param data Pointer to array of values
   * @param len Number of elements to write
   * @return Number of elements written
   */
  int writeArray(const T* data, int len) override {
    if (!_handle || !data) return 0;
    size_t bytes = len * sizeof(T);
    BaseType_t res;
    res = xRingbufferSend(_handle, (void*)data, bytes, 0);
    return (res == pdTRUE) ? len : 0;
  }

  /**
   * @brief Read a single value from the buffer
   * @param result Reference to store the value
   * @return true if successful
   */
  bool read(T& result) override { return readArray(&result, 1) == 1; }

  /**
   * @brief Read multiple values from the buffer
   * @param out Pointer to output array
   * @param len Number of elements to read
   * @return Number of elements read
   */
  int readArray(T* out, int len) override {
    if (!_handle || !out) return 0;
    size_t bytes = len * sizeof(T);
    void* item = xRingbufferReceive(_handle, &bytes, 0);
    if (item && bytes > 0) {
      int received = bytes / sizeof(T);
      memcpy(out, item, std::min(bytes, (size_t)len * sizeof(T)));
      vRingbufferReturnItem(_handle, item);
      return received;
    }
    return 0;
  }

  /**
   * @brief Peek the next value (not supported)
   * @param result Reference to store the value
   * @return Always false (not supported)
   */
  bool peek(T& result) override {
    // FreeRTOS ringbuffer does not support peeking
    return false;
  }

  /**
   * @brief Remove next len entries from the buffer
   * @param len Number of elements to remove
   * @return Number of elements removed
   */
  int clearArray(int len) override {
    T dummy[len];
    return readArray(dummy, len);
  }

  /**
   * @brief Clear the buffer (remove all entries)
   */
  void reset() override {
    if (_handle) {
      T data;
      while (read(data));
    }
  }

  /**
   * @brief Get address of start of buffer (not supported)
   * @return nullptr
   */
  T* address() override { return nullptr; }

  /**
   * @brief Number of entries available to read
   * @return Number of elements available
   */
  int available() override {
    if (!_handle) return 0;
    UBaseType_t uxFree = 0, uxRead = 0, uxWrite = 0, uxAcquire = 0,
                uxItemsWaiting = 0;
    vRingbufferGetInfo(_handle, &uxFree, &uxRead, &uxWrite, &uxAcquire,
                       &uxItemsWaiting);
    return uxItemsWaiting;
  }

  /**
   * @brief Number of entries available to write
   * @return Number of elements that can be written
   */
  int availableForWrite() override {
    if (!_handle) return 0;
    return xRingbufferGetCurFreeSize(_handle) / sizeof(T);
  }

  /**
   * @brief Get buffer capacity in elements
   * @return Capacity (number of elements)
   */
  size_t size() override { return _capacity / sizeof(T); }

  /**
   * @brief Write a single value to the buffer from ISR
   * @param data Value to write
   * @return true if successful
   */
  bool writeFromISR(T data) { return writeArrayFromISR(&data, 1) == 1; }

  /**
   * @brief Write multiple values to the buffer from ISR
   * @param data Pointer to array of values
   * @param len Number of elements to write
   * @return Number of elements written
   */
  int writeArrayFromISR(const T* data, int len) {
    if (!_handle || !data) return 0;
    size_t bytes = len * sizeof(T);
    task_woken = pdFALSE;
    BaseType_t res =
        xRingbufferSendFromISR(_handle, (void*)data, bytes, &task_woken);
    // Note: User should check task_woken after calling this method in an ISR.
    return (res == pdTRUE) ? len : 0;
  }

  /**
   * @brief Read a single value from the buffer from ISR
   * @param result Reference to store the value
   * @return true if successful
   */
  bool readFromISR(T& result) { return readArrayFromISR(&result, 1) == 1; }

  /**
   * @brief Read multiple values from the buffer from ISR
   * @param out Pointer to output array
   * @param len Number of elements to read
   * @return Number of elements read
   */
  int readArrayFromISR(T* out, int len) {
    if (!_handle || !out) return 0;
    size_t bytes = len * sizeof(T);
    void* item = xRingbufferReceiveFromISR(_handle, &bytes);
    if (item && bytes > 0) {
      int received = bytes / sizeof(T);
      memcpy(out, item, std::min(bytes, (size_t)len * sizeof(T)));
      vRingbufferReturnItem(_handle, item);
      return received;
    }
    return 0;
  }

  /**
   * @brief Informs if the last operation from ISR woke a higher priority task.
   * @return True if a higher priority task was woken
   */
  bool isTaskWoken() { return task_woken == pdTRUE; }

 protected:
  RingbufHandle_t _handle = nullptr;
  StaticRingbuffer_t static_ring_buffer;
  uint8_t* p_data = nullptr;
  size_t _capacity;
  Allocator& allocator;
  BaseType_t task_woken = pdFALSE;
};

}  // namespace audio_tools