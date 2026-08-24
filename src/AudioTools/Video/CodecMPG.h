#pragma once

#include <memory>
#include "AudioTools/CoreAudio/AudioBasic/Collections/Vector.h"
#include "AudioTools/Video/CodecVideo.h"

#include "TinyMPGDecoder.h"
#include "TinyMPGEncoder.h"


#ifndef MPG_DEFAULT_ALLOCATOR
#ifdef ESP32
#  define MPG_DEFAULT_ALLOCATOR tinympg::PSRAMAllocatorESP32<uint8_t>
#else
#  define MPG_DEFAULT_ALLOCATOR tinympg::StdAllocator<uint8_t>
#endif
#endif


/**
 * @defgroup mpg MPG
 * @ingroup codecs
 * @ingroup video
 * @brief MPEG-1 part 2 (video) encoding/decoding using https://github.com/pschatzmann/TinyMPG
 *
 * Dependency note:
 * - TinyMPG must be installed and visible in your include path.
 * - Arduino IDE/CLI: install TinyMPG as an Arduino library so
 *   `TinyMPGDecoder.h` and `TinyMPGEncoder.h` can be resolved.
 * - Project link: https://github.com/pschatzmann/TinyMPG
 */

namespace audio_tools {

/**
 * @brief MPEG-1 (part 2) video decoder wrapper around TinyMPGDecoder.
 *
 * The class mirrors the usage style of CodecH264.h: feed MPEG-1 bitstream
 * data via write(), and receive decoded frames on the configured output.
 *
 * Note: Requires TinyMPG (`TinyMPGDecoder.h`) to be installed and reachable
 * by the compiler include path:
 * https://github.com/pschatzmann/TinyMPG
 *
 * @ingroup mpg
 * @ingroup decoder
 */
class MPGDecoder : public VideoDecoder, public VideoInfoSource {
 public:
  MPGDecoder() { decoder_.setCallback(onFrame, this); }

  MPGDecoder(Print &out) : MPGDecoder() { setOutput(out); }

  MPGDecoder(VideoOutput &out) : MPGDecoder() { setOutput(out); }

  void setOutput(Print &out) override { p_out = &out; }
  void setOutput(VideoOutput &out) override { p_out_video = &out; }

  void setVideoFormat(VideoFormat format) override {
    switch (format) {
      case VideoFormat::RGB565:
      case VideoFormat::RGB666:
      case VideoFormat::RGB888:
      case VideoFormat::I420:
        pixel_format = format;
        break;
      default:
        LOGW("MPGDecoder: unsupported VideoFormat %d", (int)format);
        break;
    }
  }

  /// See tinympg::TinyMPGDecoder::setIgnorePFrames() - when set, P-pictures
  /// are silently discarded (no slices decoded, no callback fired, no
  /// error status) instead of being decoded; I- and B-pictures are
  /// unaffected. Off by default.
  void setIgnorePFrames(bool active) { decoder_.setIgnorePFrames(active); }
  bool ignorePFrames() const { return decoder_.ignorePFrames(); }

  /// Swaps the two bytes of every RGB565 pixel before it reaches
  /// setOutput()'s target - on by default. TinyMPGDecoder::toRGB565()
  /// (via convertYuv420ToRgb565(), TinyMPG/decoder/mpg_rgb.h) packs each
  /// pixel into a native uint16_t, which lands little-endian in memory on
  /// ESP32/most Arduino targets, but SPI TFT panels (ILI9341 etc.) expect
  /// RGB565 transmitted big-endian - without the swap, colors come out
  /// visibly wrong. Only affects VideoFormat::RGB565 - RGB666/RGB888/I420
  /// are unaffected (already byte-oriented, not packed into a uint16_t).
  /// Turn off only if your VideoOutput target already performs its own
  /// byte-swap.
  void setByteSwap(bool active) { byte_swap = active; }
  bool byteSwap() const { return byte_swap; }

  VideoInfo videoInfo() override {
    VideoInfo info;
    info.format = pixel_format;
    info.width = (uint16_t)decoder_.width();
    info.height = (uint16_t)decoder_.height();
    return info;
  }

  bool begin() override {
    decoder_.begin();
    return true;
  }

