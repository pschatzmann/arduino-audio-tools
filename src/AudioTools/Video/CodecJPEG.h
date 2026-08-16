#pragma once

#include <algorithm>  // std::min

#include "AudioTools/CoreAudio/AudioBasic/Collections/Vector.h"
#include "AudioTools/Video/CodecVideo.h"
#include "JPEGDecoder.h"  // https://github.com/Bodmer/JPEGDecoder

/**
 * @defgroup mjpeg MJPEG
 * @ingroup codecs
 * @ingroup video
 * @brief Motion-JPEG decoding using https://github.com/Bodmer/JPEGDecoder
 */

namespace audio_tools {

/**
 * @brief Motion-JPEG video decoder: wraps JPEGDecoder
 * (https://github.com/Bodmer/JPEGDecoder) as a VideoDecoder, so it can be
 * plugged directly into e.g. DemuxerAVI::setOutputVideo()/
 * DemuxerMP4::setOutputVideo() - the same role H264Decoder (CodecH264.h)
 * and MPGDecoder (CodecMPG.h) play for their own codecs. Named
 * "MJPEGDecoder" rather than reusing JPEGDecoder's own name to avoid
 * shadowing that (global-namespace) class from within this one.
 *
 * write()/flush() implement the VideoOutput contract (see Video.h), but
 * unlike H264Decoder/MPGDecoder - which decode incrementally as Annex-B/
 * PES bytes arrive, with no separate "frame complete" signal needed - a
 * bare JPEG byte stream has no equivalent self-framing a demuxer can use to
 * decode early. write() therefore just accumulates one JPEG image's bytes,
 * and flush() (called by e.g. DemuxerAVI/DemuxerMP4 at each frame
 * boundary) decodes the assembled image directly into a full-frame RGB565
 * buffer and writes it to the target configured via setOutput(), one
 * write() call per decoded picture - same convention as H264Decoder/
 * MPGDecoder, so any VideoOutput (OutputTFT_eSPI, OutputTinyGPU,
 * OutputOpenCV, ...) works unchanged regardless of which codec produced
 * the picture.
 *
 * @ingroup mjpeg
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class MJPEGDecoder : public VideoDecoder, public VideoInfoSource {
 public:
  MJPEGDecoder() = default;
  MJPEGDecoder(Print &out) { setOutput(out); }
  MJPEGDecoder(VideoOutput &out) { setOutput(out); }

  /// Defines the target the decoded picture is written to, one write()
  /// call per decoded picture (RGB565 - the only format JPEGDecoder
  /// produces).
  void setOutput(Print &out) { p_out = &out; }
  void setOutput(VideoOutput &out) { p_out_video = &out; }

  /// JPEGDecoder only ever produces RGB565 - any other value is logged
  /// and ignored.
  void setVideoFormat(VideoFormat format) override {
    if (format != VideoFormat::RGB565) {
      LOGW("MJPEGDecoder: unsupported VideoFormat %d - only RGB565",
           (int)format);
      return;
    }
  }

  /// Reports the dimensions of the most recently decoded picture - 0
  /// before any picture has been decoded.
  VideoInfo videoInfo() override {
    VideoInfo info;
    info.format = VideoFormat::RGB565;
    info.width = width_;
    info.height = height_;
    return info;
  }

  bool begin() override { return true; }
  void end() override {}

  /// Accumulates one JPEG image's bytes - may be called more than once
  /// per frame (e.g. from a demuxer that hands over payload in pieces).
  /// Decoding happens on flush(), not here.
  size_t write(const uint8_t *data, size_t len) override {
    if (pos == 0) start_ms = millis();
    // prevent memory fragmentation, change size only if more memory is needed
    if (img_vector.size() < pos + len) {
      img_vector.resize(pos + len);
    }
    memcpy(&img_vector[pos], data, len);
    pos += len;
    return len;
  }

  /// Decodes the assembled JPEG image and writes the resulting RGB565
  /// frame to setOutput()'s target, then resets for the next frame - a
  /// no-op if write() hasn't accumulated anything since the last call.
  void flush() override {
    if (pos == 0) return;
    if (p_out == nullptr && p_out_video == nullptr) {
      pos = 0;
      return;
    }
    jpeg_decoder.decodeArray((const uint8_t *)&img_vector[0], pos);
    width_ = jpeg_decoder.width;
    height_ = jpeg_decoder.height;
    decodeToFrameBuffer();
    size_t needed = (size_t)width_ * height_ * 2;
    if (needed > 0 && frame_buffer.size() >= needed) {
      writeToOutput(frame_buffer.data(), needed);
    }
    pos = 0;
  }

 protected:
  Vector<uint8_t> img_vector;
  size_t pos = 0;
  uint64_t start_ms = 0;
  JPEGDecoder jpeg_decoder;
  Print *p_out = nullptr;
  VideoOutput *p_out_video = nullptr;
  uint16_t width_ = 0;
  uint16_t height_ = 0;
  Vector<uint8_t> frame_buffer;  // tightly packed RGB565, 2 bytes/pixel

  /// Reads every MCU block JPEGDecoder produces for the image just handed
  /// to decodeArray() and copies it into frame_buffer, row by row (source
  /// rows are MCUWidth pixels wide even for a right/bottom edge block
  /// narrower/shorter than that - only its top-left corner holds valid
  /// pixels - so a straight linear copy would misalign those edge blocks).
  void decodeToFrameBuffer() {
    uint16_t mcu_w = jpeg_decoder.MCUWidth;
    uint16_t mcu_h = jpeg_decoder.MCUHeight;
    uint32_t max_x = jpeg_decoder.width;
    uint32_t max_y = jpeg_decoder.height;
    size_t needed = (size_t)max_x * max_y * 2;
    if (frame_buffer.size() < needed) {
      frame_buffer.resize(needed);
      if (frame_buffer.data() == nullptr) {
        LOGE("MJPEGDecoder: frame buffer allocation failed");
        return;
      }
    }
    uint16_t *dst = (uint16_t *)frame_buffer.data();
    while (jpeg_decoder.read()) {
      uint16_t *src = jpeg_decoder.pImage;
      uint32_t mcu_x = jpeg_decoder.MCUx * mcu_w;
      uint32_t mcu_y = jpeg_decoder.MCUy * mcu_h;
      uint32_t win_w = std::min((uint32_t)mcu_w, max_x - mcu_x);
      uint32_t win_h = std::min((uint32_t)mcu_h, max_y - mcu_y);
      for (uint32_t row = 0; row < win_h; row++) {
        memcpy(dst + (mcu_y + row) * max_x + mcu_x, src + row * mcu_w,
               win_w * sizeof(uint16_t));
      }
    }
  }

  size_t writeToOutput(const uint8_t *data, size_t len) {
    size_t result = 0;
    if (p_out != nullptr) result = p_out->write(data, len);
    if (p_out_video != nullptr) result += p_out_video->write(data, len);
    return result;
  }
};

}  // namespace audio_tools
