#pragma once

namespace audio_tools {

/**
 * @brief Auto-expanding buffer composed of multiple buffer instances
 * 
 * DynamicMultiBuffer manages a collection of buffer components, automatically adding
 * new buffers when existing ones become full. This provides dynamically growing 
 * storage capacity while maintaining the performance characteristics of the 
 * underlying buffer implementation.
 * 
 * Ideal use cases:
 * - Recording audio of unknown duration
 * - Processing large audio files without pre-allocating maximum memory
 * - Audio applications where memory requirements grow unpredictably
 * - Efficient memory use by allocating only what's needed
 * 
 * @ingroup buffers
 * @author Phil Schatzmann
 * @copyright GPLv3
 * @tparam T Data type to be stored in the buffer
 * @tparam BufferType The buffer implementation to use for each component
 */
template <typename T, template<typename> class BufferType>
class DynamicMultiBuffer : public BaseBuffer<T> {
 public:
  /**
   * @brief Constructor with buffer configuration
   * 
   * @param component_size Size of each individual buffer component (elements)
   * @param initial_components Number of buffer components to pre-allocate
   * @param max_components Maximum number of components (0 for unlimited)
   */
  DynamicMultiBuffer(size_t component_size, size_t initial_components = 1, 
                     size_t max_components = 0) {
    TRACED();
    component_size_ = component_size;
    max_components_ = max_components;
    
    // Create initial buffer components
    for (size_t i = 0; i < initial_components; i++) {
      if (!addBufferComponent()) {
        LOGE("Failed to allocate initial buffer component %d", (int)i);
        break;
      }
    }
  }

  /**
   * @brief Destructor - releases all buffer components
   */
  virtual ~DynamicMultiBuffer() {
    TRACED();
    // Clean up all buffer components
    for (auto* buffer : buffer_components) {
      delete buffer;
    }
    buffer_components.clear();
  }

  /**
   * @brief Read a single value from the buffer
   * 
   * Reads from the current read position, potentially spanning buffer components.
   * 
   * @param result Reference where the read value will be stored
   * @return true if read was successful, false if buffer is empty
   */
  bool read(T &result) override {
    if (isEmpty()) {
      return false;
    }

    // Find the buffer component containing the read position
    size_t buffer_idx = read_pos / component_size_;
    size_t local_pos = read_pos % component_size_;
    
    // Perform read from the appropriate buffer
    bool success = buffer_components[buffer_idx]->read(result);
    if (success) {
      read_pos++;
    }
    
    return success;
  }

  /**
   * @brief Peek at the next value without removing it
   * 
   * Returns the next element that would be read without advancing the read position.
   * 
   * @param result Reference where the peeked value will be stored
   * @return true if peek was successful, false if buffer is empty
   */
  bool peek(T &result) override {
    if (isEmpty()) {
      return false;
    }

    // Find the buffer component containing the read position
    size_t buffer_idx = read_pos / component_size_;
    size_t local_pos = read_pos % component_size_;
    
    // Perform peek from the appropriate buffer
    return buffer_components[buffer_idx]->peek(result);
  }

  /**
   * @brief Write a value to the buffer
   * 
   * Adds an element to the buffer at the current write position,
   * automatically expanding capacity by adding a new buffer component if needed.
   * 
   * @param data Value to write
   * @return true if write was successful, false if buffer is full (max components reached)
   */
  bool write(T data) override {
    // Check if we need to add a new buffer component
    if (isCurrentBufferFull()) {
      if (!addBufferComponent()) {
        return false; // Couldn't expand, so write fails
      }
    }

    // Find the buffer component for writing
    size_t buffer_idx = write_pos / component_size_;
    size_t local_pos = write_pos % component_size_;
    
    // Perform write to the appropriate buffer
    bool success = buffer_components[buffer_idx]->write(data);
    if (success) {
      write_pos++;
    }
    
    return success;
  }