  /// Releases any picture still held back for display-order reordering
  /// (see TinyMPGDecoder's docs/decoding.md, "Frame delivery order and
  /// latency") before resetting the decoder - without this, the last
  /// anchor picture (or two, if the stream has B-pictures) of a session
  /// that ends here would never reach the frame callback at all, since
  /// write() below deliberately never triggers that release on its own
  /// (see its own comment).
  void end() override {
    decoder_.write(nullptr, 0);
    decoder_.end();
  }

  /// Forwards to TinyMPGDecoder::write() with the default isLastChunk=true
  /// ("this buffer is one whole, self-contained unit"). Correct because
  /// DemuxerMPG (ContainerMPG.h) now accumulates every fragment of a
  /// picture (across PES boundaries and its own small parse buffer) into
  /// one complete access unit before ever calling write() here - each
  /// call really does receive exactly one whole picture, never a
  /// boundary-split partial one. (Previously, before that buffering
  /// existed, DemuxerMPG forwarded video data in whatever increments
  /// happened to be buffered, and this needed isLastChunk=false to avoid
  /// TinyMPGDecoder treating a split unit as complete and silently
  /// reconstructing it from stale/wrong macroblocks - see
  /// ContainerMPG.h's own video_unit_buffer comment.)
  size_t write(const uint8_t *data, size_t len) override {
    frame_emitted = false;
    decoder_.write(data, len);
    return len;
  }

  /// Deliberately a no-op, not wired to TinyMPGDecoder in any way: this
  /// is called once per picture boundary by DemuxerMPG (ContainerMPG.h),
  /// not just at genuine end-of-stream, so treating it as "no more data
  /// is coming" (the way end() above does) would release every anchor
  /// picture the moment its own data finishes decoding instead of
  /// holding it back for correct B-picture reordering - that release only
  /// happens once end() runs, or once a genuine following picture's data
  /// arrives via write().
  void flush() override {}

  bool hasError() { return decoder_.hasError(); }

  /// See VideoOutput::isKeyFrame() - true only for an I-picture
  /// (picture_coding_type == 1); P- and B-pictures both depend on other
  /// frames, so neither counts as self-contained here.
  bool isKeyFrame(const uint8_t *data, size_t len) override {
    return isMpeg1KeyFrame(data, len);
  }

  /// See VideoOutput::hadOutput() - TinyMPGDecoder's B-picture display-
  /// order reordering means a given write() can decode a new picture
  /// without emitting it yet (held back), or emit an earlier held
  /// picture instead - either way "write()+flush() was called" doesn't
  /// reliably mean "a picture was just pushed to the output" the way it
  /// does for a synchronous decoder. Reflects whether writeFrame() (only
  /// ever invoked from TinyMPGDecoder's frame callback, i.e. only when a
  /// picture was actually ready) ran during the most recent write() call.
  bool hadOutput() const override { return frame_emitted; }

  tinympg::TinyMPGDecoder<MPG_DEFAULT_ALLOCATOR> &driver() { return decoder_; }

 protected:
  tinympg::TinyMPGDecoder<MPG_DEFAULT_ALLOCATOR> decoder_;
  Print *p_out = nullptr;
  VideoOutput *p_out_video = nullptr;
  VideoFormat pixel_format = VideoFormat::RGB565;
  bool byte_swap = true;  // see setByteSwap()
  bool frame_emitted = false;  // see hadOutput()
  Vector<uint8_t> frame_buffer;

  static void onFrame(tinympg::TinyMPGDecoder<MPG_DEFAULT_ALLOCATOR> &decoder,
                      void *userData) {
    static_cast<MPGDecoder *>(userData)->writeFrame(decoder);
  }

