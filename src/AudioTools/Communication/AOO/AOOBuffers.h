#pragma once

#include "AudioTools/CoreAudio/Buffers.h"
#include "stdint.h"

namespace audio_tools {

/**
 * @brief AOO Write Buffer which caches the written data for some time to be
 * able to resend lost data
 * @author Phil Schatzmann
 */

class AOOSourceBuffer {
 public:
  AOOSourceBuffer() = default;

  AOOSourceBuffer(uint32_t timeout) { setTimeout(timeout); }

  /// adds the data to the buffer
  size_t writeArray(int id, const uint8_t *data, size_t len) {
    if (buffer_size == 0) setBufferSize(len);
    if (timeout_ms == 0) return 0;
    if (len > buffer_size) {
      LOGE("Buffer overflow %d > %d", len, buffer_size);
      return 0;
    }
    SingleBuffer<uint8_t> *p_buffer = newBuffer();
    if (p_buffer = nullptr) {
      TRACEE()
      return 0;
    }
    p_buffer->timestamp = millis() + timeout_ms;
    p_buffer->id = id;
    p_buffer->writeArray(data, len);
  }

  /// finds the data from the buffer
  size_t readArray(int id, uint8_t *data, size_t len) {
    if (len > buffer_size) {
      LOGE("Buffer underflow %d > %d", len, buffer_size);
      return 0;
    }
    for (auto &buffer : buffers) {
      if (buffer.id == id) {
        return buffer.readArray(data, len);
      }
    }
  }

  SingleBuffer<uint8_t> *getBuffer(int id) {
    for (auto &buffer : buffers) {
      if (buffer.id == id) {
        return &buffer;
      }
    }
    return nullptr;
  }

  void clear() { buffers.clear(); }

  /// Defines the validity time for a buffer entry: If 0, do not buffer!
  void setTimeout(uint32_t ms) { timeout_ms = ms; }

  /// Defines the size of an inidiviual buffer entry. If 0 it will be determined
  /// automatically based on the first write call
  void setBufferSize(size_t len) {
    clear();
    buffer_size = len;
  }

 protected:
  uint32_t timeout_ms = 1000;
  size_t buffer_size = 0;
  Vector<SingleBuffer<uint8_t>> buffers;

  /// Returns an expired buffer or creates a new one
  SingleBuffer<uint8_t> *newBuffer() {
    for (auto &buffer : buffers) {
      if (buffer.timestamp < millis()) {
        return &buffer;
      }
    }
    // add new buffer
    auto buffer = new SingleBuffer<uint8_t>(buffer_size);
    if (buffer == nullptr) {
      LOGE("insufficient RAM");
      return nullptr;
    }
    buffers.push_back(*buffer);
    return buffer;
  }
};

/**
 * @brief AOO Buffer is a NBuffer which uses a sequence numer to identify each
 * buffer entry and allows to add an empty buffer entry for gaps that can be
 * refilled later. The BaseBuffer entries are resized dynamically.
 * @author Phil Schatzmann
 */

class AOOSinkBuffer : public BaseBuffer<uint8_t> {
 public:
  AOOSinkBuffer() = default;

  /// Must be sized at runtime to support psram
  void resize(int count) {
    if (count == 0) return;
    nbuffer.resize(0, count);
  }

  /// Defines the actual buffer id that will be used in the next entry
  void setActualId(int id) { actual_id = id; }

  /// reads multiple values
  int readArray(uint8_t data[], int len) override {
    return nbuffer.readArray(data, len);
  }

  /// Standard write function used by the mixer
  int writeArray(const uint8_t data[], int len) override {
    // provide individual SingleBuffer
    SingleBuffer<uint8_t> *rec = nbuffer.writeEnd();
    if (rec == nullptr) {
      LOGE("insufficient Buffers");
      return 0;
    }
    // resize if needed
    if (len > rec->size()) {
      rec->resize(len);
    }
    rec->active = len > 0;
    rec->id = actual_id;
    rec->timestamp = millis();
    return rec->writeArray(data, len);
  }

  /// Fill the buffer for a specific gap id
  int updateArray(int id, const uint8_t data[], int len) {
    SingleBuffer<uint8_t> *rec = (SingleBuffer<uint8_t> *) nbuffer.getBuffer(id);
    if (rec == nullptr) {
      return 0;
    }
    if (len > rec->size()) {
      rec->resize(len);
    }
    rec->active = true;
    rec->timestamp = millis();
    return rec->writeArray(data, len);
  }

  int available() override { return nbuffer.available(); }
  int availableForWrite() override { return nbuffer.availableForWrite(); }
  bool isFull() override { return nbuffer.isFull(); }
  bool isEmpty() { return nbuffer.isEmpty(); }
  size_t size() { return nbuffer.size(); }
  bool read(uint8_t &result) override { return nbuffer.read(result); }
  bool peek(uint8_t &result) override { return nbuffer.peek(result); }
  bool write(uint8_t out) { return nbuffer.write(out); }
  void reset() {
    nbuffer.reset();
    actual_id = 0;
  }
  uint8_t *address() { return nbuffer.address(); }

 protected:
  uint8_t *buffer = nullptr;
  NBufferExt<uint8_t> nbuffer{0, 0};
  int actual_id = 0;
};

}  // namespace audio_tools