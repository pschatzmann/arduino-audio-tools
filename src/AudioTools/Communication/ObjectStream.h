#include "AudioTools/CoreAudio/BaseStream.h"
namespace audio_tools {
/**
 * @brief A Arduino Stream which makes sure that we read back the same size
 * as we wrote. It adds a size prefix to the data stream.
 * @ingroup communications
 * @author Phil Schatzmann
 */
class ObjectStream : public BaseStream {
  ObjectStream(Stream &stream) {
    p_in = &stream;
    p_out = &stream;
  }
  ObjectStream(Print &stream) { p_out = &stream; }

  /// reads the data from the input stream: I recommend to set the len to the
  /// max object size, to avoid that we read only a part of the object.
  size_t readBytes(uint8_t *data, size_t len) {
    if (p_in == nullptr) return 0;
    // read the size prefix if necessary
    int to_read = available();
    size_t result = 0;
    if (to_read > 0) {
      if (to_read > len) to_read = len;
      // read the data
      result = p_in->readBytes(data, to_read);
      // determe the open number of bytes
      n_open_read -= result;
      is_complete = n_open_read == 0;
      // if we have read all data we need to read the size prefix again
      if (is_complete) n_open_read = -1;
    }
    return result;
  }

  size_t write(const uint8_t *data, size_t len) {
    if (p_out == nullptr) return 0;
    // write the size prefix
    p_out->write((uint8_t *)&len, sizeof(size_t));
    // write the data
    return p_out->write(data, len);
  }

  int available() {
    if (p_in == nullptr) return 0;
    if (n_open_read >= 0) return n_open_read;
    // make sure that we can read the size prefix
    if (p_in->available() < sizeof(size_t)) return 0;
    // read the size prefix
    p_in->readBytes((uint8_t *)&n_open_read, sizeof(size_t));
    return n_open_read;
  }

  // not supported
  virtual size_t write(uint8_t ch) { return 0; }

  int availableForWrite() override {
    if (max_object_size > 0) return max_object_size;
    if (p_out == nullptr) return DEFAULT_BUFFER_SIZE;
    return p_out->availableForWrite();
  }

  /// When value is 0 (default) we determine it from the output, otherwise we
  /// return the defined value.
  void setMaxObjectSize(size_t size) { max_object_size = size; }

  /// Determine if we processed the complete object
  bool isObjectComplete() { return is_complete; }

 protected:
  Stream *p_in = nullptr;
  Print *p_out = nullptr;
  int n_open_read = -1;
  int max_object_size = 0;  // 0 u
  bool is_complete = true;
};
}  // namespace audio_tools