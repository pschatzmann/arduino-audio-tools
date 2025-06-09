#pragma once

#include "AudioTools/CoreAudio/Buffers.h"

namespace audio_tools {

/**
 * @brief Buffer implementation using ESP32's extended high memory (himem) API
 *
 * ESP32HimemBuffer provides access to memory beyond the standard 4MB SPIRAM
 * limit by leveraging the ESP32 himem API. This enables creating much larger
 * buffers for audio applications requiring significant memory.
 *
 * Usage Examples:
 * // Default window size (32768 elements)
 * ESP32HimemBuffer<int16_t> defaultBuffer(1000000);
 * // Small window size for memory-constrained scenarios
 * ESP32HimemBuffer<int16_t> smallWindowBuffer(1000000, 8192);
 * // Large window size for less frequent window switching (better performance
 * with more RAM use) ESP32HimemBuffer<int16_t> largeWindowBuffer(1000000,
 * 65536); *
 * @ingroup buffers
 * @author Phil Schatzmann
 * @copyright GPLv3
 * @tparam T Data type to be stored in the buffer
 */
template <typename T>
class ESP32HimemBuffer : public BaseBuffer<T> {
 public:
  /**
   * @brief Constructor with buffer size and optional window size
   *
   * Creates a buffer with the specified capacity using ESP32 himem.
   * The window size determines how many elements are mapped at once,
   * affecting performance and memory usage.
   *
   * @param size Number of elements the buffer should hold
   * @param window_size Number of elements per mapping window (default 32768)
   */
  ESP32HimemBuffer(size_t size, size_t window_size = 32768) {
    TRACED();
    window_size_ = window_size;
    allocate(size);
  }

  /**
   * @brief Destructor - releases allocated himem
   */
  virtual ~ESP32HimemBuffer() {
    TRACED();
    deallocate();
  }

  /**
   * @brief Read a single value from the buffer
   *
   * Reads the next element from the read position and advances the position.
   *
   * @param result Reference where the read value will be stored
   * @return true if read was successful, false if buffer is empty
   */
  bool read(T &result) override {
    if (isEmpty()) {
      return false;
    }

    // Map in the appropriate window if needed
    ensureReadWindowMapped();

    // Read from the window
    result = window_buffer[read_window_offset];

    // Advance read position
    read_pos++;
    read_window_offset++;

    // If we've reached the end of the window, unmap it (next read will map a
    // new one)
    if (read_window_offset >= window_size_ || read_pos >= buffer_size) {
      unmapReadWindow();
    }

    return true;
  }

  /**
   * @brief Peek at the next value without removing it
   *
   * Returns the next element that would be read without advancing the read
   * position.
   *
   * @param result Reference where the peeked value will be stored
   * @return true if peek was successful, false if buffer is empty
   */
  bool peek(T &result) override {
    if (isEmpty()) {
      return false;
    }

    // Map in the appropriate window if needed
    ensureReadWindowMapped();

    // Peek from the window
    result = window_buffer[read_window_offset];
    return true;
  }

  /**
   * @brief Write a value to the buffer
   *
   * Adds an element to the buffer at the current write position.
   *
   * @param data Value to write
   * @return true if write was successful, false if buffer is full
   */
  bool write(T data) override {
    if (isFull()) {
      return false;
    }

    // Map in the appropriate window if needed
    ensureWriteWindowMapped();

    // Write to the window
    window_buffer[write_window_offset] = data;

    // Advance write position
    write_pos++;
    write_window_offset++;

    // If we've reached the end of the window, unmap it (next write will map a
    // new one)
    if (write_window_offset >= window_size_ || write_pos >= buffer_size) {
      unmapWriteWindow();
    }

    return true;
  }

