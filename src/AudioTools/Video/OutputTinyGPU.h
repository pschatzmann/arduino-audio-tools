#pragma once
#include <TinyGPU.h>  // https://github.com/pschatzmann/TinyGPU
#include <TinyGPU/DisplayDriverSPI.h>  // ILI9341Driver
#include <TinyGPU/LCDBoards.h>  // LCDBoard
#include <TinyGPU/SurfaceWithExternalBuffer.h>  // zero-copy write() path

#include "AudioTools/CoreAudio/AudioBasic/Collections/Vector.h"
#include "AudioTools/Video/CodecVideo.h"

namespace audio_tools {

/**
 * @brief Bridges VideoDecoder's write() calls to a TinyGPU DisplayDriver
 * (e.g. ILI9341Driver, or any board wrapped in an LCDBoard - see the
 * LCDBoard constructor) - VideoDecoder always hands over one complete
 * frame per write() call (see its class comment), so this can push the
 * whole frame in one go.
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
  /// @param driver Any TinyGPU DisplayDriver<RGB565> - e.g. ILI9341Driver.
  /// Its own width()/height() (rotation-aware - see DisplayDriver.h) are
  /// queried fresh wherever needed (scale_to_fit's target, clearScreen()'s
  /// band loop), so a later setRotation() call is picked up automatically.
  /// @param bandHeight height (in pixels) of the strip used to clear the
  /// screen in begin() - kept small (default 40) so the clear buffer stays
  /// a small, single allocation instead of a full-screen framebuffer; see
  /// TinyGPU's bouncing-ball example for the same technique.
  OutputTinyGPU(DisplayDriver<RGB565>& driver, int pinBacklight,
                int bandHeight = 40)
      : tftDriver(driver),
        kPinBacklight(pinBacklight),
        kBandHeight(bandHeight) {}

  /// @param board Bundles the display driver (and, on concrete boards,
  /// touch/I2S/LED pin assignments) behind one begin() call - see
  /// TinyGPU's LCDBoards.h. begin() calls board.begin() instead of
  /// separately managing a backlight pin and the driver's own begin(),
  /// since the board's begin() already brings up its backlight/bus/
  /// display (and touch, if present) together.
  OutputTinyGPU(LCDBoard& board, int bandHeight = 40)
      : tftDriver(board.display()),
        kPinBacklight(-1),
        kBandHeight(bandHeight),
        p_board(&board) {}

  bool begin() {
    if (p_board != nullptr) {
      if (!p_board->begin()) return false;
    } else {
      if (kPinBacklight >= 0) {
        pinMode(kPinBacklight, OUTPUT);
        digitalWrite(kPinBacklight, HIGH);
      }
      tftDriver.begin();
    }
    clearScreen();

    return true;
  }

  /// Changes the panel's rotation after begin() - forwards to the
  /// underlying driver (see DisplayDriver::setRotation()); a no-op if
  /// that driver doesn't support rotation. Nothing else to do here:
  /// scale_to_fit's target and clearScreen()'s band loop read
  /// tftDriver.width()/height() fresh each time, and those are already
  /// rotation-aware (see DisplayDriver.h), so they pick up the change
  /// automatically.
  void setRotation(DisplayRotation rotation) { tftDriver.setRotation(rotation); }

  /// Defines the source of the video information (width, height, fps, format)
  void setVideoInfoSource(VideoInfoSource& source) { p_info = &source; }

  /// Defines the video format and dimensions - call before begin() if you
  void setVideoInfo(VideoInfo info) { info_ = info; }

  /// When enabled, a decoded frame whose size doesn't match the panel's
  /// current width/height (tftDriver.width()/height(), rotation-aware -
  /// see DisplayDriver.h) is nearest-neighbor scaled to fill it exactly -
  /// the aspect ratio is NOT preserved, the source is stretched to the
  /// panel's own aspect ratio. Off by default (direct 1:1 write at (0,0),
  /// e.g. a QCIF video shown at native size in the corner of a larger
  /// panel). Scaling is done one output row at a time (no full-frame
  /// scratch buffer) to keep RAM use independent of resolution.
  void setScaleToFit(bool enable) { scale_to_fit = enable; }

  /// Skips the actual panel refresh (the SPI-bound part) on the next and
  /// subsequent write() calls, while still validating the frame and
  /// returning len as if it had been shown - used to recover from falling
  /// behind the playback schedule without breaking a codec's decode state
  /// (the caller must still get a normal "success" return so its own
  /// frame-complete bookkeeping isn't disturbed). Call again with false to
  /// resume rendering.
  void setSkipRender(bool skip) override { skip_render = skip; }

  size_t write(const uint8_t* data, size_t len) override {
    auto start = millis();
    if (p_info != nullptr) {
      info_ = p_info->videoInfo();
    }

    if (info_.width == 0 || info_.height == 0) {
      LOGE("OutputTinyGPU: invalid video size: %d x %d", (int)info_.width,
           (int)info_.height);
      return 0;
    }

    bool isI420 = info_.format == VideoFormat::I420;
    // I420: one byte/pixel Y plane, plus two quarter-size (w/2 x h/2)
    // U/V planes - the standard w*h*3/2 packed-planar size.
    size_t needed = isI420 ? (size_t)info_.width * info_.height * 3 / 2
                            : (size_t)info_.width * info_.height * sizeof(RGB565);
    if (len < needed) {
      LOGE("OutputTinyGPU: frame too small: %d < %d bytes", (int)len,
           (int)needed);
      return 0;
    }

    if (skip_render) {
      return len;
    }

    if (isI420) {
      // Unlike the RGB565 path below, this always goes row-by-row (even
      // when scale_to_fit is off) - I420's planar layout isn't a raw byte
      // pattern the driver can view directly the way packed RGB565 is, so
      // conversion is unavoidable regardless of whether scaling also
      // happens in the same pass.
      writeI420(data, info_.width, info_.height);
      write_time_ms = millis() - start;
      return len;
    }

    if (scale_to_fit && (info_.width != (int)tftDriver.width() ||
                          info_.height != (int)tftDriver.height())) {
      writeScaled(data, info_.width, info_.height);
      write_time_ms = millis() - start;
      return len;
    }

    // View directly over the caller's buffer - no allocation, no copy.
    SurfaceWithExternalBuffer<RGB565> frame;
    frame.setExternalBuffer(const_cast<uint8_t*>(data), len);
    frame.resize(info_.width, info_.height);
    tftDriver.writeData(frame, 0, 0);
    write_time_ms = millis() - start;

    return len;
  }

  /// Nearest-neighbor scales a source frame (srcW x srcH, RGB565, tightly
  /// packed) up/down to exactly width x height, one output row at a time:
  /// build one scaled row into scale_line_, then hand it to the driver as
  /// a 1-row-tall surface. Costs one extra small SPI transaction per output
  /// row versus the unscaled single-transaction path, but needs only a
  /// single row's worth of scratch RAM regardless of resolution.
  void writeScaled(const uint8_t* data, int srcW, int srcH) {
    int width = (int)tftDriver.width();
    int height = (int)tftDriver.height();
    if ((int)scale_line_.size() != width) scale_line_.resize(width);
    const RGB565* src = (const RGB565*)data;
    for (int y = 0; y < height; y++) {
      int srcY = (int)((int64_t)y * srcH / height);
      const RGB565* srcRow = src + (size_t)srcY * srcW;
      for (int x = 0; x < width; x++) {
        int srcX = (int)((int64_t)x * srcW / width);
        scale_line_[x] = srcRow[srcX];
      }
      SurfaceWithExternalBuffer<RGB565> row;
      row.setExternalBuffer((uint8_t*)scale_line_.data(),
                             (size_t)width * sizeof(RGB565));
      row.resize(width, 1);
      tftDriver.writeData(row, 0, y);
    }
  }

  /// Converts one planar I420 frame (srcW x srcH: full-size Y plane
  /// followed by two (srcW/2)x(srcH/2) U/V planes) to RGB565 and writes it
  /// to the driver. The YUV->RGB conversion (several integer multiplies +
  /// clamps per pixel - not free) always runs exactly once per *source*
  /// pixel, into i420_rgb_buf_ at native resolution; scale_to_fit then
  /// reuses writeScaled()'s existing (conversion-free, just array
  /// indexing) RGB565 upscale path instead of redoing the YUV math once
  /// per *output* pixel - on this panel (320x240 vs a 176x144 source)
  /// that's the difference between ~25k and ~77k conversions per frame.
  void writeI420(const uint8_t* data, int srcW, int srcH) {
    const uint8_t* yPlane = data;
    const uint8_t* uPlane = yPlane + (size_t)srcW * srcH;
    const uint8_t* vPlane = uPlane + (size_t)(srcW / 2) * (srcH / 2);
    size_t srcPixels = (size_t)srcW * srcH;
    if (i420_rgb_buf_.size() != srcPixels) i420_rgb_buf_.resize(srcPixels);
    for (int y = 0; y < srcH; y++) {
      const uint8_t* yRow = yPlane + (size_t)y * srcW;
      const uint8_t* uRow = uPlane + (size_t)(y / 2) * (srcW / 2);
      const uint8_t* vRow = vPlane + (size_t)(y / 2) * (srcW / 2);
      RGB565* dstRow = i420_rgb_buf_.data() + (size_t)y * srcW;
      for (int x = 0; x < srcW; x++) {
        dstRow[x] = yuvToRgb565(yRow[x], uRow[x / 2], vRow[x / 2]);
      }
    }

    if (scale_to_fit && (srcW != (int)tftDriver.width() ||
                          srcH != (int)tftDriver.height())) {
      writeScaled((const uint8_t*)i420_rgb_buf_.data(), srcW, srcH);
      return;
    }
    SurfaceWithExternalBuffer<RGB565> frame;
    frame.setExternalBuffer((uint8_t*)i420_rgb_buf_.data(),
                             srcPixels * sizeof(RGB565));
    frame.resize(srcW, srcH);
    tftDriver.writeData(frame, 0, 0);
  }

  /// BT.601 (limited-range) integer YUV->RGB565 conversion - the standard
  /// fixed-point formula (avoids per-pixel float math), same coefficients
  /// used by most embedded YUV converters.
  static RGB565 yuvToRgb565(uint8_t Y, uint8_t U, uint8_t V) {
    int c = (int)Y - 16;
    int d = (int)U - 128;
    int e = (int)V - 128;
    int r = (298 * c + 409 * e + 128) >> 8;
    int g = (298 * c - 100 * d - 208 * e + 128) >> 8;
    int b = (298 * c + 516 * d + 128) >> 8;
    r = r < 0 ? 0 : (r > 255 ? 255 : r);
    g = g < 0 ? 0 : (g > 255 ? 255 : g);
    b = b < 0 ? 0 : (b > 255 ? 255 : b);
    return RGB565((uint8_t)r, (uint8_t)g, (uint8_t)b);
  }

  void clearScreen() {
    int width = (int)tftDriver.width();
    int height = (int)tftDriver.height();
    SurfaceRGB565 band(width, kBandHeight, FontRGB565);
    band.begin();
    band.clear(RGB565(0, 0, 0));
    for (int y = 0; y < height; y += kBandHeight) {
      tftDriver.writeData(band, 0, y);
    }
    band.end();
  }

  uint32_t getWriteTimeMs() const { return write_time_ms; }

 protected:
  int kPinBacklight;
  int kBandHeight;
  VideoInfoSource* p_info = nullptr;
  VideoInfo info_;
  DisplayDriver<RGB565>& tftDriver;
  LCDBoard* p_board = nullptr;
  bool scale_to_fit = false;
  bool skip_render = false;
  Vector<RGB565> scale_line_;
  /// Native-resolution scratch buffer for writeI420()'s YUV->RGB565
  /// conversion - unlike scale_line_ (one row), this holds a full frame,
  /// since the conversion must run before scaling can reuse writeScaled().
  /// Only allocated once an I420 frame is actually written.
  Vector<RGB565> i420_rgb_buf_;
  uint32_t write_time_ms = 0;
};

}  // namespace audio_tools
