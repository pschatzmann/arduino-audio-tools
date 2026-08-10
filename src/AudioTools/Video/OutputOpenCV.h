#pragma once

#include "AudioTools/Video/Video.h"
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

namespace audio_tools {

/**
 * @brief Display a video frame with OpenCV, to be used on the desktop -
 * VideoFormat::MJPEG (the default) expects one complete, already-encoded
 * JPEG image assembled across write() calls and closed off by flush(), as
 * produced by e.g. DemuxerAVI. Any other setVideoFormat() value (e.g.
 * RGB565, the common raw picture format decoders like H264Decoder produce)
 * expects one complete, already-decoded picture per write() call instead -
 * setSize() must be called too in that case, since unlike JPEG a raw
 * picture doesn't carry its own dimensions.
 * @ingroup video
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class OutputOpenCV : public VideoOutput {
public:
  OutputOpenCV() = default;

  /// Selects the format of the data passed to write() - see class comment.
  void setVideoFormat(VideoFormat format) { video_format = format; }

  /// Defines the picture size - only needed (and used) for raw, non-MJPEG
  /// formats (see class comment). Call before the first write().
  void setSize(uint16_t width, uint16_t height) {
    this->width = width;
    this->height = height;
  }

  size_t write(const uint8_t *data, size_t len) override {
    ensureWindow();
    if (video_format != VideoFormat::MJPEG) {
      displayRaw(data, len);
      return len;
    }
    // Accumulates jpeg bytes for the frame currently being assembled
    if (pos == 0) start = millis();
    // prevent memory fragmentation, change size only if more memory is needed
    if (img_vector.size() < pos + len) {
      img_vector.resize(pos + len);
    }
    memcpy(&img_vector[pos], data, len);
    pos += len;
    return len;
  }

  /// Displays the assembled jpeg image, then resets for the next frame -
  /// only relevant for VideoFormat::MJPEG; raw formats display immediately
  /// from write() instead (one complete picture per call), so this is a
  /// no-op for those.
  void flush() override {
    if (pos == 0) return;
    displayJpeg();
    pos = 0;
  }

protected:
  VideoFormat video_format = VideoFormat::MJPEG;
  uint16_t width = 0;
  uint16_t height = 0;
  bool create_window = true;
  std::vector<uint8_t> img_vector;
  const char *window = "Movie";
  size_t pos = 0;
  uint64_t start = 0;

  void ensureWindow() {
    if (create_window) {
      create_window = false;
      // create image window named "My Image"
      cv::namedWindow(window);
    }
  }

  void displayJpeg() {
    cv::Mat data(1, pos, CV_8UC1, (void *)&img_vector[0]);
    // cv::InputArray input_array(img_vector);
    cv::Mat mat = cv::imdecode(data, 0);
    cv::imshow(window, mat);
    // cv::pollKey();
    cv::waitKey(1);
  }

  /// Wraps one complete, already-decoded raw picture (no copy - OpenCV
  /// reads directly out of 'data') and converts it to OpenCV's native BGR
  /// for display. Not every VideoFormat OpenCV could conceivably display
  /// is covered here - see the switch below for exactly which ones are.
  void displayRaw(const uint8_t *data, size_t len) {
    if (width == 0 || height == 0) {
      LOGE("OutputOpenCV: setSize() was not called");
      return;
    }
    cv::Mat bgr;
    switch (video_format) {
      case VideoFormat::RGB565: {
        cv::Mat raw(height, width, CV_8UC2, (void *)data);
        cv::cvtColor(raw, bgr, cv::COLOR_BGR5652BGR);
        break;
      }
      case VideoFormat::RGB888:
      case VideoFormat::RGB666: {
        // RGB666's low 2 bits/channel are unused (left-justified in each
        // byte, see VideoFormat) - same 3-bytes/pixel memory layout as
        // RGB888 otherwise, so displays fine at slightly reduced precision
        cv::Mat raw(height, width, CV_8UC3, (void *)data);
        cv::cvtColor(raw, bgr, cv::COLOR_RGB2BGR);
        break;
      }
      case VideoFormat::YUV422: {
        cv::Mat raw(height, width, CV_8UC2, (void *)data);
        cv::cvtColor(raw, bgr, cv::COLOR_YUV2BGR_YUYV);
        break;
      }
      case VideoFormat::I420: {
        cv::Mat raw(height + height / 2, width, CV_8UC1, (void *)data);
        cv::cvtColor(raw, bgr, cv::COLOR_YUV2BGR_I420);
        break;
      }
      default:
        LOGE("OutputOpenCV: unsupported VideoFormat %d", (int)video_format);
        return;
    }
    cv::imshow(window, bgr);
    cv::waitKey(1);
  }
};

} // namespace audio_tools
