#pragma once

#include "AudioTools/Concurrency/Zephyr/QueueZephyr.h"
#include "AudioTools/CoreAudio/Buffers.h"
#include "AudioToolsConfig.h"

#define DEFAULT_BUFFER_WAIT 1000

namespace audio_tools {

/**
 * @brief NBuffer using Zephyr queues for available and filled buffers.
 * 
 * @note Supported by all Zephyr platforms
 * 
 * @ingroup buffers
 * @ingroup concurrency
 */
template <typename T>
class SynchronizedNBufferZephyrT : public NBuffer<T> {
 public:
  SynchronizedNBufferZephyrT(int bufferSize, int bufferCount,
                             int writeMaxWait = DEFAULT_BUFFER_WAIT,
                             int readMaxWait = DEFAULT_BUFFER_WAIT) {
    read_max_wait = readMaxWait;
    write_max_wait = writeMaxWait;
    resize(bufferSize, bufferCount);
  }

  ~SynchronizedNBufferZephyrT() { cleanup(); }

  bool resize(size_t bufferSize, int bufferCount) {
    if (buffer_size == bufferSize && buffer_count == bufferCount) {
      return true;
    }

    max_size = bufferSize * bufferCount;
    NBuffer<T>::buffer_count = bufferCount;
    NBuffer<T>::buffer_size = bufferSize;

    cleanup();
    available_buffers.resize(bufferCount);
    filled_buffers.resize(bufferCount);

    setReadMaxWait(read_max_wait);
    setWriteMaxWait(write_max_wait);

    // setup buffers
    for (int j = 0; j < bufferCount; j++) {
      BaseBuffer<T> *tmp = new SingleBuffer<T>(bufferSize);
      if (tmp != nullptr) {
        available_buffers.enqueue(tmp);
      } else {
        LOGE("Not Enough Memory for buffer %d", j);
        return false;
      }
    }
    return true;
  }

  void setReadMaxWait(TickType_t ticks) {
    available_buffers.setReadMaxWait(ticks);
    filled_buffers.setReadMaxWait(ticks);
  }

  void setWriteMaxWait(TickType_t ticks) {
    available_buffers.setWriteMaxWait(ticks);
    filled_buffers.setWriteMaxWait(ticks);
  }

  size_t size() { return max_size; }

  int bufferCountFilled() { return filled_buffers.size(); }

  int bufferCountEmpty() { return available_buffers.size(); }

 protected:
  QueueZephyr<BaseBuffer<T> *> available_buffers{0, DEFAULT_BUFFER_WAIT, 0};
  QueueZephyr<BaseBuffer<T> *> filled_buffers{0, DEFAULT_BUFFER_WAIT, 0};
  size_t max_size = 0;
  size_t read_max_wait = DEFAULT_BUFFER_WAIT;
  size_t write_max_wait = DEFAULT_BUFFER_WAIT;
  int buffer_size = 0;
  int buffer_count = 0;

  /// Removes all allocated buffers
  void cleanup() {
    BaseBuffer<T> *buffer = nullptr;
    while (available_buffers.dequeue(buffer)) {
      delete buffer;
    }
    while (filled_buffers.dequeue(buffer)) {
      delete buffer;
    }
  }

  BaseBuffer<T> *getNextAvailableBuffer() {
    BaseBuffer<T> *result;
    return available_buffers.dequeue(result) ? result : nullptr;
  }

  bool addAvailableBuffer(BaseBuffer<T> *buffer) {
    return available_buffers.enqueue(buffer);
  }

  BaseBuffer<T> *getNextFilledBuffer() {
    BaseBuffer<T> *result;
    return filled_buffers.dequeue(result) ? result : nullptr;
  }

  bool addFilledBuffer(BaseBuffer<T> *buffer) {
    return filled_buffers.enqueue(buffer);
  }
};

/// @brief Zephyr synchronized buffer for managing multiple audio buffers
/// @ingroup buffers
using SynchronizedNBufferZephyr = SynchronizedNBufferZephyrT<uint8_t>;

/// @brief Compatibility typedef for RTOS-based synchronized N-buffer naming
using SynchronizedNBufferRTOS = SynchronizedNBufferZephyr;

/// @brief Default synchronized buffer alias
/// @ingroup buffers
using SynchronizedNBuffer = SynchronizedNBufferZephyr;

}  // namespace audio_tools
