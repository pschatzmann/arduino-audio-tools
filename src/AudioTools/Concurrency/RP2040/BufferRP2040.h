#pragma once

#ifndef ARDUINO_ARCH_RP2040
# error "Unsupported architecture"
#endif

#include "AudioConfig.h"
#include "AudioTools/CoreAudio/AudioLogger.h"
#include "AudioTools/CoreAudio/Buffers.h"

namespace audio_tools {

// #if ESP_IDF_VERSION_MAJOR >= 4

/**
 * @brief Buffer implementation which is based on a RP2040 queue. This
 * class is intended to be used to exchange data between the 2 different
 * cores. Multi-core and IRQ safe queue implementation!
 * 
 * In order to increase the efficiency we to not enqueue individual items
 * but write them into a temporary buffer of bufferSize and write this
 * array to the queue when it is full.
 * 
 * @ingroup buffers
 * @ingroup concurrency
 * @author Phil Schatzmann
 * @copyright GPLv3 *
 * @tparam T
 */
template <typename T>
class BufferRP2040T : public BaseBuffer<T> {
 public:
  BufferRP2040T(int bufferCount) : BaseBuffer<T>() {
    buffer_size = 1;
    buffer_size_bytes = sizeof(T);
    buffer_size_req_bytes = buffer_size_bytes * bufferCount;
  }

  BufferRP2040T(size_t bufferSize, int bufferCount) : BaseBuffer<T>() {
    buffer_size = bufferSize;
    buffer_size_bytes = bufferSize * sizeof(T);
    buffer_size_req_bytes = buffer_size_bytes * bufferCount;
  }

  ~BufferRP2040T() { reset(); }

  /// Re-Allocats the memory and the queue (size is in entries)
  bool resize(size_t size) {
    int req_bytes = size * sizeof(T);
    if (buffer_size_alloc_bytes < req_bytes) {
      LOGI("resize %d -> %d", buffer_size_alloc_bytes / sizeof(T), size);
      assert(buffer_size_bytes > 0);
      write_buffer.resize(buffer_size);
      read_buffer.resize(buffer_size * 2);
      // create new queu
      if (req_bytes > buffer_size_alloc_bytes) {
        // release existing queue
        if (buffer_size_alloc_bytes > 0) {
          queue_free(&queue);
        }

        int count = req_bytes / buffer_size_bytes;
        LOGI("queue_init(size:%d, count:%d)", buffer_size_bytes, count);
        queue_init(&queue, buffer_size_bytes, count);
        buffer_size_alloc_bytes = req_bytes;
      }
    }
    return true;
  }

  // reads a single value
  T read() {
    T data = 0;
    readArray(&data, 1);
    return data;
  }

  // peeks the actual entry from the buffer
  T peek() {
    LOGE("peek not implmented");
    return 0;
  }

  // reads multiple values
  int readArray(T data[], int len) override {
    LOGD("readArray: %d", len);
    // handle unalloc;ated queue
    if (buffer_size_alloc_bytes == 0) return 0;

    // blocking read
    while (is_blocking_read && read_buffer.available() + available() < len) 
      delay(1);

    // fill read buffer if necessary
    while (read_buffer.availableForWrite() >= buffer_size) {
      LOGD("reading %d %d ", buffer_size, read_buffer.availableForWrite());
      T tmp[buffer_size];
      if (queue_try_remove(&queue, tmp)){
        LOGD("queue_try_remove -> success");
        read_buffer.writeArray(tmp, buffer_size);      
      } else {
        LOGD("queue_try_remove -> failed");
        break;
      }
    }
    LOGD("read_buffer.available: %d, availableForWrite: %d ", read_buffer.available(), read_buffer.availableForWrite());
    int result = read_buffer.readArray(data, len);
    LOGD("=> readArray: %d -> %d", len, result);
    return result;
  }

  int writeArray(const T data[], int len) override {
    LOGD("writeArray: %d", len);
    int result = 0;
    // make sure that we have the data allocated
    resize(buffer_size_req_bytes / sizeof(T));

    if (is_blocking_write) {
      result = writeBlocking(data, len);
    } else {
      result = writeNonBlocking(data, len);
    }

    return result;
  }

  // checks if the buffer is full
  bool isFull() override {
    if (buffer_size_alloc_bytes == 0) return false;
    return queue_is_full(&queue);
  }

  bool isEmpty() {
    if (buffer_size_alloc_bytes == 0) return true;
    return queue_is_empty(&queue);
  }

  // write add an entry to the buffer
  bool write(T data) override { return writeArray(&data, 1) == 1; }

  // clears the buffer
  void reset() override {
    queue_free(&queue);
    buffer_size_alloc_bytes = 0;
  }

  // provides the number of entries that are available to read
  int available() override {
    if (buffer_size_alloc_bytes == 0) return 0;
    return (queue_get_level(&queue) * buffer_size);
  }

  // provides the number of entries that are available to write
  int availableForWrite() override { 
    if (buffer_size_alloc_bytes == 0) return size();
    return size() - available(); }

  // returns the address of the start of the physical read buffer
  T *address() override {
    LOGE("address() not implemented");
    return nullptr;
  }

  size_t size() { return buffer_size_alloc_bytes / sizeof(T); }

  /// When we use a non blocking write, the write size must be identical with the buffer size
  void setBlockingWrite(bool flag){
    is_blocking_write = flag;
  }

  /// When we use a blockingread, the we wait for the data to be available
  void setBlockingRead(bool flag){
    is_blocking_read = flag;
  }


 protected:
  queue_t queue;
  int buffer_size_alloc_bytes = 0;
  int buffer_size_req_bytes = 0;
  int buffer_size_bytes = 0;
  int buffer_size = 0;
  SingleBuffer<T> write_buffer{0};
  audio_tools::RingBuffer<T> read_buffer{0};
  bool is_blocking_write = true;
  bool is_blocking_read = false;

  int writeBlocking(const T data[], int len) {
    LOGD("writeArray: %d", len);

    if (len > buffer_size_bytes){
      LOGE("write %d too big for buffer_size: %d", len, buffer_size_bytes);
      return 0;
    }

    // fill the write buffer and when it is full flush it to the queue
    for (int j = 0; j < len; j++) {
      write_buffer.write(data[j]);
      if (write_buffer.isFull()) {
        LOGD("queue_add_blocking");
        queue_add_blocking(&queue, write_buffer.data());
        write_buffer.reset();
      }
    }
    return len;
  }

  int writeNonBlocking(const T data[], int len) {
    if (len != buffer_size_bytes){
      LOGE("write %d must be buffer_size: %d", len, buffer_size_bytes);
      return 0;
    }

    if (queue_try_add(&queue, write_buffer.data())){
      return len;
    }
    return 0;
  }

};

using BufferRP2040 = BufferRP2040T<uint8_t>;

}  // namespace audio_tools