  void writeFrame(tinympg::TinyMPGDecoder<MPG_DEFAULT_ALLOCATOR> &decoder) {
    if (p_out == nullptr && p_out_video == nullptr) return;

    size_t w = decoder.width();
    size_t h = decoder.height();
    size_t n = 0;

    switch (pixel_format) {
      case VideoFormat::RGB565: {
        size_t needed = w * h;
        if (frame_buffer.size() < needed * 2) {
          frame_buffer.resize(needed * 2);
          if (frame_buffer.data() == nullptr) {
            LOGE("MPGDecoder: frame buffer allocation failed (RGB565)");
            return;
          }
        }
        n = decoder.toRGB565((uint16_t *)frame_buffer.data(), needed);
        if (n > 0) {
          if (byte_swap) {
            uint16_t *px = (uint16_t *)frame_buffer.data();
            for (size_t i = 0; i < n; i++) {
              uint16_t v = px[i];
              px[i] = (uint16_t)((v << 8) | (v >> 8));
            }
          }
          writeToOutput(frame_buffer.data(), n * 2);
        }
        break;
      }
      case VideoFormat::RGB888: {
        size_t needed = w * h * 3;
        if (frame_buffer.size() < needed) {
          frame_buffer.resize(needed);
          if (frame_buffer.data() == nullptr) {
            LOGE("MPGDecoder: frame buffer allocation failed (RGB888)");
            return;
          }
        }
        n = decoder.toRGB888(frame_buffer.data(), needed);
        if (n > 0) writeToOutput(frame_buffer.data(), n);
        break;
      }
      case VideoFormat::RGB666: {
        size_t needed = w * h * 3;
        if (frame_buffer.size() < needed) {
          frame_buffer.resize(needed);
          if (frame_buffer.data() == nullptr) {
            LOGE("MPGDecoder: frame buffer allocation failed (RGB666)");
            return;
          }
        }
        n = decoder.toRGB666(frame_buffer.data(), needed);
        if (n > 0) writeToOutput(frame_buffer.data(), n);
        break;
      }
      case VideoFormat::I420: {
        size_t needed = w * h + 2 * (w / 2) * (h / 2);
        if (frame_buffer.size() < needed) {
          frame_buffer.resize(needed);
          if (frame_buffer.data() == nullptr) {
            LOGE("MPGDecoder: frame buffer allocation failed (I420)");
            return;
          }
        }
        n = decoder.toYUV420(frame_buffer.data(), needed);
        if (n > 0) writeToOutput(frame_buffer.data(), n);
        break;
      }
      default:
        break;
    }
    if (n > 0) frame_emitted = true;
  }

  size_t writeToOutput(uint8_t *data, size_t len) {
    size_t result = 0;
    if (p_out != nullptr) result = p_out->write((const uint8_t *)data, len);
    if (p_out_video != nullptr) result += p_out_video->write((const uint8_t *)data, len);
    return result;
  }
};

/**
 * @brief MPEG-1 part 2 video encoder wrapper around TinyMPGEncoder.
 *
 * This mirrors the API shape of H264Encoder in CodecH264.h.
 *
 * Note: Requires TinyMPG (`TinyMPGEncoder.h`) to be installed and reachable
 * by the compiler include path:
 * https://github.com/pschatzmann/TinyMPG
 *
 * @ingroup mpg
 * @ingroup encoder
 */
class MPGEncoder : public VideoEncoder {
 public:
  MPGEncoder(int width = 0, int height = 0) {
    width_ = width;
    height_ = height;
    encoder_.setQp(8);
    if (width_ > 0 && height_ > 0) {
      encoder_.setSize(width_, height_);
      ensureBitstreamBuffer();
    }
  }

  MPGEncoder(Print &out) { setOutput(out); }

  MPGEncoder(VideoOutput &out) { setOutput(out); }

  void setOutput(Print &out) override { p_out = &out; }
  void setOutput(VideoOutput &out) { p_out_video = &out; }

  void setVideoFormat(VideoFormat format) override {
    switch (format) {
      case VideoFormat::I420:
      case VideoFormat::YUV422:
      case VideoFormat::RGB565:
      case VideoFormat::RGB666:
      case VideoFormat::RGB888:
        input_format = format;
        break;
      default:
        LOGW("MPGEncoder: unsupported VideoFormat %d", (int)format);
        break;
    }
  }

  VideoInfo videoInfo() override {
    VideoInfo info;
    info.format = VideoFormat::MPEG1;
    info.width = (uint16_t)width_;
    info.height = (uint16_t)height_;
    return info;
  }

  void setSize(int width, int height) {
    width_ = width;
    height_ = height;
    encoder_.setSize(width, height);
    ensureBitstreamBuffer();
  }

  void setQp(int qscale) { encoder_.setQp(qscale); }

  void setFrameRate(int fps) { encoder_.setFrameRate(fps); }

  void setBitstreamBufferSize(size_t bytes) {
    bitstream_buffer.resize(bytes);
    if (bitstream_buffer.data() == nullptr && bytes > 0) {
      LOGE("MPGEncoder: bitstream buffer allocation failed");
    }
  }

