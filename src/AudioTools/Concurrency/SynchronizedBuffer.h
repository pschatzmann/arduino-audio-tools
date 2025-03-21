#pragma once
#include "AudioToolsConfig.h"
#include "AudioTools/CoreAudio/AudioTypes.h"
#include "AudioTools/CoreAudio/Buffers.h"
#include "AudioTools/CoreAudio/AudioLogger.h"
#include "Mutex.h"
#include "LockGuard.h"

namespace audio_tools {

/**
 * @brief Wrapper class that can turn any Buffer into a thread save
 * implementation.
 * @ingroup buffers
 * @ingroup concurrency
 * @author Phil Schatzmann
 * @copyright GPLv3 *
 * @tparam T
 */
template <typename T> 
class SynchronizedBuffer : public BaseBuffer<T> {
public:
  SynchronizedBuffer(BaseBuffer<T> &buffer, MutexBase &mutex, bool syncAvailable=false) {
    p_buffer = &buffer;
    p_mutex = &mutex;
    is_sync_available = syncAvailable;
  }

  // reads a single value
  bool read(T &result) override {
    TRACED();
    LockGuard guard(p_mutex);
    return p_buffer->read(result);
  }

  // reads multiple values
  int readArray(T data[], int len) {
    TRACED();
    LockGuard guard(p_mutex);
    int lenResult = MIN(len, available());
    return p_buffer->readArray(data, lenResult);
  }

  int writeArray(const T data[], int len) {
    LOGD("%s: %d", LOG_METHOD, len);
    LockGuard guard(p_mutex);
    return p_buffer->writeArray(data, len);
  }

  // peeks the actual entry from the buffer
  bool peek(T &result) override {
    TRACED();
    LockGuard guard(p_mutex);
    return p_buffer->peek(result);
  }

  // checks if the buffer is full
  bool isFull() override { return p_buffer->isFull(); }

  bool isEmpty() { return available() == 0; }

  // write add an entry to the buffer
  bool write(T data) override {
    TRACED();
    LockGuard guard(p_mutex);
    return p_buffer->write(data);
  }

  // clears the buffer
  void reset() override {
    TRACED();
    LockGuard guard(p_mutex);
    p_buffer->reset();
  }

  // provides the number of entries that are available to read
  int available() override {
    TRACED();
    if (is_sync_available) LockGuard guard(p_mutex);
    return p_buffer->available();
  }

  // provides the number of entries that are available to write
  int availableForWrite() override {
    TRACED();
    if (is_sync_available) LockGuard guard(p_mutex);
    return p_buffer->availableForWrite();
  }

  // returns the address of the start of the physical read buffer
  T *address() override {
    TRACED();
    return p_buffer->address();
  }

  size_t size() {
    return p_buffer->size();
  }

protected:
  BaseBuffer<T> *p_buffer = nullptr;
  MutexBase *p_mutex = nullptr;
  bool is_sync_available = false;
};


} // namespace audio_tools

