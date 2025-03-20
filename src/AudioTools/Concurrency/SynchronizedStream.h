#pragma once
#include "AudioTools/CoreAudio/Buffers.h"
#include "LockGuard.h"
#include "Mutex.h"
#include "Stream.h"

namespace audio_tools {

/***
 * @brief Wrapper class that can turn any Stream into a thread save
 * implementation. This is done by adding a Mutex to the Stream. The
 * read and write operations are buffered and the access to the stream is
 * protected by the Mutex.
 * @ingroup streams
 * @ingroup concurrency
 * @author Phil Schatzmann
 */

class SynchronizedStream : public Stream {
 public:
  SynchronizedStream(Stream &stream, MutexBase &mutex) {
    p_stream = &stream;
    p_mutex = &mutex;
  }

  // reads a single value
  int read() override {
    if (read_buffer.isEmpty()) {
      LockGuard guard(p_mutex);
      p_stream->readBytes(read_buffer.address(), read_buffer.size());
    }
    return read_buffer.read();
  }

  // peeks the actual entry from the buffer
  int peek() override {
    LockGuard guard(p_mutex);
    return p_stream->peek();
  }

  // write add an entry to the buffer
  size_t write(uint8_t data) override {
    write_buffer.write(data);
    if (write_buffer.isFull()) {
      LockGuard guard(p_mutex);
      size_t written = p_stream->write((const uint8_t *)write_buffer.data(),
                                       write_buffer.size());
      assert(written == write_buffer.size());
      write_buffer.reset();
    }
    return 1;
  }

  // provides the number of entries that are available to read
  int available() override {
    LockGuard guard(p_mutex);
    return p_stream->available();
  }

  // provides the number of entries that are available to write
  int availableForWrite() override {
    LockGuard guard(p_mutex);
    return p_stream->availableForWrite();
  }

  /// Defines the size of the internal buffers
  void setBufferSize(int size) {
    read_buffer.resize(size);
    write_buffer.resize(size);
  }

 protected:
  Stream *p_stream = nullptr;
  MutexBase *p_mutex = nullptr;
  SingleBuffer<uint8_t> read_buffer;
  SingleBuffer<uint8_t> write_buffer;
};

}  // namespace audio_tools
