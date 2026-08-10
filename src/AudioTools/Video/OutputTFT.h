#pragma once
#include "AudioTools/Video/CodecVideo.h"
#include <TFT_eSPI.h>  // https://github.com/Bodmer/TFT_eSPI

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
class OutputTFT : public VideoOutput {
 public:
  OutputTFT(TFT_eSPI &tft, VideoDecoder &decoder)
      : p_tft(&tft), p_decoder(&decoder) {}
  size_t write(const uint8_t *data, size_t len) override {
    VideoInfo info = p_decoder->videoInfo();
    p_tft->pushImage(0, 0, info.width, info.height, (uint16_t *)data);
    return len;
  }

 protected:
  TFT_eSPI *p_tft;
  VideoDecoder *p_decoder;
};

}  // namespace audio_tools