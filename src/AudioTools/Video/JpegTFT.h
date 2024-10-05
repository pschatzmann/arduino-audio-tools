#pragma once
#include "AudioTools/CoreAudio/AudioBasic/Collections/Vector.h"
#include "JPEGDecoder.h" // https://github.com/Bodmer/JPEGDecoder
#include "TFT_eSPI.h"    // https://github.com/Bodmer/TFT_eSPI
#include "Video/Video.h"
#include <algorithm> // std::min

namespace audio_tools {

/**
 * @brief Display jpeg image using https://github.com/Bodmer/TFT_eSPI and
 * https://github.com/Bodmer/JPEGDecoder
 * @ingroup video
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class JpegTFT : public VideoOutput {
public:
  JpegTFT(TFT_eSPI &TFTscreen) { p_screen = &TFTscreen; }

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
  }

  /// dipsplay a single jpeg image, provides the milliseconds since the begin
  /// frame to calculate the necessary delay
  uint32_t endFrame() override {
    assert(open == 0);
    jpeg_decoder.decodeArray((const uint8_t *)&img_vector[0], size);
    renderJPEG(0, 0);
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
  Vector<uint8_t> img_vector;
  size_t pos = 0;
  size_t size = 0;
  int open = 0;
  uint64_t start = 0;
  JPEGDecoder jpeg_decoder;
  TFT_eSPI *p_screen = nullptr;

  //====================================================================================
  //   Decode and paint onto the TFT screen
  //====================================================================================
  uint32_t renderJPEG(int xpos, int ypos) {
    // retrieve infomration about the image
    uint16_t *pImg;
    uint16_t mcu_w = jpeg_decoder.MCUWidth;
    uint16_t mcu_h = jpeg_decoder.MCUHeight;
    uint32_t max_x = jpeg_decoder.width;
    uint32_t max_y = jpeg_decoder.height;

    // Jpeg images are draw as a set of image block (tiles) called Minimum
    // Coding Units (MCUs) Typically these MCUs are 16x16 pixel blocks Determine
    // the width and height of the right and bottom edge image blocks
    uint32_t min_w = std::min((uint32_t)mcu_w, max_x % mcu_w);
    uint32_t min_h = std::min((uint32_t)mcu_h, max_y % mcu_h);

    // save the current image block size
    uint32_t win_w = mcu_w;
    uint32_t win_h = mcu_h;

    // record the current time so we can measure how long it takes to draw an
    // image
    uint32_t drawTime = millis();

    // save the coordinate of the right and bottom edges to assist image
    // cropping to the screen size
    max_x += xpos;
    max_y += ypos;

    // read each MCU block until there are no more
    while (jpeg_decoder.read()) {
      // save a pointer to the image block
      pImg = jpeg_decoder.pImage;

      // calculate where the image block should be drawn on the screen
      int mcu_x = jpeg_decoder.MCUx * mcu_w + xpos;
      int mcu_y = jpeg_decoder.MCUy * mcu_h + ypos;

      // check if the image block size needs to be changed for the right and
      // bottom edges
      if (mcu_x + mcu_w <= max_x)
        win_w = mcu_w;
      else
        win_w = min_w;
      if (mcu_y + mcu_h <= max_y)
        win_h = mcu_h;
      else
        win_h = min_h;

      // calculate how many pixels must be drawn
      uint32_t mcu_pixels = win_w * win_h;

      // draw image block if it will fit on the screen
      if ((mcu_x + win_w) <= p_screen->width() &&
          (mcu_y + win_h) <= p_screen->height()) {
        // open a window onto the screen to paint the pixels into
        // p_screen->setAddrWindow(mcu_x, mcu_y, mcu_x + win_w - 1, mcu_y +
        // win_h - 1);
        p_screen->setAddrWindow(mcu_x, mcu_y, mcu_x + win_w - 1,
                                mcu_y + win_h - 1);
        // push all the image block pixels to the screen
        while (mcu_pixels--)
          p_screen->pushColor(*pImg++); // Send to TFT 16 bits at a time
      }

      // stop drawing blocks if the bottom of the screen has been reached
      // the abort function will close the file
      else if ((mcu_y + win_h) >= p_screen->height())
        jpeg_decoder.abort();
    }

    // calculate how long it took to draw the image
    drawTime = millis() - drawTime; // Calculate the time it took
    return drawTime;
  }
};  // JpegTFT

} // namespace audio_tools
