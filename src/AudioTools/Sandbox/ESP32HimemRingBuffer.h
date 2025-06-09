#pragma once
#include "ESP32HimemBuffer.h"

/**
 * @brief Ring buffer implementation using ESP32's extended high memory (himem) API
 *
 * ESP32HimemRingBuffer extends the ESP32HimemBuffer to provide circular buffer
 * functionality, where read and write operations wrap around when reaching the
 * buffer's end. This is ideal for:
 * 
 * - Continuous audio streaming applications
 * - Producer-consumer scenarios with different read/write rates
 * - Real-time audio processing with minimal latency
 * - Efficient memory usage for continuous data flows
 *
 * @ingroup buffers
 * @author Phil Schatzmann
 * @copyright GPLv3
 * @tparam T Data type to be stored in the buffer
 */
template <typename T>
class ESP32HimemRingBuffer : public ESP32HimemBuffer<T> {
 public:
  /**
   * @brief Constructor with buffer size and optional window size
   * 
   * Creates a ring buffer with the specified capacity using ESP32 himem.
   * The actual usable capacity will be one element less than specified
   * to distinguish between full and empty states.
   *
   * @param size Number of elements the buffer should hold
   * @param window_size Number of elements per mapping window (default 32768)
   */
  ESP32HimemRingBuffer(size_t size, size_t window_size = 32768) 
      : ESP32HimemBuffer<T>(size, window_size) {
    TRACED();
    // Ring buffer implementation needs one element to distinguish between full/empty
    this->effective_capacity = this->buffer_size - 1;
  }

  /**
   * @brief Read a single value from the ring buffer
   *
   * Reads the next element from the read position and advances the position.
   * If the read position reaches the end, it wraps around to the beginning.
   *
   * @param result Reference where the read value will be stored
   * @return true if read was successful, false if buffer is empty
   */
  bool read(T &result) override {
    if (isEmpty()) {
      return false;
    }

    // Map in the appropriate window if needed
    this->ensureReadWindowMapped();

    // Read from the window
    result = this->window_buffer[this->read_window_offset];
    
    // Advance read position with wrap-around
    this->read_pos = (this->read_pos + 1) % this->buffer_size;
    this->read_window_offset++;
    
    // If we've reached the end of the window, unmap it (next read will map a new one)
    if (this->read_window_offset >= this->window_size_ || 
        this->read_pos == 0) { // Also unmap when wrapping around
      this->unmapReadWindow();
    }
    
    return true;
  }

  /**
   * @brief Write a value to the ring buffer
   *
   * Adds an element to the buffer at the current write position and advances the position.
   * If the write position reaches the end, it wraps around to the beginning.
   *
   * @param data Value to write
   * @return true if write was successful, false if buffer is full
   */
  bool write(T data) override {
    if (isFull()) {
      return false;
    }

    // Map in the appropriate window if needed
    this->ensureWriteWindowMapped();
    
    // Write to the window
    this->window_buffer[this->write_window_offset] = data;
    
    // Advance write position with wrap-around
    this->write_pos = (this->write_pos + 1) % this->buffer_size;
    this->write_window_offset++;
    
    // If we've reached the end of the window, unmap it (next write will map a new one)
    if (this->write_window_offset >= this->window_size_ || 
        this->write_pos == 0) { // Also unmap when wrapping around
      this->unmapWriteWindow();
    }
    
    return true;
  }