  /**
   * @brief Optimized bulk read operation for himem
   *
   * Reads multiple elements in a way that minimizes window mapping/unmapping
   * operations, providing much better performance than individual reads.
   *
   * @param data Buffer where read data will be stored
   * @param len Number of elements to read
   * @return Number of elements actually read
   */
  int readArray(T data[], int len) override {
    if (isEmpty() || data == nullptr) {
      return 0;
    }

    int count = min(len, available());
    if (count == 0) return 0;

    // Handle reads that span across windows
    size_t elements_read = 0;
    size_t remaining = count;

    while (remaining > 0) {
      // Figure out current window and position within window
      size_t current_window = read_pos / window_size_;
      size_t window_offset = read_pos % window_size_;

      // Calculate how many elements we can read in this window
      size_t can_read = min(remaining, window_size_ - window_offset);

      // Map the window if needed
      if (current_window != current_read_window) {
        unmapReadWindow();

        // Map the new window
        size_t offset = current_window * window_size_ * sizeof(T);
        esp_err_t err = esp_himem_map(
            himem_handle, himem_range, offset, window_size_ * sizeof(T),
            ESP_HIMEM_PERM_READ, (void **)&window_buffer);

        if (err != ESP_OK) {
          LOGE("Failed to map himem for reading: %d", err);
          break;
        }

        current_read_window = current_window;
      }

      // Copy data using efficient memcpy
      memcpy(data + elements_read, window_buffer + window_offset,
             can_read * sizeof(T));

      // Update positions
      elements_read += can_read;
      remaining -= can_read;
      read_pos += can_read;

      // If we've read all available data or reached the end of the buffer
      if (read_pos >= buffer_size) {
        unmapReadWindow();
        break;
      }
    }

    LOGD("readArray %d -> %d", len, (int)elements_read);
    return elements_read;
  }

  /**
   * @brief Optimized bulk write operation for himem
   *
   * Writes multiple elements in a way that minimizes window mapping/unmapping
   * operations, providing much better performance than individual writes.
   *
   * @param data Array of elements to write
   * @param len Number of elements to write
   * @return Number of elements actually written
   */
  int writeArray(const T data[], int len) override {
    if (isFull() || data == nullptr) {
      return 0;
    }

    int count = min(len, availableForWrite());
    if (count == 0) return 0;

    // Handle writes that span across windows
    size_t elements_written = 0;
    size_t remaining = count;

    while (remaining > 0) {
      // Figure out current window and position within window
      size_t current_window = write_pos / window_size_;
      size_t window_offset = write_pos % window_size_;

      // Calculate how many elements we can write in this window
      size_t can_write = min(remaining, window_size_ - window_offset);

      // Map the window if needed
      if (current_window != current_write_window) {
        unmapWriteWindow();

        // Map the new window
        size_t offset = current_window * window_size_ * sizeof(T);
        esp_err_t err = esp_himem_map(
            himem_handle, himem_range, offset, window_size_ * sizeof(T),
            ESP_HIMEM_PERM_RW, (void **)&window_buffer);

        if (err != ESP_OK) {
          LOGE("Failed to map himem for writing: %d", err);
          break;
        }

        current_write_window = current_window;
      }

      // Copy data using efficient memcpy
      memcpy(window_buffer + window_offset, data + elements_written,
             can_write * sizeof(T));

      // Update positions
      elements_written += can_write;
      remaining -= can_write;
      write_pos += can_write;

      // If we've filled the buffer
      if (write_pos >= buffer_size) {
        unmapWriteWindow();
        break;
      }
    }

    LOGD("writeArray %d -> %d", len, (int)elements_written);
    return elements_written;
  }

  /**
   * @brief Get the current window size
   *
   * Returns the number of elements that are mapped at once in each window.
   *
   * @return Window size in number of elements
   */
  size_t getWindowSize() { return window_size_; }

  // Rest of the methods unchanged...

 protected:
  // Himem handle and management
  esp_himem_handle_t himem_handle = NULL;
  esp_himem_rangehandle_t himem_range = NULL;

  // Buffer state
  size_t buffer_size = 0;  // Total elements in buffer
  size_t read_pos = 0;     // Current read position
  size_t write_pos = 0;    // Current write position

