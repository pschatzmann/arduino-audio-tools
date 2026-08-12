#pragma once

#include "AudioTools/CoreAudio/AudioBasic/Collections/Vector.h"
#include "AudioTools/Video/CodecVideo.h"
#include "TinyH264Decoder.h"
#include "TinyH264Encoder.h"

/**
 * @defgroup h264 H264
 * @ingroup codecs
 * @ingroup video
 * @brief H.264 encoding/decoding using https://github.com/pschatzmann/TinyH264
 */

/// Allocator H264Decoder uses for its decoded picture buffers - PSRAM
/// (falling back to the regular heap if absent/exhausted) on ESP32, the
/// regular heap elsewhere. Define this yourself before including this
/// header to override (e.g. to force the regular heap on an ESP32 board
/// without PSRAM).
#ifndef H264_DEFAULT_ALLOCATOR
#ifdef ESP32
#define H264_DEFAULT_ALLOCATOR tinyh264::PSRAMAllocatorESP32<uint8_t>
#else
#define H264_DEFAULT_ALLOCATOR tinyh264::StdAllocator<uint8_t>
#endif
#endif

namespace audio_tools {

/**
 * @brief H.264 video decoder: wraps TinyH264Decoder
 * (https://github.com/pschatzmann/TinyH264) as a VideoOutput, so it can be
 * plugged directly into e.g. DemuxerAVI::setOutputVideo()/
 * DemuxerMP4::setOutputVideo(). Consumes an Annex-B H.264 bitstream via
 * write() and, once a complete picture has been decoded, writes it -
 * converted to setVideoFormat()'s format (RGB565 by default, the common
 * TFT wire format) - to the Print target configured via setOutput(), one
 * write() call per decoded picture.
 *
 * write()/flush() implement the VideoOutput contract (see Video.h): write()
 * may be called one or more times per frame - bytes are forwarded to the
 * decoder immediately (Annex-B start codes are self-delimiting, so no
 * internal buffering is needed here; a picture may complete partway through
 * any write() call, not just at flush()). flush() is a no-op, kept only
 * because some producers (e.g. DemuxerAVI/DemuxerMP4) call it
 * unconditionally after each frame.
 *
 * Uses H264_DEFAULT_ALLOCATOR for the decoded picture buffers -
 * see that macro's own comment above for the PSRAM-on-ESP32/heap-
 * elsewhere default it picks, and how to override it.
 *
 * @ingroup h264
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class H264Decoder : public VideoDecoder {
 public:
  H264Decoder() { decoder_.setCallback(onFrame, this); }

  /// Defines the target the decoded picture is written to, one write()
  /// call per decoded picture, in the format selected via
  /// setVideoFormat() (RGB565 by default).
  void setOutput(Print &out) override { p_out = &out; }

  /// Selects the pixel format written to setOutput()'s target - RGB565
  /// (the default) is what most TFT display libraries expect; RGB666/
  /// RGB888 for higher color depth displays; I420 to pass the decoded
  /// planes through unconverted (tightly packed: Y bytes, then U bytes,
  /// then V bytes, no row padding - see TinyH264Decoder::toYUV420()).
  /// Any other VideoFormat is logged and ignored (previous format kept).
  void setVideoFormat(VideoFormat format) override {
    switch (format) {
      case VideoFormat::RGB565:
      case VideoFormat::RGB666:
      case VideoFormat::RGB888:
      case VideoFormat::I420:
        pixel_format = format;
        break;
      default:
        LOGW("H264Decoder: unsupported VideoFormat %d", (int)format);
        break;
    }
  }

  /// Reports the format/dimensions of the picture written to
  /// setOutput()'s target - format is setVideoFormat()'s most recently
  /// selected value (RGB565 if never called); width/height are the most
  /// recently decoded picture's (0 before any picture has been decoded).
  VideoInfo videoInfo() override {
    VideoInfo info;
    info.format = pixel_format;
    info.width = (uint16_t)decoder_.width();
    info.height = (uint16_t)decoder_.height();
    return info;
  }

  /// Runtime-active maximum stored reference pictures - see
  /// TinyH264Decoder::setMaxRefFrames().
  void setMaxRefFrames(int n) { decoder_.setMaxRefFrames(n); }

  /// Reserves the decoder's picture buffers up front - see
  /// TinyH264Decoder::begin().
  bool begin() override {
    decoder_.begin();
    return true;
  }

  /// Releases the decoder's picture buffers - see TinyH264Decoder::end().
  void end() override { decoder_.end(); }

  /// Feeds one chunk of Annex-B H.264 data - may be called more than once
  /// per frame (e.g. from a demuxer that hands over payload in pieces).
  /// Decodes immediately, invoking setOutput()'s Print once per completed
  /// picture from within this call (not deferred to flush()).
  size_t write(const uint8_t *data, size_t len) override {
    decoder_.write(data, len);
    return len;
  }

  /// No-op - kept to satisfy the VideoOutput/Print contract; decoding
  /// already happens synchronously in write() (see class comment).
  void flush() override {}

  /// True if the most recent write() call ended in a bitstream error or
  /// hit an unsupported stream feature.
  bool hasError() { return decoder_.hasError(); }

  /// Direct access to the wrapped TinyH264Decoder, e.g. for its width()/
  /// height()/y()/u()/v()/getY()/getU()/getV() accessors.
  tinyh264::TinyH264Decoder<H264_DEFAULT_ALLOCATOR> &driver() { return decoder_; }

 protected:
  tinyh264::TinyH264Decoder<H264_DEFAULT_ALLOCATOR> decoder_;
  Print *p_out = nullptr;
  VideoFormat pixel_format = VideoFormat::RGB565;
  Vector<uint8_t> frame_buffer;

  static void onFrame(tinyh264::TinyH264Decoder<H264_DEFAULT_ALLOCATOR> &decoder,
                       void *userData) {
    static_cast<H264Decoder *>(userData)->writeFrame();
  }

  void writeFrame() {
    if (p_out == nullptr) return;
    size_t w = decoder_.width();
    size_t h = decoder_.height();
    size_t n = 0;
    switch (pixel_format) {
      case VideoFormat::RGB565: {
        size_t needed = w * h;
        if (frame_buffer.size() < needed * 2) {
          frame_buffer.resize(needed * 2);
          if (frame_buffer.data() == nullptr) {
            LOGE("H264Decoder: frame buffer allocation failed (RGB565)");
            return;
          }
        }
        n = decoder_.toRGB565((uint16_t *)frame_buffer.data(), needed);
        if (n > 0) p_out->write(frame_buffer.data(), n * 2);
        break;
      }
      case VideoFormat::RGB666: {
        size_t needed = w * h * 3;
        if (frame_buffer.size() < needed) {
          frame_buffer.resize(needed);
          if (frame_buffer.data() == nullptr) {
            LOGE("H264Decoder: frame buffer allocation failed (RGB666)");
            return;
          }
        }
        n = decoder_.toRGB666(frame_buffer.data(), needed);
        if (n > 0) p_out->write(frame_buffer.data(), n);
        break;
      }
      case VideoFormat::RGB888: {
        size_t needed = w * h * 3;
        if (frame_buffer.size() < needed) {
          frame_buffer.resize(needed);
          if (frame_buffer.data() == nullptr) {
            LOGE("H264Decoder: frame buffer allocation failed (RGB888)");
            return;
          }
        }
        n = decoder_.toRGB888(frame_buffer.data(), needed);
        if (n > 0) p_out->write(frame_buffer.data(), n);
        break;
      }
      case VideoFormat::I420: {
        size_t needed = w * h + 2 * (w / 2) * (h / 2);
        if (frame_buffer.size() < needed) {
          frame_buffer.resize(needed);
          if (frame_buffer.data() == nullptr) {
            LOGE("H264Decoder: frame buffer allocation failed (I420)");
            return;
          }
        }
        n = decoder_.toYUV420(frame_buffer.data(), needed);
        if (n > 0) p_out->write(frame_buffer.data(), n);
        break;
      }
      default:
        break;
    }
  }
};

/**
 * @brief H.264 video encoder: wraps TinyH264Encoder
 * (https://github.com/pschatzmann/TinyH264). Configure via setSize()/
 * setQp() (or setTargetBitrate() for rate control) same as TinyH264Encoder
 * and setVideoFormat() for the raw picture format write() expects (I420 -
 * planar Y/U/V concatenated in one buffer - by default), then feed raw
 * pictures via write() - the resulting Annex-B bitstream is written to the
 * Print target configured via setOutput() (e.g. a Muxer's addVideoFrame()
 * by way of an adapter, or any other Print) instead of a caller-supplied
 * buffer.
 *
 * The internal bitstream scratch buffer starts at
 * kDefaultBitstreamBufferSize and grows (doubling, up to a few attempts)
 * whenever an encode call comes back short - the same "call again with a
 * bigger buffer" recovery TinyH264Encoder's own encodeFrame() family
 * documents, done automatically so callers don't need to size it by hand.
 * Use setBitstreamBufferSize() to pre-size it (e.g. to avoid the first
 * frame paying for a grow-and-retry).
 *
 *
 * @ingroup h264
 * @ingroup encoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class H264Encoder : public VideoEncoder {
 public:
  /// Default size (bytes) of the internal bitstream scratch buffer -
  /// see setBitstreamBufferSize().
  static constexpr size_t kDefaultBitstreamBufferSize = 16384;

  H264Encoder(int width = 0, int height = 0, int keyframeInterval = 0)
      : encoder_(width, height, keyframeInterval) {
    width_ = width;
    height_ = height;
  }

  /// Defines the target the encoded bitstream is written to, one write()
  /// call per write() call that actually produced output.
  void setOutput(Print &out) override { p_out = &out; }

  /// Selects the raw picture format write() expects - VideoFormat::I420
  /// (the default: planar Y/U/V, concatenated Y then U then V in one
  /// buffer, split via setSize()'s width/height), YUV422 (packed YUYV),
  /// RGB565/RGB666/RGB888 (packed) - see TinyH264Encoder's own
  /// encodeFrame()-family doc comments (h264_encoder.h) for exactly what
  /// each byte layout is. Any other VideoFormat is logged and ignored
  /// (previous format kept).
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
        LOGW("H264Encoder: unsupported VideoFormat %d", (int)format);
        break;
    }
  }

  /// Reports the codec/dimensions of the bitstream written to
  /// setOutput()'s target - format is always VideoFormat::H264 (NOT
  /// setVideoFormat()'s raw input format); width/height match setSize()
  /// (0 if that was never called).
  VideoInfo videoInfo() override {
    VideoInfo info;
    info.format = VideoFormat::H264;
    info.width = (uint16_t)width_;
    info.height = (uint16_t)height_;
    return info;
  }

  /// See TinyH264Encoder::setSize()
  void setSize(int width, int height) {
    width_ = width;
    height_ = height;
    encoder_.setSize(width, height);
  }
  /// See TinyH264Encoder::setQp()
  void setQp(int qp) { encoder_.setQp(qp); }
  /// See TinyH264Encoder::setTargetBitrate()
  void setTargetBitrate(int bitsPerSecond, double fps) {
    encoder_.setTargetBitrate(bitsPerSecond, fps);
  }
  /// See TinyH264Encoder::setKeyframeInterval()
  void setKeyframeInterval(int frames) {
    encoder_.setKeyframeInterval(frames);
  }
  /// See TinyH264Encoder::setStride()
  void setStride(int strideY, int strideC) {
    encoder_.setStride(strideY, strideC);
  }
  /// See TinyH264Encoder::setPackedStride()
  void setPackedStride(int stride) { encoder_.setPackedStride(stride); }

  /// Pre-sizes the internal bitstream scratch buffer encodeFrame() and its
  /// color-format siblings encode into before writing the result to
  /// setOutput() - call before the first encode call to avoid paying for
  /// the automatic grow-and-retry on it (see class comment). Shrinks are
  /// ignored (the buffer never releases memory on its own).
  void setBitstreamBufferSize(size_t size) {
    if (size > bitstream.size()) {
      bitstream.resize(size);
      if (bitstream.data() == nullptr) {
        LOGE("H264Encoder: bitstream buffer allocation failed");
      }
    }
  }

  /// See TinyH264Encoder::begin() - equivalent to
  /// begin(reserveColorConversionScratch=false); see that overload if you
  /// need to pass true.
  bool begin() override { return begin(false); }
  /// See TinyH264Encoder::begin()
  bool begin(bool reserveColorConversionScratch) {
    encoder_.begin(reserveColorConversionScratch);
    return true;
  }
  /// See TinyH264Encoder::end()
  void end() override { encoder_.end(); }

  /// The QP actually used by the most recent write() call.
  int lastQp() const { return encoder_.lastQp(); }

  /// Encodes one raw picture, in setVideoFormat()'s format (I420 by
  /// default), sized for setSize()'s width/height, and writes the
  /// resulting Annex-B bitstream to setOutput() - returns the number of
  /// bytes written (0 if nothing was, e.g. setSize()/setOutput() was
  /// never called, or `len` doesn't match the configured format/size).
  /// `len` is only used to validate the I420 case (the other formats are
  /// packed buffers TinyH264Encoder itself size-checks internally); see
  /// TinyH264Encoder's own encodeFrame()-family doc comments
  /// (h264_encoder.h) for exactly what each byte layout/size is.
  size_t write(const uint8_t *data, size_t len) override {
    switch (input_format) {
      case VideoFormat::I420: {
        size_t ySize = (size_t)width_ * height_;
        size_t cSize = (size_t)(width_ / 2) * (height_ / 2);
        if (len < ySize + 2 * cSize) return 0;
        const uint8_t *srcY = data;
        const uint8_t *srcU = data + ySize;
        const uint8_t *srcV = data + ySize + cSize;
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
        return 0;
    }
  }

  /// Direct access to the wrapped TinyH264Encoder, e.g. for its
  /// width()/height()/y()/u()/v() reconstructed-picture accessors.
  tinyh264::TinyH264Encoder<H264_DEFAULT_ALLOCATOR> &driver() { return encoder_; }

 protected:
  /// Number of times to double the bitstream buffer and retry an
  /// undersized encode call before giving up (see class comment).
  static constexpr int kMaxGrowRetries = 4;

  tinyh264::TinyH264Encoder<H264_DEFAULT_ALLOCATOR> encoder_;
  Print *p_out = nullptr;
  Vector<uint8_t> bitstream;
  VideoFormat input_format = VideoFormat::I420;
  int width_ = 0;
  int height_ = 0;

  template <typename Fn>
  size_t writeOut(Fn encodeInto) {
    if (p_out == nullptr) return 0;
    if (bitstream.size() == 0) {
      bitstream.resize(kDefaultBitstreamBufferSize);
      if (bitstream.data() == nullptr) {
        LOGE("H264Encoder: bitstream buffer allocation failed");
        return 0;
      }
    }
    size_t n = encodeInto(bitstream.data(), bitstream.size());
    for (int attempt = 0; n == 0 && attempt < kMaxGrowRetries; ++attempt) {
      bitstream.resize(bitstream.size() * 2);
      if (bitstream.data() == nullptr) {
        LOGE("H264Encoder: bitstream buffer grow failed");
        return 0;
      }
      n = encodeInto(bitstream.data(), bitstream.size());
    }
    if (n > 0) p_out->write(bitstream.data(), n);
    return n;
  }
};

}  // namespace audio_tools
