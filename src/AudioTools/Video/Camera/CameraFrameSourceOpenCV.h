#pragma once

#include "AudioTools/Video/Video.h"
#include <opencv2/core/core.hpp>
#include <opencv2/videoio/videoio.hpp>
#include <opencv2/imgcodecs/imgcodecs.hpp>
#include <vector>

namespace audio_tools {

/**
 * @brief VideoFrameSource that pulls frames from an OpenCV cv::VideoCapture
 * (e.g. a USB/CSI webcam via V4L2 on Linux/Raspberry Pi, or any other
 * OpenCV-supported backend) - the desktop/Linux counterpart to
 * CameraFrameSource, which targets the ESP32 esp_camera.h driver instead.
 *
 * nextFrame() grabs a picture with cv::VideoCapture::read() and, by
 * default (videoInfo().format == VideoFormat::MJPEG), JPEG-encodes it via
 * cv::imencode() so the result matches CameraFrameSource's own
 * already-encoded-JPEG contract. setVideoInfo() with VideoFormat::RAW
 * instead returns the picture as captured - OpenCV's native 3
 * bytes/pixel BGR layout, which is what VideoFormat::RAW itself denotes -
 * with no re-encoding, e.g. to feed a VideoEncoder (H264Encoder) directly.
 * As with any VideoFrameSource, the returned pointer is only valid until
 * the next nextFrame() call.
 *
 * width/height/fps are read back from the capture device in begin() -
 * set them beforehand via setVideoInfo()/the constructor to request
 * specific values from cv::VideoCapture (best effort; not all
 * backends/devices honor every value, so the actually-applied values are
 * what ends up in videoInfo() afterwards).
 *
 * @ingroup video
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class CameraFrameSourceOpenCV : public VideoFrameSource {
 public:
  CameraFrameSourceOpenCV() = default;
  /// @param device cv::VideoCapture index (e.g. 0 for /dev/video0)
  CameraFrameSourceOpenCV(int device) : device_index(device) {}

  /// Requests width/height/fps/format from the capture device - all
  /// optional (0/UNKNOWN = leave the device's own default); actually
  /// applied on a best-effort basis in begin() via
  /// cv::VideoCapture::set(). format only selects nextFrame()'s own
  /// output (see class comment), it is not passed to OpenCV. Call before
  /// begin().
  void setVideoInfo(VideoInfo info) { this->info = info; }

  /// Use a device path/URL/GStreamer pipeline (cv::VideoCapture(const
  /// String&)) instead of a numeric index - call before begin().
  void setDevice(const char *device) { device_string = device; }

  /// Selects the capture backend (e.g. cv::CAP_V4L2) - default cv::CAP_ANY
  /// lets OpenCV pick. Call before begin().
  void setApiPreference(int api) { api_preference = api; }

  /// JPEG quality (0-100) used to encode frames when videoInfo().format is
  /// VideoFormat::MJPEG (the default) - see cv::IMWRITE_JPEG_QUALITY.
  void setJpegQuality(int quality) { jpeg_quality = quality; }

  /// Opens the capture device (cv::VideoCapture::open()), applies any
  /// width/height/fps requested via setVideoInfo(), then reads back the
  /// values actually in effect.
  bool begin() {
    bool ok = device_string != nullptr
                  ? cap.open(device_string, api_preference)
                  : cap.open(device_index, api_preference);
    if (!ok) {
      LOGE("CameraFrameSourceOpenCV: could not open capture device");
      return false;
    }
    if (info.width > 0) cap.set(cv::CAP_PROP_FRAME_WIDTH, info.width);
    if (info.height > 0) cap.set(cv::CAP_PROP_FRAME_HEIGHT, info.height);
    if (info.fps > 0) cap.set(cv::CAP_PROP_FPS, info.fps);

    info.width = (uint16_t)cap.get(cv::CAP_PROP_FRAME_WIDTH);
    info.height = (uint16_t)cap.get(cv::CAP_PROP_FRAME_HEIGHT);
    float dev_fps = (float)cap.get(cv::CAP_PROP_FPS);
    if (dev_fps > 0) info.fps = dev_fps;
    if (info.format == VideoFormat::UNKNOWN) info.format = VideoFormat::MJPEG;
    return true;
  }

  /// Releases the capture device.
  void end() { cap.release(); }

  /// Captures a new picture and, unless videoInfo().format is
  /// VideoFormat::RAW, JPEG-encodes it - nullptr (len 0) if the capture
  /// failed.
  const uint8_t *nextFrame(size_t &len) override {
    if (!cap.read(frame) || frame.empty()) {
      len = 0;
      return nullptr;
    }
    if (info.format == VideoFormat::RAW) {
      len = frame.total() * frame.elemSize();
      return frame.data;
    }
    std::vector<int> params{cv::IMWRITE_JPEG_QUALITY, jpeg_quality};
    cv::imencode(".jpg", frame, jpeg_buffer, params);
    len = jpeg_buffer.size();
    return jpeg_buffer.data();
  }

  /// Provides the width/height/fps/format determined in begin() (or
  /// requested explicitly beforehand via setVideoInfo() - see there).
  VideoInfo videoInfo() override { return info; }

 protected:
  cv::VideoCapture cap;
  cv::Mat frame;
  std::vector<uint8_t> jpeg_buffer;
  VideoInfo info;
  int device_index = 0;
  const char *device_string = nullptr;
  int api_preference = cv::CAP_ANY;
  int jpeg_quality = 90;
};

using CameraFrameSource = CameraFrameSourceOpenCV;

}  // namespace audio_tools
