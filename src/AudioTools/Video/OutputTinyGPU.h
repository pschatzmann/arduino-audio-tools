#pragma once
#include <TinyGPU.h>  // https://github.com/pschatzmann/TinyGPU
#include <TinyGPU/DisplayDriverSPI.h>  // ILI9341Driver
#include <TinyGPU/SurfaceWithExternalBuffer.h>  // zero-copy write() path

#include "AudioTools/Video/CodecVideo.h"

namespace audio_tools {

/**
 * @brief Bridges VideoDecoder's write() calls to a TinyGPU ILI9341Driver -
 * VideoDecoder always hands over one complete frame per write() call (see
 * its class comment), so this can push the whole frame in one go.
 * Usually the data is in RGB565 format, but other formats are supported as well
 *
 * write() never allocates or copies a full-frame buffer of its own: it
 * wraps the caller's already-decoded frame in a SurfaceWithExternalBuffer
 * (a TinyGPU view over externally-owned memory) and hands that straight to
 * the display driver, so RAM use stays independent of the frame size -
 * only clearScreen()'s small band buffer (kBandHeight rows) is actually
 * owned/allocated here.
 * @ingroup video
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class OutputTinyGPU : public VideoOutput {
 public:
  /// @param bandHeight height (in pixels) of the strip used to clear the
  /// screen in begin() - kept small (default 40) so the clear buffer stays
  /// a small, single allocation instead of a full-screen framebuffer; see
  /// TinyGPU's bouncing-ball example for the same technique.
  OutputTinyGPU(ILI9341Driver<RGB565>& driver, int width, int height,
                int pinBacklight, int bandHeight = 40)
      : tftDriver(driver),
        width(width),
        height(height),
        kPinBacklight(pinBacklight),
        kBandHeight(bandHeight) {}

  bool begin() {
    if (kPinBacklight >= 0) {
      pinMode(kPinBacklight, OUTPUT);
      digitalWrite(kPinBacklight, HIGH);
    }
    tftDriver.begin();
    clearScreen();

    return true;
  }

  /// Defines the source of the video information (width, height, fps, format)
  void setVideoInfoSource(VideoInfoSource& source) { p_info = &source; }

  /// Defines the video format and dimensions - call before begin() if you
  void setVideoInfo(VideoInfo info) { info_ = info; }

  size_t write(const uint8_t* data, size_t len) override {
    if (p_info != nullptr) {
      info_ = p_info->videoInfo();
    }

    if (info_.width == 0 || info_.height == 0) {
      LOGE("OutputTinyGPU: invalid video size: %d x %d", (int)info_.width,
           (int)info_.height);
      return 0;
    }

    size_t needed = (size_t)info_.width * info_.height * sizeof(RGB565);
    if (len < needed) {
      LOGE("OutputTinyGPU: frame too small: %d < %d bytes", (int)len,
           (int)needed);
      return 0;
    }

    // View directly over the caller's buffer - no allocation, no copy.
    SurfaceWithExternalBuffer<RGB565> frame;
    frame.setExternalBuffer(const_cast<uint8_t*>(data), len);
    frame.resize(info_.width, info_.height);
    tftDriver.writeData(frame, 0, 0);

    return len;
  }

  void clearScreen() {
    SurfaceRGB565 band(width, kBandHeight, FontRGB565);
    band.begin();
    band.clear(RGB565(0, 0, 0));
    for (int y = 0; y < height; y += kBandHeight) {
      tftDriver.writeData(band, 0, y);
    }
    band.end();
  }

 protected:
  int width = 320;
  int height = 240;
  int kPinBacklight;
  int kBandHeight;
  VideoInfoSource* p_info = nullptr;
  VideoInfo info_;
  ILI9341Driver<RGB565>& tftDriver;
};

}  // namespace audio_tools
