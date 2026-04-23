/*
 * Author: Phil Schatzmann
 * RTSP Media Callback Source for custom media data providers
 */

#pragma once

#include "IMediaSource.h"
#include "RTSPFormat.h"

namespace audio_tools {

/**
 * @brief Callback-based RTSP Media Source
 *
 * RTSPMediaCallbackSource provides a flexible way to stream custom media data
 * over RTSP by using user-provided callback functions. This is ideal for:
 * 
 * - Custom audio/video sources that don't fit the Stream interface
 * - Hardware-specific media capture (cameras, microphones, etc.)
 * - Generated content (synthetic audio, test patterns, etc.)
 * - Integration with existing media libraries or codecs
 * 
 * The callback approach allows maximum flexibility while maintaining 
 * compatibility with the RTSP streaming framework.
 *
 * @ingroup rtsp
 * @author Phil Schatzmann
 */
class RTSPMediaCallbackSource : public IMediaSource {
 public:
  /// Callback function type for reading media data
  /// @param buffer Buffer to write media data to
  /// @param maxBytes Maximum number of bytes to write
  /// @param userData Optional user data pointer passed to callback
  /// @return Number of bytes written (0 = no data available, -1 = error)
  typedef int (*read_callback_t)(uint8_t* buffer, int maxBytes, void* userData);
  
  /// Callback function type for source lifecycle management
  /// @param userData Optional user data pointer passed to callback
  typedef void (*lifecycle_callback_t)(void* userData);
  
  /**
   * @brief Default constructor
   */
  RTSPMediaCallbackSource() {
    read_callback = nullptr;
    start_callback = nullptr;
    stop_callback = nullptr;
    user_data = nullptr;
    p_format = &default_format;
    is_active = false;
  }
  
  /**
   * @brief Constructor with read callback
   * @param callback Function to call for reading media data
   * @param userData Optional pointer passed to callbacks
   */
  RTSPMediaCallbackSource(read_callback_t callback, void* userData = nullptr) 
    : RTSPMediaCallbackSource() {
    setReadCallback(callback, userData);
  }
  
  /**
   * @brief Constructor with format and read callback
   * @param format RTSPFormat configuration for the media stream
   * @param callback Function to call for reading media data
   * @param userData Optional pointer passed to callbacks
   */
  RTSPMediaCallbackSource(RTSPFormat& format, read_callback_t callback, void* userData = nullptr) 
    : RTSPMediaCallbackSource(callback, userData) {
    setFormat(format);
  }
  
  virtual ~RTSPMediaCallbackSource() {
    stop();
  }
  
  /**
   * @brief Set the callback function for reading media data
   * @param callback Function that provides media data
   * @param userData Optional pointer passed to the callback
   */
  void setReadCallback(read_callback_t callback, void* userData = nullptr) {
    read_callback = callback;
    user_data = userData;
  }
  
  /**
   * @brief Set optional start callback (called when streaming begins)
   * @param callback Function called during start()
   */
  void setStartCallback(lifecycle_callback_t callback) {
    start_callback = callback;
  }
  
  /**
   * @brief Set optional stop callback (called when streaming ends)
   * @param callback Function called during stop()
   */
  void setStopCallback(lifecycle_callback_t callback) {
    stop_callback = callback;
  }
  
  /**
   * @brief Set the RTSP format configuration
   * @param format Format configuration for the media stream
   */
  void setFormat(RTSPFormat& format) {
    p_format = &format;
  }
  
  /**
   * @brief Get the current RTSP format configuration
   * @return Reference to the format configuration
   */
  RTSPFormat& getFormat() override {
    return *p_format;
  }
  
  /**
   * @brief Read media data using the configured callback
   * @param dest Buffer to receive media data
   * @param maxBytes Maximum number of bytes to read
   * @return Number of bytes actually read
   */
  int readBytes(void* dest, int maxBytes) override {
    if (!is_active || read_callback == nullptr) {
      return 0;
    }
    
    if (dest == nullptr || maxBytes <= 0) {
      LOGW("RTSPMediaCallbackSource: invalid parameters");
      return 0;
    }
    
    // Call user-provided callback
    int result = read_callback((uint8_t*)dest, maxBytes, user_data);
    
    // Log result for debugging
    if (result > 0) {
      LOGD("RTSPMediaCallbackSource: read %d bytes", result);
    } else if (result < 0) {
      LOGW("RTSPMediaCallbackSource: callback returned error %d", result);
    }
    
    return result;
  }
  
  /**
   * @brief Start the media source
   * Calls the optional start callback if configured
   */
  void start() override {
    LOGI("RTSPMediaCallbackSource: starting");
    
    if (read_callback == nullptr) {
      LOGE("RTSPMediaCallbackSource: no read callback configured");
      return;
    }
    
    IMediaSource::start();
    is_active = true;
    
    // Call optional start callback
    if (start_callback != nullptr) {
      start_callback(user_data);
    }
    
    LOGI("RTSPMediaCallbackSource: started successfully");
  }
  
  /**
   * @brief Stop the media source
   * Calls the optional stop callback if configured
   */
  void stop() override {
    LOGI("RTSPMediaCallbackSource: stopping");
    
    is_active = false;
    IMediaSource::stop();
    
    // Call optional stop callback
    if (stop_callback != nullptr) {
      stop_callback(user_data);
    }
    
    LOGI("RTSPMediaCallbackSource: stopped");
  }
  
  /**
   * @brief Check if the source is active
   * @return true if started and callback is configured
   */
  bool isActive() const {
    return is_active && read_callback != nullptr;
  }
  
  /**
   * @brief Get the user data pointer
   * @return Pointer passed to callbacks
   */
  void* getUserData() const {
    return user_data;
  }
  
 protected:
  read_callback_t read_callback;
  lifecycle_callback_t start_callback;
  lifecycle_callback_t stop_callback;
  void* user_data;
  RTSPFormat* p_format;
  RTSPFormatPCM default_format;
  bool is_active;
};

/**
 * @brief Convenience typedef for callback function signatures
 */
namespace RTSPCallbacks {
  using ReadCallback = RTSPMediaCallbackSource::read_callback_t;
  using LifecycleCallback = RTSPMediaCallbackSource::lifecycle_callback_t;
}

} // namespace audio_tools