  /**
   * @brief Optimized bulk read operation
   * 
   * Reads multiple elements, potentially spanning across buffer components.
   * 
   * @param data Buffer where read data will be stored
   * @param len Number of elements to read
   * @return Number of elements actually read
   */
  int readArray(T data[], int len) override {
    if (isEmpty() || data == nullptr) {
      return 0;
    }
    
    int total_read = 0;
    int remaining = min(len, available());
    
    while (remaining > 0) {
      // Find the current buffer component and position
      size_t buffer_idx = read_pos / component_size_;
      size_t local_pos = read_pos % component_size_;
      
      // Calculate how many elements we can read from this component
      int can_read = min(remaining, (int)(component_size_ - local_pos));
      
      // Set up the buffer component for reading from the correct position
      buffer_components[buffer_idx]->reset();
      for (size_t i = 0; i < local_pos; i++) {
        T dummy;
        buffer_components[buffer_idx]->read(dummy);
      }
      
      // Read from the buffer component
      int actually_read = buffer_components[buffer_idx]->readArray(data + total_read, can_read);
      
      // Update positions
      total_read += actually_read;
      read_pos += actually_read;
      remaining -= actually_read;
      
      // If we couldn't read as much as expected, stop
      if (actually_read < can_read) {
        break;
      }
    }
    
    return total_read;
  }

  /**
   * @brief Optimized bulk write operation
   * 
   * Writes multiple elements, automatically expanding capacity by adding 
   * new buffer components as needed.
   * 
   * @param data Array of elements to write
   * @param len Number of elements to write
   * @return Number of elements actually written
   */
  int writeArray(const T data[], int len) override {
    if (data == nullptr) {
      return 0;
    }
    
    int total_written = 0;
    int remaining = len;
    
    while (remaining > 0) {
      // Check if we need to add a new buffer component
      if (isCurrentBufferFull()) {
        if (!addBufferComponent()) {
          break; // Couldn't expand, so stop writing
        }
      }
      
      // Find the current buffer component and position
      size_t buffer_idx = write_pos / component_size_;
      size_t local_pos = write_pos % component_size_;
      
      // Calculate how many elements we can write to this component
      int can_write = min(remaining, (int)(component_size_ - local_pos));
      
      // Set up the buffer component for writing at the correct position
      buffer_components[buffer_idx]->reset();
      for (size_t i = 0; i < local_pos; i++) {
        buffer_components[buffer_idx]->write(T()); // Write dummy values
      }
      
      // Write to the buffer component
      int actually_written = buffer_components[buffer_idx]->writeArray(data + total_written, can_write);
      
      // Update positions
      total_written += actually_written;
      write_pos += actually_written;
      remaining -= actually_written;
      
      // If we couldn't write as much as expected, stop
      if (actually_written < can_write) {
        break;
      }
    }
    
    return total_written;
  }

  /**
   * @brief Reset the buffer to empty state
   * 
   * Clears all buffer components but keeps them allocated.
   */
  void reset() override {
    read_pos = 0;
    write_pos = 0;
    
    // Reset all buffer components
    for (auto* buffer : buffer_components) {
      buffer->reset();
    }
  }

  /**
   * @brief Get number of elements available to read
   * 
   * @return Number of elements that can be read from the buffer
   */
  int available() override {
    return write_pos - read_pos;
  }

  /**
   * @brief Get space available for writing
   * 
   * Returns remaining space in existing components plus potential
   * new components up to max_components limit.
   * 
   * @return Number of elements that can be written
   */
  int availableForWrite() override {
    // Calculate space in existing buffers
    size_t existing_space = totalCapacity() - write_pos;
    
    // If no maximum component limit, return a large value
    if (max_components_ == 0 || buffer_components.size() < max_components_) {
      return existing_space + 1000000; // Effectively unlimited
    }
    
    return existing_space;
  }

  /**
   * @brief Check if the buffer is full
   * 
   * Buffer is considered full only if we've reached max components
   * and the last component is full.
   * 
   * @return true if no more elements can be written
   */
  bool isFull() override {
    if (max_components_ == 0) {
      return false; // Never full if unlimited components
    }
    
    return buffer_components.size() >= max_components_ && isCurrentBufferFull();
  }

  /**
   * @brief Check if the buffer is empty
   * 
   * @return true if no elements are available to read
   */
  bool isEmpty() override {
    return available() == 0;
  }

