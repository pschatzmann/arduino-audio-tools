#pragma once

#include <memory>
#include "esp_camera.h"

class Camera;

/***
 * @brief A simple Arduino C++ API over the ESP32 camera functionality.
 * @author Phil Schatzmann
 */

class Camera {
  // esp_camera_fb_return
  struct Deleter {
    void operator()(camera_fb_t *ptr) const {
      if (ptr) {
        esp_camera_fb_return(ptr);
      }
    }
  };

 public:
  Camera() = default;
  /**
   * @brief Initialize the camera driver
   *
   * @note call camera_probe before calling this function
   *
   * This function detects and configures camera over I2C interface,
   * allocates framebuffer and DMA buffers,
   * initializes parallel I2S input, and sets up DMA descriptors.
   *
   * Currently this function can only be called once and there is
   * no way to de-initialize this module.
   *
   * @param config  Camera configuration parameters
   *
   * @return RESULT_OK on success
   */
  bool begin(const camera_config_t &config) {
    return esp_camera_init(&config) == ESP_OK;
  }

  /**
   * @brief Deinitialize the camera driver
   *
   * @return
   *      - RESULT_OK on success
   *      - ERR_INVALID_STATE if the driver hasn't been initialized yet
   */
  bool end(void) { return esp_camera_deinit() == ESP_OK; }

  /**
   * @brief Obtain unique_ptr to a frame buffer.
   *
   * @return pointer to the frame buffer
   */
  auto frameBuffer(void) {
    return std::unique_ptr<camera_fb_t, Deleter>(esp_camera_fb_get());
  }

  /**
   * @brief Save camera settings to non-volatile-storage (NVS)
   *
   * @param key   A unique nvs key name for the camera settings
   */
  bool settingsSave(const char *key) {
    return esp_camera_save_to_nvs(key) == ESP_OK;
  }

  /**
   * @brief Load camera settings from non-volatile-storage (NVS)
   *
   * @param key   A unique nvs key name for the camera settings
   */
  bool settingsLoad(const char *key) {
    return esp_camera_load_from_nvs(key) == ESP_OK;
  }

  /**
   * @brief Return the frame buffer to be reused again.
   *
   * @param fb    Pointer to the frame buffer
   */
  void returnFrameBuffer(camera_fb_t &fb) { return esp_camera_fb_return(&fb); }
};


