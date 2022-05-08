#pragma once
namespace audio_tools {

/**
 * @brief An SD backed Ring Buffer that we can use to receive
 * streaming audio. We expect an open file as parameter. The
 * file must support the stream interface and the seek method.
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

template <class F> class FileRingBuffer : public Stream {
public:
  /**
   * @brief Construct a new Ring Buffer File Stream object
   *
   * @param file open file in read / write open
   * @param size maximum file size
   */
  FileRingBuffer(size_t size = 500000, bool autoFlushOnOverflow = true) {
    this->max_size = size;
    this->auto_flush = autoFlushOnOverflow;
  }

  void begin(F file, ) {
    this->file = file;
    active = true;
  }

  /// Write at the end of the ring buffer
  size write(uint8_t buffer, size_t len) {
    if (!active)
      return 0;
    int write_len = len;
    int result = 0;

    // limit write len to available space
    if (write_len > availableForWrite()) {
      if (auto_flush) {
        // remove oldest data to make space
        int to_free = write_len - availableForWrite();
        read_pos += to_free;
        available -= to_free;
        if (read_pos >= max_size) {
          read_pos -= max_size;
        }
        assert(write_len <= availableForWrite())
      } else {
        // limit data to write
        write_len = availableForWrite();
      }
    }

    if (write_len > 0) {
      int part_at_end = write_len;
      int part_at_start = 0;

      // Check if we would read past the end
      if (write_pos + write_len > size) {
        part_at_end = size - write_pos;
        part_at_start = len - part_at_end;
        if (part_at_start < read_pos) {
          LOGE("SDRingBuffer is full");
          part_at_start = 0;
        }
      }

      if (!file.seek(write_pos)) {
        LOGE("seek: %d", write_pos);
      }
      size_t written =
          file.writeBytes(buffer, part_at_end) if (written == len) {
        result += part_at_end;
        write_pos += part_at_start;
        available += part_at_start;
      }
      else {
        LOGE("write at end failed: %d instead of %d", written, part_at_end);
      }

      // at overflow we restart to write at the beginning
      if (part_at_start > 0) {
        if (!file.seek(0)) {
          LOGE("seek: %d", 0);
        }
        size_t written = file.writeBytes(buffer, part_at_start);
        if (written == len) {
          result += part_at_start;
          write_pos = part_at_start;
          available += part_at_start;
        } else {
          LOGE("write at start failed: %d instead of %d", written,
               part_at_start);
        }
      }
    } else {
      LOGE("SDRingBuffer is full");
    }
    return result;
  }

  /// Read from the beginning of the ring buffer
  size readBytes(uint8_t buffer, size_t len) {
    if (!active)
      return 0;
    int read_len = len;
    // if we do not have enough data we limit the size
    if (read_len > available) {
      read_len = available;
    }
    // if we read at end we need to limit the size
    if (read_pos + read_len > max_size) {
      read_len = max_size - read_pos;
    }
    // read the data
    if (!file.seek(read_pos){
      LOGE("seek: %d", read_pos);
    }
    if (file.readBytes(buffer, read_len) == read_len) {
      read_pos += read_len;
    }
    // at end we start at the beginning again
    if (read_pos >= max_size) {
      read_pos = 0;
    }
    return read_len;
  }

  size available() { return available; }
  size availableForWrite() { return max_size - available; }

protected:
  bool active = false;
  bool auto_flush;
  size_t max_size;
  size_t read_pos = 0;
  size_t write_pos = 0;
  size_t available = 0;
  F file;
};

} // namespace audio_tools