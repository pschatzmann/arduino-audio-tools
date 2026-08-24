#pragma once

#include "AudioTools/CoreAudio/AudioBasic/Collections/Vector.h"
#include "AudioTools/Video/CodecH264.h"
#include "AudioTools/Video/CodecJPEG.h"
#include "AudioTools/Video/CodecMPG.h"
#include "AudioTools/Video/CodecVideo.h"

namespace audio_tools {

/// True if `data` looks like the start of a JPEG image (SOI marker FF D8)
/// - unambiguous, and reliable even on the very first write() of a stream
/// since a JPEG image always begins at FF D8 regardless of framing.
/// @ingroup video
inline bool isMjpegVideo(const uint8_t *data, size_t len) {
  return len >= 2 && data[0] == 0xFF && data[1] == 0xD8;
}

/// True if `data` contains an MPEG-1 sequence_header (00 00 01 B3) -
/// required by spec to precede the first picture of any real MPEG-1
/// elementary stream, so it's reliably present in the very first access
/// unit a demuxer ever hands to write() (bundled with the first GOP/
/// picture - see ContainerMPG.h's own unit-buffering comment). 0xB3 can
/// never appear as a valid H.264 NAL header byte (forbidden_zero_bit
/// would have to be 1), so this never false-positives on H.264 content.
/// @ingroup video
inline bool isMpeg1Video(const uint8_t *data, size_t len) {
  for (size_t i = 0; i + 3 < len; i++) {
    if (data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 1 &&
        data[i + 3] == 0xB3) {
      return true;
    }
  }
  return false;
}

/// True if `data` starts with an Annex-B start code (00 00 01) followed
/// by a structurally valid NAL header (forbidden_zero_bit == 0, a
/// defined nal_unit_type). Checked after isMpeg1Video() (see
/// MultiVideoDecoder's constructor) since a handful of low NAL-type
/// values numerically overlap MPEG-1's slice_start_code range and are
/// only disambiguated by MPEG-1's own sequence_header having already
/// been ruled out first.
/// @ingroup video
inline bool isH264Video(const uint8_t *data, size_t len) {
  for (size_t i = 0; i + 3 < len; i++) {
    if (data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 1) {
      uint8_t header = data[i + 3];
      if ((header & 0x80) == 0) {  // forbidden_zero_bit must be 0
        uint8_t nal_type = header & 0x1F;
        if (nal_type >= 1 && nal_type <= 23) return true;
      }
      i += 2;
    }
  }
  return false;
}

/**
 * @brief Manages multiple VideoDecoders with automatic format detection -
 * the video-side counterpart of MultiDecoder (AudioCodecs/MultiDecoder.h).
 *
 * Comes pre-registered with every video codec this library ships a
 * portable (no hardware-specific backend) software decoder for:
 * MJPEGDecoder (Motion-JPEG, TinyJPEG, CodecJPEG.h), MPGDecoder
 * (MPEG-1, TinyMPG, CodecMPG.h), H264Decoder (H.264 Annex-B, TinyH264,
 * CodecH264.h) - drop it into a demuxer's setOutputVideo() the same way
 * any single decoder would go, and it self-selects the right one from
 * the bitstream's own framing instead of the caller having to know the
 * codec up front. Extend with addDecoder() for anything else (e.g.
 * H264DecoderESP32S3's hardware-accelerated backend - not pre-registered
 * here since it's ESP32-S3-only and needs its own library/header).
 *
 * Detection runs once, on the very first write() call, preferring the
 * container's own answer over guessing from raw bytes: if
 * setVideoInfoSource() was given a source (typically the demuxer feeding
 * this object - DemuxerAVI/DemuxerMP4/DemuxerMPG all implement
 * VideoInfoSource and already parse the real codec from their own
 * container metadata, e.g. DemuxerAVI::getVideoInfo().format from the
 * AVI 'strf' chunk's FOURCC - see ContainerAVI.h's own comment) and its
 * videoInfo().format matches a registered decoder, that decoder is
 * selected directly, no sniffing needed. Otherwise (no source set, or
 * its format is UNKNOWN/unregistered) falls back to each registered
 * decoder's check() function, tried in registration order (MJPEG, then
 * MPEG-1, then H.264 - see isMjpegVideo()/isMpeg1Video()/isH264Video()
 * above) against that first call's bytes. Reliable either way because
 * every current caller already hands over one complete access unit per
 * write() call. Every subsequent call, plus flush()/isKeyFrame()/
 * setSkipRender(), is forwarded to that same decoder for the rest of
 * the stream - never re-detected.
 *
 * setOutput()/setVideoFormat() are applied to all registered decoders
 * eagerly (cheap - just stores pointers/an enum, no allocation), but
 * begin() is only ever called on the one decoder actually selected -
 * matching MultiDecoder's own memory-efficient lazy-init rationale,
 * since a decoder's begin() is what allocates its picture buffers.
 *
 * Dependencies (install via Library Manager) - all three are required to
 * build this header, regardless of which formats your content actually
 * uses:
 * - https://github.com/pschatzmann/TinyH264
 * - https://github.com/pschatzmann/TinyMPG
 * - https://github.com/pschatzmann/TinyJPEG
 *
 * @ingroup video
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class MultiVideoDecoder : public VideoDecoder, public VideoInfoSource {
 public:
  MultiVideoDecoder() {
    addDecoder(mjpeg, VideoFormat::MJPEG, isMjpegVideo);
    addDecoder(mpeg1, VideoFormat::MPEG1, isMpeg1Video);
    addDecoder(h264, VideoFormat::H264, isH264Video);
  }

  /// Registers a decoder together with a detection function that
  /// inspects the first write() call's bytes and returns true if this
  /// decoder can handle that format - see the class comment for when/how
  /// this runs. Call before the first write() reaches this object.
  void addDecoder(VideoDecoder &decoder, VideoFormat format,
                  bool (*check)(const uint8_t *data, size_t len)) {
    DecoderInfo info;
    info.decoder = &decoder;
    info.format = format;
    info.check = check;
    decoders.push_back(info);
  }

  /// Provides the container's own answer for which codec the video track
  /// actually is - takes precedence over content-sniffing when set (see
  /// the class comment). Pass the demuxer feeding this object, e.g.
  /// multiVideoDecoder.setVideoInfoSource(aviDemuxer). Must outlive this
  /// object; call before the first write().
  void setVideoInfoSource(VideoInfoSource &source) {
    p_video_info_source = &source;
  }

  void setOutput(Print &out) override {
    for (int i = 0; i < decoders.size(); i++) decoders[i].decoder->setOutput(out);
  }

  /// See VideoDecoder::setOutput(VideoOutput&) - forwarded to every
  /// registered decoder eagerly (same rationale as the Print& overload
  /// above), so whichever one gets selected on the first write() is
  /// already wired.
  void setOutput(VideoOutput &out) override {
    mjpeg.setOutput(out);
    mpeg1.setOutput(out);
    h264.setOutput(out);
  }

  void setVideoFormat(VideoFormat format) override {
    for (int i = 0; i < decoders.size(); i++)
      decoders[i].decoder->setVideoFormat(format);
  }

  VideoInfo videoInfo() override {
    return p_selected != nullptr ? p_selected->videoInfo() : VideoInfo{};
  }

  /// Defers actual decoder init to the first write() (see class comment)
  /// - always succeeds itself.
  bool begin() override {
    p_selected = nullptr;
    p_selected_format = VideoFormat::UNKNOWN;
    is_first = true;
    return true;
  }

  void end() override {
    if (p_selected != nullptr) p_selected->end();
    p_selected = nullptr;
    p_selected_format = VideoFormat::UNKNOWN;
    is_first = true;
  }

  size_t write(const uint8_t *data, size_t len) override {
    if (len == 0) return 0;
    if (is_first) {
      is_first = false;
      if (p_video_info_source != nullptr) {
        VideoFormat source_format = p_video_info_source->videoInfo().format;
        for (int i = 0; i < decoders.size(); i++) {
          if (decoders[i].format == source_format) {
            LOGI("MultiVideoDecoder: format %d from VideoInfoSource",
                 (int)source_format);
            select(decoders[i]);
            break;
          }
        }
      }
      if (p_selected == nullptr) {
        for (int i = 0; i < decoders.size(); i++) {
          if (decoders[i].check(data, len)) {
            select(decoders[i]);
            break;
          }
        }
      }
      if (p_selected == nullptr) {
        LOGE(
            "MultiVideoDecoder: could not determine video format from "
            "first %u bytes",
            (unsigned)len);
      }
    }
    return p_selected != nullptr ? p_selected->write(data, len) : len;
  }

  void flush() override {
    if (p_selected != nullptr) p_selected->flush();
  }

  void setSkipRender(bool skip) override {
    if (p_selected != nullptr) p_selected->setSkipRender(skip);
  }

  bool isKeyFrame(const uint8_t *data, size_t len) override {
    return p_selected != nullptr ? p_selected->isKeyFrame(data, len) : false;
  }

  /// The codec detection picked for the current stream -
  /// VideoFormat::UNKNOWN before the first write(), or if none matched.
  VideoFormat selectedFormat() const { return p_selected_format; }

 protected:
  struct DecoderInfo {
    VideoDecoder *decoder = nullptr;
    VideoFormat format = VideoFormat::UNKNOWN;
    bool (*check)(const uint8_t *data, size_t len) = nullptr;
  };

  MJPEGDecoder mjpeg;
  MPGDecoder mpeg1;
  H264Decoder h264;
  Vector<DecoderInfo> decoders{0};

  VideoDecoder *p_selected = nullptr;
  VideoFormat p_selected_format = VideoFormat::UNKNOWN;
  VideoInfoSource *p_video_info_source = nullptr;
  bool is_first = true;

  void select(DecoderInfo &info) {
    p_selected = info.decoder;
    p_selected_format = info.format;
    p_selected->begin();
    LOGI("MultiVideoDecoder: selected decoder for format %d", (int)info.format);
  }
};

}  // namespace audio_tools
