#pragma once
#include "AudioTools/Video/Video.h"
#include "esp_camera.h"

namespace audio_tools {

/**
 * @brief VideoFrameSource that pulls frames from the ESP32 camera
 * (esp_camera.h, the Arduino-ESP32 "esp32-camera" component) - nextFrame()
 * returns esp_camera_fb_get()'s buffer, releasing the previous frame
 * (esp_camera_fb_return()) first; as with any VideoFrameSource, the
 * returned pointer is only valid until the next nextFrame() call.
 *
 * width/height/fps/format (see videoInfo()) must be configured explicitly
 * - via the constructor or setVideoInfo() - matching camera_config_t's own
 * frame_size/pixel_format, since VideoFrameSource's contract requires them
 * to be known before any frame has actually been captured (e.g. by
 * VideoMuxerWithTasks::begin(), which configures its Muxer from them).
 *
 * @ingroup video
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class CameraFrameSource : public VideoFrameSource {
 public:
  CameraFrameSource() = default;
  /// @param info width/height/fps/format reported by videoInfo() - must
  /// match camera_config_t's own frame_size/pixel_format (this class has
  /// no lookup table from one to the other).
  CameraFrameSource(VideoInfo info) : info(info) {}

  /// Defines the camera_config_t esp_camera_init() is called with - see
  /// the esp32-camera component's own docs for the individual fields.
  /// Call before begin().
  void setCameraConfig(camera_config_t config) { cfg = config; }

  /// Defines the width/height/fps/format reported by videoInfo() - see
  /// the constructor. Call before begin().
  void setVideoInfo(VideoInfo info) { this->info = info; }

  /// Initializes the camera (esp_camera_init()).
  bool begin() { return esp_camera_init(&cfg) == ESP_OK; }

  /// Releases the last captured frame buffer (if any) and deinitializes
  /// the camera.
  void end() {
    releaseFrame();
    esp_camera_deinit();
  }

  /// Releases the previous frame (if still held), captures a new one via
  /// esp_camera_fb_get(), and returns its buffer - nullptr (len 0) if the
  /// camera capture failed.
  const uint8_t *nextFrame(size_t &len) override {
    releaseFrame();
    fb = esp_camera_fb_get();
    if (fb == nullptr) {
      len = 0;
      return nullptr;
    }
    len = fb->len;
    return fb->buf;
  }

  /// Provides the width/height/fps/format configured via the constructor
  /// or setVideoInfo().
  VideoInfo videoInfo() override { return info; }

 protected:
  camera_config_t cfg{};
  camera_fb_t *fb = nullptr;
  VideoInfo info;

  void releaseFrame() {
    if (fb != nullptr) {
      esp_camera_fb_return(fb);
      fb = nullptr;
    }
  }
};

using CameraFrameSource = CameraFrameSourceESP32;

}  // namespace audio_tools