  /**
   * @brief Optimized bulk read operation for himem ring buffer
   *
   * Reads multiple elements, handling wrap-around at buffer boundaries.
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

    // Handle reads that may wrap around the buffer
    size_t elements_read = 0;
    size_t remaining = count;
    
    while (remaining > 0) {
      // Figure out current window and position within window
      size_t current_window = this->read_pos / this->window_size_;
      size_t window_offset = this->read_pos % this->window_size_;
      
      // Calculate how many elements we can read in this window before either:
      // 1. Reaching the window boundary
      // 2. Reaching the buffer end (wrap-around point)
      // 3. Reading all requested elements
      size_t to_buffer_end = this->buffer_size - this->read_pos;
      size_t to_window_end = this->window_size_ - window_offset;
      size_t can_read = min(min(remaining, to_window_end), to_buffer_end);
      
      // Map the window if needed
      if (current_window != this->current_read_window) {
        this->unmapReadWindow();
        
        // Map the new window
        size_t offset = current_window * this->window_size_ * sizeof(T);
        esp_err_t err = esp_himem_map(this->himem_handle, this->himem_range, 
                                     offset, this->window_size_ * sizeof(T),
                                     ESP_HIMEM_PERM_READ, 
                                     (void**)&this->window_buffer);
        
        if (err != ESP_OK) {
          LOGE("Failed to map himem for reading: %d", err);
          break;
        }
        
        this->current_read_window = current_window;
      }
      
      // Copy data using efficient memcpy
      memcpy(data + elements_read, this->window_buffer + window_offset, can_read * sizeof(T));
      
      // Update positions
      elements_read += can_read;
      remaining -= can_read;
      this->read_pos = (this->read_pos + can_read) % this->buffer_size;
      
      // If we've reached the end of the window or wrapped around, unmap
      if (this->read_pos == 0 || (this->read_pos % this->window_size_) == 0) {
        this->unmapReadWindow();
      }
    }
    
    LOGD("readArray %d -> %d", len, (int)elements_read);
    return elements_read;
  }

  /**
   * @brief Optimized bulk write operation for himem ring buffer
   *
   * Writes multiple elements, handling wrap-around at buffer boundaries.
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
    
    // Handle writes that may wrap around the buffer
    size_t elements_written = 0;
    size_t remaining = count;
    
    while (remaining > 0) {
      // Figure out current window and position within window
      size_t current_window = this->write_pos / this->window_size_;
      size_t window_offset = this->write_pos % this->window_size_;
      
      // Calculate how many elements we can write in this window before either:
      // 1. Reaching the window boundary
      // 2. Reaching the buffer end (wrap-around point)
      // 3. Writing all requested elements
      size_t to_buffer_end = this->buffer_size - this->write_pos;
      size_t to_window_end = this->window_size_ - window_offset;
      size_t can_write = min(min(remaining, to_window_end), to_buffer_end);
      
      // Map the window if needed
      if (current_window != this->current_write_window) {
        this->unmapWriteWindow();
        
        // Map the new window
        size_t offset = current_window * this->window_size_ * sizeof(T);
        esp_err_t err = esp_himem_map(this->himem_handle, this->himem_range, 
                                     offset, this->window_size_ * sizeof(T),
                                     ESP_HIMEM_PERM_RW, 
                                     (void**)&this->window_buffer);
        
        if (err != ESP_OK) {
          LOGE("Failed to map himem for writing: %d", err);
          break;
        }
        
        this->current_write_window = current_window;
      }
      
      // Copy data using efficient memcpy
      memcpy(this->window_buffer + window_offset, data + elements_written, can_write * sizeof(T));
      
      // Update positions
      elements_written += can_write;
      remaining -= can_write;
      this->write_pos = (this->write_pos + can_write) % this->buffer_size;
      
      // If we've reached the end of the window or wrapped around, unmap
      if (this->write_pos == 0 || (this->write_pos % this->window_size_) == 0) {
        this->unmapWriteWindow();
      }
    }
    
    LOGD("writeArray %d -> %d", len, (int)elements_written);
    return elements_written;
  }

  /**
   * @brief Reset the ring buffer to empty state
   *
   * Clears the buffer by setting read and write positions to the same value.
   */
  void reset() override {
    this->read_pos = 0;
    this->write_pos = 0;
    this->unmapReadWindow();
    this->unmapWriteWindow();
  }

  /**
   * @brief Get number of elements available to read
   *
   * For a ring buffer, this needs to account for the possibility that
   * write_pos might be less than read_pos (when it has wrapped around).
   *
   * @return Number of elements that can be read from the buffer
   */
  int available() override {
    if (this->write_pos >= this->read_pos) {
      return this->write_pos - this->read_pos;
    } else {
      return this->buffer_size - this->read_pos + this->write_pos;
    }
  }

  /**
   * @brief Get space available for writing
   *
   * For a ring buffer, this needs to account for the possibility that
   * read_pos might be less than write_pos (when it has wrapped around).
   * Also, we reserve one element to distinguish between full and empty states.
   *
   * @return Number of elements that can be written to the buffer
   */
  int availableForWrite() override {
    return this->effective_capacity - available();
  }

  /**
   * @brief Check if the buffer is full
   *
   * A ring buffer is full when there is only one empty space left
   * (we reserve one element to distinguish between full and empty).
   *
   * @return true if no more elements can be written
   */
  bool isFull() override {
    return availableForWrite() == 0;
  }

  /**
   * @brief Check if the buffer is empty
   *
   * A ring buffer is empty when read_pos equals write_pos.
   *
   * @return true if no elements are available to read
   */
  bool isEmpty() override {
    return this->read_pos == this->write_pos;
  }

  /**
   * @brief Get the effective capacity of the ring buffer
   * 
   * The effective capacity is one less than the allocated size
   * due to the need to distinguish between full and empty states.
   * 
   * @return Effective capacity in number of elements
   */
  size_t size() override {
    return this->effective_capacity;
  }

 protected:
  size_t effective_capacity; // Actual usable space (buffer_size - 1)
};