#pragma once
#include "AudioToolsConfig.h"
#include "AudioTools/CoreAudio/AudioLogger.h"
#include "AudioTools/CoreAudio/Buffers.h"
#include "AudioTools/CoreAudio/AudioBasic/Collections/Allocator.h"
#include "AudioTools/Concurrency/Mutex.h"
#include "AudioTools/Concurrency/LockGuard.h"

#ifndef AUDIO_TOOLS_RTOS_TICK_TYPES_DEFINED
#define AUDIO_TOOLS_RTOS_TICK_TYPES_DEFINED
using TickType_t = uint32_t;
using BaseType_t = int;
#ifndef portMAX_DELAY
#define portMAX_DELAY UINT32_MAX
#endif
#ifndef pdMS_TO_TICKS
#define pdMS_TO_TICKS(ms) (ms)
#endif
#ifndef pdTRUE
#define pdTRUE 1
#endif
#ifndef pdFALSE
#define pdFALSE 0
#endif
#endif

namespace audio_tools {

/**
 * @brief Buffer implementation which provides the same blocking, thread-safe
 * semantics as BufferRTOS (ESP32/STM32) and BufferZephyr, for platforms that
 * have no native blocking stream buffer primitive - built on a
 * SpinLock-protected RingBuffer plus a delay(1) polling wait loop.
 *
 * @note Supported on any platform providing MutexBase, delay() and millis()
 * (e.g. Desktop, RP2040 without FreeRTOS)
 *
 * @ingroup buffers
 * @ingroup concurrency
 * @author Phil Schatzmann
 * @copyright GPLv3
 * @tparam T
 */
template <typename T>
class BufferBlocking : public BaseBuffer<T> {
 public:
  BufferBlocking(size_t streamBufferSize, size_t xTriggerLevel = 1,
                 TickType_t writeMaxWait = portMAX_DELAY,
                 TickType_t readMaxWait = portMAX_DELAY,
                 Allocator &allocator = DefaultAllocator)
      : buffer(streamBufferSize, allocator) {
    (void)xTriggerLevel;  // no coalescing support: every write is visible immediately
    readWait = readMaxWait;
    writeWait = writeMaxWait;
  }

  /// Re-Allocates the memory of the ring buffer
  bool resize(size_t size) {
    LockGuard guard(mutex);
    return buffer.resize(size);
  }

  void setReadMaxWait(TickType_t ticks) { readWait = ticks; }

  void setWriteMaxWait(TickType_t ticks) { writeWait = ticks; }

  // no ISR context on the supported platforms: kept for BufferRTOS API parity
  void setWriteFromISR(bool active) {}
  void setReadFromISR(bool active) {}

  // reads a single value
  bool read(T &result) override { return readArray(&result, 1) == 1; }

  // reads multiple values, blocking up to readWait ms until data is available
  int readArray(T data[], int len) {
    if (data == nullptr || len <= 0) return 0;
    uint32_t start = millis();
    int result = 0;
    while (result == 0) {
      {
        LockGuard guard(mutex);
        result = buffer.readArray(data, len);
      }
      if (result > 0 || !isTimeLeft(start, readWait)) break;
      delay(1);
    }
    return result;
  }

  // writes multiple values, blocking up to writeWait ms until space is available
  int writeArray(const T data[], int len) {
    if (data == nullptr || len <= 0) return 0;
    uint32_t start = millis();
    int result = 0;
    while (result == 0) {
      {
        LockGuard guard(mutex);
        result = buffer.writeArray(data, len);
      }
      if (result > 0 || !isTimeLeft(start, writeWait)) break;
      delay(1);
    }
    return result;
  }

  // peeks the actual entry from the buffer
  bool peek(T &result) override {
    LockGuard guard(mutex);
    return buffer.peek(result);
  }

  // checks if the buffer is full
  bool isFull() override {
    LockGuard guard(mutex);
    return buffer.isFull();
  }

  bool isEmpty() {
    LockGuard guard(mutex);
    return buffer.isEmpty();
  }

  // write add an entry to the buffer
  bool write(T data) override { return writeArray(&data, 1) == 1; }

  // clears the buffer
  void reset() override {
    LockGuard guard(mutex);
    buffer.reset();
  }

  // provides the number of entries that are available to read
  int available() override {
    LockGuard guard(mutex);
    return buffer.available();
  }

  // provides the number of entries that are available to write
  int availableForWrite() override {
    LockGuard guard(mutex);
    return buffer.availableForWrite();
  }

  // returns the address of the start of the physical read buffer
  T *address() override {
    LOGE("address() not implemented");
    return nullptr;
  }

  size_t size() { return buffer.size(); }

  operator bool() { return size() > 0; }

 protected:
  RingBuffer<T> buffer;
  SpinLock mutex;
  TickType_t readWait = portMAX_DELAY;
  TickType_t writeWait = portMAX_DELAY;

  bool isTimeLeft(uint32_t start, TickType_t maxWait) {
    if (maxWait == portMAX_DELAY) return true;
    return millis() - start < maxWait;
  }
};

/// @brief Template alias so code written against BufferRTOS (ESP32/STM32
/// FreeRTOS naming) also builds on platforms without FreeRTOS.
/// @ingroup concurrency
template <class T>
using BufferRTOS = BufferBlocking<T>;

/// @brief Template alias for a synchronized buffer, matching the RTOS naming.
/// @ingroup concurrency
template <class T>
using SynchronizedBufferRTOS = BufferBlocking<T>;

/// @brief Default thread-safe, blocking stream buffer for this platform.
/// @ingroup concurrency
template <class T>
using StreamBuffer = BufferBlocking<T>;

}  // namespace audio_tools
