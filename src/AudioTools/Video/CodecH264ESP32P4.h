#pragma once

#include "AudioTools/Video/CodecVideo.h"
#include "H264EncoderP4.h"

/**
 * @defgroup h264esp32p4 H264ESP32P4
 * @ingroup codecs
 * @ingroup video
 * @brief H.264 hardware encoding on ESP32-P4 using
 * https://github.com/pschatzmann/codec-h264-ESP32P4 (esp_h264's dedicated,
 * register/DMA-driven H.264 hardware encoder block). ESP32-P4 only -
 * H264ConfigP4.h in that library fails the build on any other target.
 *
 * Encode only: ESP32-P4 has no H.264 hardware decoder, and
 * codec-h264-ESP32P4 does not wrap a software one either. Use H264Decoder
 * (CodecH264.h, the portable TinyH264-based decoder) or
 * H264DecoderESP32S3 (on an S3) for decode.
 */

namespace audio_tools {

/**
 * @brief H.264 video encoder for ESP32-P4: wraps
 * esp_h264_p4::H264EncoderP4 (https://github.com/pschatzmann/codec-h264-ESP32P4),
 * which drives ESP32-P4's dedicated H.264 hardware encoder block - the
 * ESP32-P4-specific counterpart of H264Encoder (CodecH264.h, the portable
 * TinyH264-based software encoder) and a hardware-backed sibling of
 * H264EncoderESP32S3 (whose wrapped esp_h264::H264Encoder is software-only,
 * since the S3 has no H.264 hardware block).
 *
 * Configure via setSize()/setFrameRate()/setBitrate()/setGop()/
 * setQpRange()/setVideoFormat() (I420 - the default -, RGB565 or YUV422),
 * then feed raw pictures via write() - it writes the resulting Annex-B
 * bitstream straight to the Print target passed to setOutput().
 *
 * Unlike RGB565/YUV422 (packed formats the hardware encoder consumes
 * as-is), I420 (planar Y/U/V) is not one of the hardware's accepted raw
 * layouts - write() instead routes it through
 * esp_h264_p4::H264EncoderP4::encodeYUV420Planar(), which repacks it into
 * the hardware's required ESP_H264_RAW_FMT_O_UYY_E_VYY layout internally.
 *
 * The Alloc template parameter is forwarded to esp_h264_p4::H264EncoderP4
 * - see that class's own file comment (H264EncoderP4.h) for the PSRAM- vs.
 * internal-RAM allocator choice (H264EncoderP4PSRAM/H264EncoderP4RAM
 * aliases). Defaults to H264P4_DEFAULT_ALLOCATOR (PSRAM).
 *
 * @ingroup h264esp32p4
 * @ingroup encoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
template <typename Alloc = H264P4_DEFAULT_ALLOCATOR>
class H264EncoderESP32P4 : public VideoEncoder {
 protected:
  /// esp_h264_p4::H264EncoderP4::encode()/encodeYUV420Planar() write
  /// straight to a Print& internally (there's no "return the buffer"
  /// variant) - this fans that single Print target out to whichever of
  /// setOutput(Print&)/setOutput(VideoOutput&) are configured, mirroring
  /// H264EncoderESP32S3's (CodecH264ESP32S3.h) dual p_out/p_out_video
  /// write.
  class DualOutputPrint : public Print {
   public:
    void setTargets(Print *out, VideoOutput *outVideo) {
      p_out = out;
      p_out_video = outVideo;
    }
    size_t write(uint8_t c) override { return write(&c, 1); }
    size_t write(const uint8_t *data, size_t len) override {
      size_t result = 0;
      if (p_out != nullptr) result = p_out->write(data, len);
      if (p_out_video != nullptr) result = p_out_video->write(data, len);
      return result;
    }

   protected:
    Print *p_out = nullptr;
    VideoOutput *p_out_video = nullptr;
  };

 public:
  H264EncoderESP32P4() { config_ = encoder_.defaultConfig(); }

  H264EncoderESP32P4(Print &out) : H264EncoderESP32P4() { setOutput(out); }

  H264EncoderESP32P4(VideoOutput &out) : H264EncoderESP32P4() {
    setOutput(out);
  }

  /// Defines the target each write() call writes its encoded bitstream
  /// to.
  void setOutput(Print &out) override {
    p_out = &out;
    dual_out_.setTargets(p_out, p_out_video);
  }
  void setOutput(VideoOutput &out) {
    p_out_video = &out;
    dual_out_.setTargets(p_out, p_out_video);
  }

  /// Selects the raw picture format write() expects - this backend
  /// (esp_h264_p4) supports VideoFormat::I420 (the default: planar Y/U/V,
  /// no row padding - repacked internally, see class comment), RGB565
  /// (16-bit, 5-6-5 packed, little-endian) and YUV422 (packed YUYV); any
  /// other value is logged and ignored (the previously selected format
  /// stays in effect). Call before begin() - it also selects the
  /// hardware's raw pixel format (Config::pic_type).
  void setVideoFormat(VideoFormat format) override {
    switch (format) {
      case VideoFormat::I420:
        config_.pic_type = ESP_H264_RAW_FMT_O_UYY_E_VYY;
        input_format = format;
        break;
      case VideoFormat::RGB565:
        config_.pic_type = ESP_H264_RAW_FMT_RGB565_LE;
        input_format = format;
        break;
      case VideoFormat::YUV422:
        config_.pic_type = ESP_H264_RAW_FMT_YUYV;
        input_format = format;
        break;
      default:
        LOGW("H264EncoderESP32P4: unsupported VideoFormat %d", (int)format);
        break;
    }
  }

  /// Reports the codec/dimensions of the bitstream written to
  /// setOutput()'s target - format is always VideoFormat::H264 (NOT
  /// setVideoFormat()'s raw input format); width/height match setSize()
  /// (640x480 if never called - see Config's defaults).
  VideoInfo videoInfo() override {
    VideoInfo info;
    info.format = VideoFormat::H264;
    info.width = (uint16_t)config_.width;
    info.height = (uint16_t)config_.height;
    return info;
  }

  /// Picture size - must match the raw frame data passed to write().
  /// Hardware limits apply (max 1920x2032, min 80x80 - see
  /// esp_h264_types.h). Call before begin().
  void setSize(int width, int height) {
    config_.width = width;
    config_.height = height;
  }
  /// Target frames/sec - also the default GOP size (see setGop()) unless
  /// overridden. Call before begin().
  void setFrameRate(int fps) { config_.fps = fps; }
  /// Target bitrate (bits/sec) - defaults to width*height*fps*30/100 if
  /// never called. Call before begin().
  void setBitrate(int bitsPerSecond) { config_.bitrate = bitsPerSecond; }
  /// Group-of-Pictures size (keyframe interval, in frames) - unlike the
  /// software encoder (H264EncoderESP32S3), 0 is invalid here; defaults
  /// to setFrameRate()'s fps if never called. Call before begin().
  void setGop(int gop) { config_.gop = gop; }
  /// Quantizer parameter range [0, 51] the encoder's rate control stays
  /// within - lower is better quality/higher bitrate. Defaults to
  /// [25, 30] if never called. Call before begin().
  void setQpRange(int qpMin, int qpMax) {
    config_.qp_min = qpMin;
    config_.qp_max = qpMax;
  }
  /// Output buffer size (bytes) the wrapped encoder allocates for the
  /// encoded bitstream - defaults to width*height*bytes-per-pixel (for
  /// the selected raw format) if never called. Call before begin().
  void setOutputBufferSize(size_t size) { config_.outBufferSize = size; }

  /// Initializes the encoder - see esp_h264_p4::H264EncoderP4::begin().
  bool begin() override { return encoder_.begin(config_); }

  /// Releases the encoder's resources - see
  /// esp_h264_p4::H264EncoderP4::end().
  void end() override { encoder_.end(); }

  /// Encodes one raw picture, in setVideoFormat()'s format (I420 by
  /// default: Y, then U, then V, no row padding; RGB565 or YUV422
  /// likewise, passed straight through to the hardware), and writes the
  /// result to setOutput() - `len` must be at least width*height*3/2
  /// (I420) or width*height*2 (RGB565/YUV422). Returns 1 on success (see
  /// class comment - not an exact byte count, the underlying
  /// esp_h264_p4::H264EncoderP4 only reports success/failure), or 0 on
  /// failure (e.g. setOutput() wasn't configured, `len` too small, or the
  /// format is unsupported).
  size_t write(const uint8_t *data, size_t len) override {
    if (p_out == nullptr && p_out_video == nullptr) return 0;
    bool ok = false;
    switch (input_format) {
      case VideoFormat::I420: {
        size_t ySize = (size_t)config_.width * config_.height;
        size_t cSize = ySize / 4;
        if (len < ySize + 2 * cSize) break;
        const uint8_t *srcY = data;
        const uint8_t *srcU = data + ySize;
        const uint8_t *srcV = data + ySize + cSize;
        ok = encoder_.encodeYUV420Planar(srcY, config_.width, srcU, srcV,
                                          config_.width / 2, dual_out_);
        break;
      }
      case VideoFormat::RGB565:
      case VideoFormat::YUV422:
        ok = encoder_.encode(data, len, dual_out_);
        break;
      default:
        break;
    }
    return ok ? 1 : 0;
  }

  /// Frame type of the most recent successful encode (IDR/I/P) - see
  /// esp_h264_p4::H264EncoderP4::lastFrameType().
  esp_h264_frame_type_t lastFrameType() const {
    return encoder_.lastFrameType();
  }

  /// Direct access to the wrapped esp_h264_p4::H264EncoderP4.
  esp_h264_p4::H264EncoderP4<Alloc> &driver() { return encoder_; }

 protected:
  esp_h264_p4::H264EncoderP4<Alloc> encoder_;
  typename esp_h264_p4::H264EncoderP4<Alloc>::Config config_;
  Print *p_out = nullptr;
  VideoOutput *p_out_video = nullptr;
  DualOutputPrint dual_out_;
  VideoFormat input_format = VideoFormat::I420;
};

}  // namespace audio_tools
