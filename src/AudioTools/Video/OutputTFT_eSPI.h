#pragma once
#include <TFT_eSPI.h>  // https://github.com/Bodmer/TFT_eSPI

#include "AudioTools/Video/CodecVideo.h"

namespace audio_tools {

/**
 * @brief Bridges VideoDecoder's write() calls to the
 * TFT - VideoDecoder always hands over one complete frame per write() call
 * (see its class comment), so this can push the whole frame in one go.
 * Usually the data is in RGB565 format, but other formats are supported as well
 * @ingroup video
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class OutputTFT_eSPI : public VideoOutput {
 public:
  OutputTFT_eSPI(TFT_eSPI& tft) : tft_(tft) {}

  bool begin() {
    tft_.init();
    tft_.setRotation(1);
    tft_.fillScreen(TFT_BLACK);
    return true;
  }

  /// Defines the source of the video information (width, height, fps, format)
  void setVideoInfoSource(VideoInfoSource& source) {
    p_info = &source;
  }

  /// Defines the video format and dimensions - call before begin() if you
  void setVideoInfo(VideoInfo info) {
    info_ = info;
  } 

  size_t write(const uint8_t* data, size_t len) override {
    auto start = millis();
    if (p_info != nullptr) {
      info_ = p_info->videoInfo();
    }
    if (info_.width == 0 || info_.height == 0) {
      LOGE("OutputTFT: invalid video size: %d x %d", (int)info_.width,
           (int)info_.height);
      return 0;
    }
    tft_.pushImage(0, 0, info_.width, info_.height, (uint16_t*)data);
    write_time_ms = millis() - start;
    return len;
  }

  uint32_t getWriteTimeMs() const { return write_time_ms; }


 protected:
  TFT_eSPI& tft_;
  VideoInfoSource* p_info = nullptr;
  VideoInfo info_;
  uint32_t write_time_ms = 0;
};

}  // namespace audio_tools