  // Window management
  size_t window_size_ = 32768;  // Elements per window (configurable)
  T *window_buffer = nullptr;   // Pointer to mapped window
  size_t current_read_window =
      SIZE_MAX;  // Which window is currently mapped for reading
  size_t current_write_window =
      SIZE_MAX;  // Which window is currently mapped for writing
  size_t read_window_offset = 0;   // Offset within read window
  size_t write_window_offset = 0;  // Offset within write window

  /**
   * @brief Allocate memory for the buffer
   *
   * Requests memory from the ESP32 himem system.
   *
   * @param size Number of elements to allocate
   * @return true if allocation was successful
   */
  bool allocate(size_t size) {
    esp_err_t err;

    // Calculate needed memory in bytes
    size_t bytes_needed = size * sizeof(T);

    // Align to block size required by himem
    size_t block_size = esp_himem_get_phys_size();
    size_t blocks_needed = (bytes_needed + block_size - 1) / block_size;
    size_t bytes_allocated = blocks_needed * block_size;

    // Adjust buffer_size to actual capacity
    buffer_size = bytes_allocated / sizeof(T);

    // Allocate himem
    err = esp_himem_alloc(bytes_allocated, &himem_handle);
    if (err != ESP_OK) {
      LOGE("Failed to allocate himem: %d", err);
      return false;
    }

    // Allocate range for mapping windows
    err = esp_himem_alloc_map_range(ESP_HIMEM_BLKSZ, &himem_range);
    if (err != ESP_OK) {
      LOGE("Failed to allocate himem range: %d", err);
      esp_himem_free(himem_handle);
      himem_handle = NULL;
      return false;
    }

    // Allocate window buffer (using the configured window size)
    window_buffer = (T *)malloc(window_size_ * sizeof(T));
    if (window_buffer == nullptr) {
      LOGE("Failed to allocate window buffer");
      esp_himem_free_map_range(himem_range);
      esp_himem_free(himem_handle);
      himem_range = NULL;
      himem_handle = NULL;
      return false;
    }

    reset();
    return true;
  }

  // Other methods with updated window_size_ references...

  /**
   * @brief Ensure the appropriate read window is mapped
   *
   * Maps the memory block containing the current read position
   * into the window buffer if it's not already mapped.
   */
  void ensureReadWindowMapped() {
    if (himem_handle == NULL || himem_range == NULL) return;

    size_t window_needed = read_pos / window_size_;

    // If correct window already mapped, just update offset
    if (current_read_window == window_needed) {
      read_window_offset = read_pos % window_size_;
      return;
    }

    // Unmap current window if any
    unmapReadWindow();

    // Map new window
    size_t offset = window_needed * window_size_ * sizeof(T);
    esp_err_t err = esp_himem_map(himem_handle, himem_range, offset,
                                  window_size_ * sizeof(T), ESP_HIMEM_PERM_READ,
                                  (void **)&window_buffer);

    if (err != ESP_OK) {
      LOGE("Failed to map himem for reading: %d", err);
      return;
    }

    current_read_window = window_needed;
    read_window_offset = read_pos % window_size_;
  }

  /**
   * @brief Ensure the appropriate write window is mapped
   *
   * Maps the memory block containing the current write position
   * into the window buffer if it's not already mapped.
   */
  void ensureWriteWindowMapped() {
    if (himem_handle == NULL || himem_range == NULL) return;

    size_t window_needed = write_pos / window_size_;

    // If correct window already mapped, just update offset
    if (current_write_window == window_needed) {
      write_window_offset = write_pos % window_size_;
      return;
    }

    // Unmap current window if any
    unmapWriteWindow();

    // Map new window
    size_t offset = window_needed * window_size_ * sizeof(T);
    esp_err_t err = esp_himem_map(himem_handle, himem_range, offset,
                                  window_size_ * sizeof(T), ESP_HIMEM_PERM_RW,
                                  (void **)&window_buffer);

    if (err != ESP_OK) {
      LOGE("Failed to map himem for writing: %d", err);
      return;
    }

    current_write_window = window_needed;
    write_window_offset = write_pos % window_size_;
  }

  // Other methods as before...
};
}  // namespace audio_tools