#pragma once

#include "Video/Video.h"
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>

namespace audio_tools {

/**
 * @brief Display image with opencv to be used on the desktop
 * @ingroup video
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class JpegOpenCV : public VideoOutput {
public:
  JpegOpenCV() = default;

  // Allocate memory and create window
  void beginFrame(size_t jpegSize) override {
    if (start == 0l)
      start = millis();
    LOGI("jpegSize: %d", (int)jpegSize);
    // prevent memory fragmentation, change size only if more memory is needed
    if (img_vector.size() < jpegSize) {
      img_vector.resize(jpegSize);
    }
    this->pos = 0;
    this->open = jpegSize;
    this->size = jpegSize;

    if (create_window) {
      create_window = false;
      // create image window named "My Image"
      cv::namedWindow(window);
    }
  }

  /// dipsplay a single jpeg image, provides the milliseconds since the begin
  /// frame to calculate the necessary delay
  uint32_t endFrame() override {
    display();
    return millis() - start;
  }

  // Add some more data to the image vector
  size_t write(const uint8_t *data, size_t len) override {
    memcpy(&img_vector[pos], data, len);
    pos += len;
    open -= len;
    return len;
  }

protected:
  bool create_window = true;
  std::vector<uint8_t> img_vector;
  const char *window = "Movie";
  size_t pos = 0;
  size_t size = 0;
  int open = 0;
  uint64_t start = 0;

  void display() {
    assert(open == 0);

    cv::Mat data(1, size, CV_8UC1, (void *)&img_vector[0]);
    // cv::InputArray input_array(img_vector);
    cv::Mat mat = cv::imdecode(data, 0);
    cv::imshow(window, mat);
    // cv::pollKey();
    cv::waitKey(1);
  }
};

} // namespace audio_tools