  /**
   * @brief Get pointer to current read position
   * 
   * Note: This method has limitations since data may span multiple buffer
   * components. It returns the address from the current component.
   * 
   * @return Pointer to the current read position in memory
   */
  T* address() override {
    if (isEmpty() || buffer_components.empty()) {
      return nullptr;
    }
    
    size_t buffer_idx = read_pos / component_size_;
    return buffer_components[buffer_idx]->address();
  }

  /**
   * @brief Get total capacity of the buffer
   * 
   * @return Size of all components combined, in number of elements
   */
  size_t size() override {
    return totalCapacity();
  }

  /**
   * @brief Resize the buffer
   * 
   * Changes the number of buffer components to match the requested size.
   * 
   * @param new_size New size in number of elements
   * @return true if resize was successful
   */
  bool resize(int new_size) override {
    // Calculate needed components
    size_t needed_components = (new_size + component_size_ - 1) / component_size_;
    
    // Check if we're allowed to have this many components
    if (max_components_ != 0 && needed_components > max_components_) {
      needed_components = max_components_;
    }
    
    // Remove excess components
    while (buffer_components.size() > needed_components) {
      delete buffer_components.back();
      buffer_components.pop_back();
    }
    
    // Add needed components
    while (buffer_components.size() < needed_components) {
      if (!addBufferComponent()) {
        return false;
      }
    }
    
    // Adjust read/write positions if they're now out of bounds
    size_t total_capacity = totalCapacity();
    if (write_pos > total_capacity) {
      write_pos = total_capacity;
    }
    if (read_pos > write_pos) {
      read_pos = write_pos;
    }
    
    return true;
  }

  /**
   * @brief Get the number of buffer components
   * 
   * @return Current number of buffer components
   */
  size_t getComponentCount() {
    return buffer_components.size();
  }

  /**
   * @brief Get the size of each component
   * 
   * @return Size of each buffer component in elements
   */
  size_t getComponentSize() {
    return component_size_;
  }

  /**
   * @brief (Re)sets the read pos to the start
   */
  bool begin() {
    read_pos = 0;
    return true;
  }

  /**
   * @brief (Re)sets the read pos to the indicated position
   */
  bool begin(int pos) {
    if (pos > write_pos) return false;
    read_pos = pos;
    return true;
  }

 protected:
  Vector<BufferType<T>*> buffer_components; // Collection of buffer components
  size_t component_size_ = 0;    // Size of each component in elements
  size_t max_components_ = 0;    // Maximum number of components (0 = unlimited)
  size_t read_pos = 0;           // Global read position
  size_t write_pos = 0;          // Global write position

  /**
   * @brief Add a new buffer component
   * 
   * Creates and adds a new buffer component if allowed by max_components.
   * 
   * @return true if component was added, false if max reached or allocation failed
   */
  bool addBufferComponent() {
    // Check if we're allowed to add more components
    if (max_components_ != 0 && buffer_components.size() >= max_components_) {
      LOGW("Maximum number of buffer components reached: %d", (int)max_components_);
      return false;
    }
    
    // Create new buffer component
    BufferType<T>* new_buffer = new BufferType<T>(component_size_);
    if (new_buffer == nullptr) {
      LOGE("Failed to allocate new buffer component");
      return false;
    }

    // make sure the component has the correct size
    new_buffer->resize(component_size_);
    
    // Add to our collection
    buffer_components.push_back(new_buffer);
    LOGI("Added buffer component #%d", (int)buffer_components.size());
    
    return true;
  }

  /**
   * @brief Calculate total capacity of all buffer components
   * 
   * @return Combined size of all components in elements
   */
  size_t totalCapacity() {
    return buffer_components.size() * component_size_;
  }

  /**
   * @brief Check if the current write buffer is full
   * 
   * @return true if current buffer is full or no buffers exist
   */
  bool isCurrentBufferFull() {
    if (buffer_components.empty()) {
      return true;
    }
    
    size_t buffer_idx = write_pos / component_size_;
    size_t local_pos = write_pos % component_size_;
    
    return local_pos >= component_size_ || buffer_idx >= buffer_components.size();
  }
};

}