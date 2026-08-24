#pragma once

#include <algorithm>  // std::min

#include "AudioTools/CoreAudio/AudioBasic/Collections/Vector.h"
#include "AudioTools/Video/CodecVideo.h"
#include "TinyJPEGDecoder.h"  // https://github.com/pschatzmann/TinyJPEG

/**
 * @defgroup mjpeg MJPEG
 * @ingroup codecs
 * @ingroup video
 * @brief Motion-JPEG decoding using https://github.com/pschatzmann/TinyJPEG
 */

namespace audio_tools {

/**
 * @brief Motion-JPEG video decoder: wraps TinyJPEGDecoder
 * (https://github.com/pschatzmann/TinyJPEG, a header-only port of ChaN's
 * TJpgDec) as a VideoDecoder, so it can be plugged directly into e.g.
 * DemuxerAVI::setOutputVideo()/DemuxerMP4::setOutputVideo() - the same
 * role H264Decoder (CodecH264.h) and MPGDecoder (CodecMPG.h) play for
 * their own codecs. Named "MJPEGDecoder" rather than reusing
 * TinyJPEGDecoder's own name to avoid shadowing that class from within
 * this one.
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
 * Owns a plain, private tinyjpeg::TinyJPEGDecoder member - unlike
 * Bodmer's JPEGDecoder (this class's previous backend), TinyJPEGDecoder
 * threads its per-decode context through the decoder's own instance
 * instead of a global singleton, so multiple MJPEGDecoder instances (or
 * any other concurrent TinyJPEGDecoder use elsewhere in the same sketch)
 * coexist safely.
 *
 * @ingroup mjpeg
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class MJPEGDecoder : public VideoDecoder, public VideoInfoSource {
 public:
  MJPEGDecoder() {
    decoder_.setCallback(onBlock);
    decoder_.setUserData(this);
    decoder_.setSwapBytes(byte_swap);
  }
  MJPEGDecoder(Print &out) : MJPEGDecoder() { setOutput(out); }
  MJPEGDecoder(VideoOutput &out) : MJPEGDecoder() { setOutput(out); }

  /// Defines the target the decoded picture is written to, one write()
  /// call per decoded picture (RGB565 - the only format TinyJPEGDecoder
  /// produces).
  void setOutput(Print &out) override { p_out = &out; }
  void setOutput(VideoOutput &out) override { p_out_video = &out; }

  /// TinyJPEGDecoder only ever produces RGB565 - any other value is
  /// logged and ignored.
  void setVideoFormat(VideoFormat format) override {
    if (format != VideoFormat::RGB565) {
      LOGW("MJPEGDecoder: unsupported VideoFormat %d - only RGB565",
           (int)format);
      return;
    }
  }

  /// Swaps the two bytes of every decoded RGB565 pixel before it reaches
  /// setOutput()'s target - on by default. TinyJPEGDecoder packs each
  /// pixel in the CPU's native uint16_t byte order (little-endian on
  /// ESP32/most Arduino targets) unless told otherwise, but SPI TFT
  /// panels (ILI9341 etc.) expect RGB565 transmitted big-endian - without
  /// the swap, colors come out visibly wrong (channels scrambled, not
  /// just a hue shift). Forwards straight to
  /// tinyjpeg::TinyJPEGDecoder::setSwapBytes(). Turn off only if your
  /// VideoOutput target already performs its own byte-swap (e.g. some
  /// display libraries' pushColors() variants).
  void setByteSwap(bool active) {
    byte_swap = active;
    decoder_.setSwapBytes(active);
  }
  bool byteSwap() const { return byte_swap; }

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
  /// Reads the picture's width/height first (getJpgSize() - a cheap,
  /// header-only parse) so frame_buffer can be sized before the
  /// per-MCU-block callback (onBlock()/writeBlock()) starts firing,
  /// since TinyJPEGDecoder streams blocks out via callback as it decodes
  /// rather than handing back a whole decoded picture at once.
  void flush() override {
    if (pos == 0) return;
    if (p_out == nullptr && p_out_video == nullptr) {
      pos = 0;
      return;
    }
    uint16_t w = 0, h = 0;
    if (decoder_.getJpgSize(&w, &h, img_vector.data(), pos) != JDR_OK ||
        w == 0 || h == 0) {
      LOGE("MJPEGDecoder: could not determine JPEG size");
      pos = 0;
      return;
    }
    width_ = w;
    height_ = h;
    size_t needed = (size_t)width_ * height_ * 2;
    if (frame_buffer.size() < needed) {
      frame_buffer.resize(needed);
      if (frame_buffer.data() == nullptr) {
        LOGE("MJPEGDecoder: frame buffer allocation failed");
        pos = 0;
        return;
      }
    }
    if (decoder_.drawJpg(0, 0, img_vector.data(), pos) == JDR_OK) {
      writeToOutput(frame_buffer.data(), needed);
    }
    pos = 0;
  }

  /// See VideoOutput::isKeyFrame() - always true: Motion-JPEG is an
  /// all-intra format, every frame is a complete, independently decodable
  /// image with no inter-frame prediction (the same property that makes an
  /// H.264 IDR frame or an MPEG-1 I-picture "key", just true of every
  /// frame here instead of periodically). This matters beyond
  /// classification - VideoAudioSyncTask's awaiting_keyframe recovery
  /// latch (set after a resync) only clears once a frame reports
  /// isKeyFrame()==true; returning false here would leave every frame
  /// classified as non-key, so once any resync ever fired, that latch
  /// would never clear and rendering would silently stall forever.
  bool isKeyFrame(const uint8_t *data, size_t len) override { return true; }

 protected:
  Vector<uint8_t> img_vector;
  size_t pos = 0;
  uint64_t start_ms = 0;
  tinyjpeg::TinyJPEGDecoder decoder_;
  Print *p_out = nullptr;
  VideoOutput *p_out_video = nullptr;
  uint16_t width_ = 0;
  uint16_t height_ = 0;
  bool byte_swap = true;  // see setByteSwap()
  Vector<uint8_t> frame_buffer;  // tightly packed RGB565, 2 bytes/pixel

  /// Per-MCU-block callback registered on decoder_ - see
  /// tinyjpeg::SketchCallback. `decoder`'s getUserData() is this instance
  /// (set in the constructor), routing back to writeBlock().
  static bool onBlock(tinyjpeg::TinyJPEGDecoder &decoder, int16_t x,
                       int16_t y, uint16_t w, uint16_t h, uint16_t *data) {
    return static_cast<MJPEGDecoder *>(decoder.getUserData())
        ->writeBlock(x, y, w, h, data);
  }

  /// Copies one decoded MCU block into frame_buffer at (x, y) - a
  /// right/bottom edge block can be narrower/shorter than a full MCU
  /// (w/h already reflect that; TinyJPEGDecoder, unlike the old
  /// JPEGDecoder backend, never hands back an over-wide/over-tall edge
  /// block in the first place), so no separate clamping is needed beyond
  /// the defensive min() below.
  bool writeBlock(int16_t x, int16_t y, uint16_t w, uint16_t h,
                   uint16_t *data) {
    if (x < 0 || y < 0) return true;
    uint32_t max_x = width_;
    uint32_t max_y = height_;
    if ((uint32_t)x >= max_x || (uint32_t)y >= max_y) return true;
    uint32_t win_w = std::min((uint32_t)w, max_x - (uint32_t)x);
    uint32_t win_h = std::min((uint32_t)h, max_y - (uint32_t)y);
    uint16_t *dst = (uint16_t *)frame_buffer.data();
    for (uint32_t row = 0; row < win_h; row++) {
      memcpy(dst + ((uint32_t)y + row) * max_x + (uint32_t)x,
             data + row * w, win_w * sizeof(uint16_t));
    }
    return true;
  }

  size_t writeToOutput(const uint8_t *data, size_t len) {
    size_t result = 0;
    if (p_out != nullptr) result = p_out->write(data, len);
    if (p_out_video != nullptr) result += p_out_video->write(data, len);
    return result;
  }
};

}  // namespace audio_tools
