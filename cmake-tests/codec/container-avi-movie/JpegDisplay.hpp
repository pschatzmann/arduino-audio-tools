#pragma once

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>

#include "AudioCodecs/ContainerAVI.h"

/**
 * Display image with opencv
 */
class JpegDisplay : public VideoOutput {
 public:
  JpegDisplay() {
    // create image window named "My Image"
    cv::namedWindow(window);
  }
  // add some data to image vector
  size_t write(uint8_t buffer, size_t len) {
    memcpy(&img_vector[pos], buffer, len);
    pos += len;
  }

  void beginFrame(int size) override { img_vector.resize(size); }
  void endFrame() override { display(); }
  void display() {
    cv::InputArray input_array(img_vector);
    cv::Mat mat = cv::imdecode(input_array, IMREAD_COLOR);
    cv::imshow(windows, mat);
  }

 protected:
  std::vector img_vector;
  const char* window = "Movie";
  uint8_t pos = 0;
};
