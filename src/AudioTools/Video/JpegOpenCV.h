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

  // Accumulates jpeg bytes for the frame currently being assembled
  size_t write(const uint8_t *data, size_t len) override {
    if (pos == 0) start = millis();
    // prevent memory fragmentation, change size only if more memory is needed
    if (img_vector.size() < pos + len) {
      img_vector.resize(pos + len);
    }
    if (create_window) {
      create_window = false;
      // create image window named "My Image"
      cv::namedWindow(window);
    }
    memcpy(&img_vector[pos], data, len);
    pos += len;
    return len;
  }

  /// Displays the assembled jpeg image, then resets for the next frame
  void flush() override {
    if (pos == 0) return;
    display();
    pos = 0;
  }

protected:
  bool create_window = true;
  std::vector<uint8_t> img_vector;
  const char *window = "Movie";
  size_t pos = 0;
  uint64_t start = 0;

  void display() {
    cv::Mat data(1, pos, CV_8UC1, (void *)&img_vector[0]);
    // cv::InputArray input_array(img_vector);
    cv::Mat mat = cv::imdecode(data, 0);
    cv::imshow(window, mat);
    // cv::pollKey();
    cv::waitKey(1);
  }
};

} // namespace audio_tools
