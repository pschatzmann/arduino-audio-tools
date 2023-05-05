#pragma once

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>

#include "AudioCodecs/ContainerAVI.h"

/**
 * Display image with opencv
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class JpegDisplayOpenCV : public VideoOutput {
 public:
  JpegDisplay() = default;

  // add some data to image vector
  size_t write(const uint8_t* buffer, size_t byteCount) override {
    memcpy(&img_vector[pos], buffer, byteCount);
    pos += byteCount;
    open -= byteCount;
    return byteCount;
  }

  void beginFrame(size_t size) override {
    LOGI("jpegSize: %d", (int)size);
    img_vector.resize(size);
    pos = 0;
    open = size;

    if (create_window) {
      create_window = false;
      // create image window named "My Image"
      cv::namedWindow(window);
    }
  }

  void endFrame() override { display(); }

 protected:
  bool create_window = true;
  std::vector<uint8_t> img_vector;
  const char* window = "Movie";
  size_t pos = 0;
  size_t open = 0;

  void display() {
    assert(open == 0);

    cv::Mat data(1, (int)img_vector.size(), CV_8UC1, (void*)&img_vector[0]);
    // cv::InputArray input_array(img_vector);
    cv::Mat mat = cv::imdecode(data, 0);
    cv::imshow(window, mat);
    //cv::pollKey();
    cv::waitKey(1);
  }
};