  bool begin() override {
    if (width_ <= 0 || height_ <= 0) {
      LOGE("MPGEncoder: invalid size - call setSize() first");
      return false;
    }
    if (!ensureBitstreamBuffer()){
      LOGE("MPGEncoder: failed to allocate bitstream buffer");
      return false;
    }
    return true;
  }

  void end() override {}

  size_t write(const uint8_t *data, size_t len) override {
    if (p_out == nullptr && p_out_video == nullptr) return 0;
    if (width_ <= 0 || height_ <= 0) {
      LOGE("MPGEncoder: invalid size - call setSize() first");
      return 0;
    }
    if (!ensureBitstreamBuffer()) {
      LOGE("MPGEncoder: bitstream buffer not available");
      return 0;
    }

    switch (input_format) {
      case VideoFormat::I420: {
        size_t y_size = (size_t)width_ * (size_t)height_;
        size_t uv_size = y_size / 4;
        size_t min_size = y_size + uv_size + uv_size;
        if (len < min_size) {
          LOGE("MPGEncoder: input buffer too small for I420 frame");
          return 0;
        }
        const uint8_t *srcY = data;
        const uint8_t *srcU = data + y_size;
        const uint8_t *srcV = data + y_size + uv_size;
        return writeOut([&](uint8_t *dst, size_t cap) {
          return encoder_.encodeFrame(srcY, srcU, srcV, dst, cap);
        });
      }
      case VideoFormat::YUV422:
        return writeOut([&](uint8_t *dst, size_t cap) {
          return encoder_.encodeFrameYuv422(data, dst, cap);
        });
      case VideoFormat::RGB565:
        return writeOut([&](uint8_t *dst, size_t cap) {
          return encoder_.encodeFrameRgb565((const uint16_t *)data, dst, cap);
        });
      case VideoFormat::RGB666:
        return writeOut([&](uint8_t *dst, size_t cap) {
          return encoder_.encodeFrameRgb666(data, dst, cap);
        });
      case VideoFormat::RGB888:
        return writeOut([&](uint8_t *dst, size_t cap) {
          return encoder_.encodeFrameRgb888(data, dst, cap);
        });
      default:
        LOGE("MPGEncoder: unsupported VideoFormat %d", (int)input_format);
        return 0;
    }
  }

  tinympg::TinyMPGEncoder<MPG_DEFAULT_ALLOCATOR> &driver() { return encoder_; }

 protected:
  static constexpr int kMaxGrowRetries = 4;

  tinympg::TinyMPGEncoder<MPG_DEFAULT_ALLOCATOR> encoder_;
  Print *p_out = nullptr;
  VideoOutput *p_out_video = nullptr;
  VideoFormat input_format = VideoFormat::I420;
  int width_ = 0;
  int height_ = 0;
  Vector<uint8_t> bitstream_buffer;

  template <typename Fn>
  size_t writeOut(Fn encodeInto) {
    if (p_out == nullptr && p_out_video == nullptr) return 0;
    size_t encoded = encodeInto(bitstream_buffer.data(), bitstream_buffer.size());
    for (int attempt = 0; encoded == 0 && attempt < kMaxGrowRetries; ++attempt) {
      bitstream_buffer.resize(bitstream_buffer.size() * 2);
      if (bitstream_buffer.data() == nullptr) {
        LOGE("MPGEncoder: bitstream buffer grow failed");
        return 0;
      }
      encoded = encodeInto(bitstream_buffer.data(), bitstream_buffer.size());
    }
    if (encoded == 0) {
      LOGE("MPGEncoder: encode failed (status=%d)", (int)encoder_.lastStatus());
      return 0;
    }
    if (p_out != nullptr) p_out->write(bitstream_buffer.data(), encoded);
    if (p_out_video != nullptr) p_out_video->write(bitstream_buffer.data(), encoded);
    return encoded;
  }

  bool ensureBitstreamBuffer() {
    if (width_ <= 0 || height_ <= 0) return false;
    size_t needed = (size_t)width_ * (size_t)height_ * 2 + 4096;
    if (bitstream_buffer.size() < needed) {
      bitstream_buffer.resize(needed);
      if (bitstream_buffer.data() == nullptr) {
        return false;
      }
    }
    return bitstream_buffer.data() != nullptr;
  }
};

}  // namespace audio_tools
