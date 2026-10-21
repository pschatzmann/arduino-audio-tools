#pragma once
#include "AudioTools/Video/Video.h"
#include "TinyCamera.h"

namespace audio_tools {

/**
 * @brief VideoFrameSource that pulls frames from a camera via TinyCamera
 * (https://github.com/pschatzmann/TinyCamera) - so it works on every
 * platform TinyCamera supports (ESP32 via esp32-camera, ESP32-P4 MIPI-CSI
 * via ESP_Video, RP2040, STM32, desktop). nextFrame() returns the buffer of
 * a freshly captured frame, returning the previous one to the driver first;
 * as with any VideoFrameSource, the returned pointer is only valid until
 * the next nextFrame() call.
 *
 * begin() captures one probe frame to determine width/height/format, so
 * videoInfo() is complete right after it (as VideoMuxer::begin() requires)
 * without any frame_size lookup table. Only the frame rate has to be given,
 * since a camera has no fixed one: use the rate you actually want to
 * capture (and encode) at.
 *
 * @ingroup video
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class CameraFrameSourceTinyCamera : public VideoFrameSource {
 public:
  CameraFrameSourceTinyCamera() = default;

  /// Initializes the camera and captures a probe frame to determine
  /// width/height/format - see the class comment. @param fps frame rate
  /// reported by videoInfo().
  bool begin(const camera_config_t &config, float fps) {
    info.fps = fps;
    if (!camera_.begin(config)) return false;
    tiny_camera::TinyCameraFrame probe = camera_.captureFrame();
    if (!probe) {
      LOGE("CameraFrameSourceTinyCamera: probe capture failed");
      return false;
    }
    info.width = probe.width();
    info.height = probe.height();
    info.format = toVideoFormat(probe.format());
    info.frame_size = videoFrameSizeBytes(info.format, info.width, info.height);
    LOGI("CameraFrameSourceTinyCamera: %dx%d, format %d", (int)info.width,
         (int)info.height, (int)info.format);
    return true;
  }

  /// Returns the last captured frame (if any) and releases the camera.
  void end() {
    frame_.release();
    camera_.end();
  }

  /// Returns the previous frame (if still held), captures a new one and
  /// returns its buffer - nullptr (len 0) if the capture failed.
  const uint8_t *nextFrame(size_t &len) override {
    frame_.release();
    frame_ = camera_.captureFrame();
    if (!frame_) {
      len = 0;
      return nullptr;
    }
    len = frame_.size();
    return frame_.data();
  }

  /// Width/height/format determined by begin(), and the fps given to it.
  VideoInfo videoInfo() override { return info; }

  /// Access to the camera, e.g. for sensor tuning (camera().sensor()).
  tiny_camera::TinyCamera &camera() { return camera_; }

 protected:
  tiny_camera::TinyCamera camera_;
  tiny_camera::TinyCameraFrame frame_;
  VideoInfo info;

  static VideoFormat toVideoFormat(pixformat_t format) {
    switch (format) {
      case PIXFORMAT_RGB565:
        return VideoFormat::RGB565;
      case PIXFORMAT_YUV422:
        return VideoFormat::YUV422;
      case PIXFORMAT_JPEG:
        return VideoFormat::MJPEG;
#if defined(TINY_CAMERA_ESP_VIDEO)
      case PIXFORMAT_RGB888:
        return VideoFormat::RGB888;
#endif
      default:
        return VideoFormat::UNKNOWN;
    }
  }
};

}  // namespace audio_tools
