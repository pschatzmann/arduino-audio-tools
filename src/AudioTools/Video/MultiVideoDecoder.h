#pragma once

#include "AudioTools/CoreAudio/AudioBasic/Collections/Vector.h"
#include "AudioTools/Video/CodecVideo.h"

namespace audio_tools {

/**
 * @brief Manages multiple VideoDecoders with automatic format detection -
 * the video-side counterpart of MultiDecoder (AudioCodecs/MultiDecoder.h).
 * No decoders are registered by default and this header has no codec-
 * library dependency of its own - register whatever your content needs
 * via addDecoder(), or use MultiVideoDecoderFull (MultiVideoDecoderFull.h)
 * for one pre-registered with every video codec this library ships a
 * portable (no hardware-specific backend) software decoder for
 * (H264/MJPEG/MPEG-1).
 *
 * Drop it into a demuxer's setOutputVideo() the same way any single
 * decoder would go, and it self-selects the right registered decoder from
 * the bitstream's own framing instead of the caller having to know the
 * codec up front.
 *
 * Content-sniffing detection (the fallback path - see below) is done via
 * each registered decoder's own VideoDecoder::isValid() - a virtual
 * method with a default-false implementation, so a decoder only takes
 * part in auto-detection if it actually overrides it (the built-in
 * H264Decoder/MJPEGDecoder/MPGDecoder each do - see their own class
 * comments); a decoder that doesn't (e.g. H264DecoderESP32S3, not
 * auto-detected by default - see MultiVideoDecoderFull's own comment)
 * can still be registered and selected via a VideoInfoSource answer.
 *
 * Detection runs once, on the very first write() call, preferring the
 * container's own answer over guessing from raw bytes: if
 * setVideoInfoSource() was given a source (typically the demuxer feeding
 * this object - DemuxerAVI/DemuxerMP4/DemuxerMPG all implement
 * VideoInfoSource and already parse the real codec from their own
 * container metadata, e.g. DemuxerAVI::getVideoInfo().format from the
 * AVI 'strf' chunk's FOURCC - see ContainerAVI.h's own comment) and its
 * videoInfo().format matches a registered decoder, that decoder is
 * selected directly, no sniffing needed. Otherwise (no source set, or its
 * format is UNKNOWN/unregistered) falls back to each registered decoder's
 * isValid(), tried in registration order, against that first call's
 * bytes. Reliable either way because every current caller already hands
 * over one complete access unit per write() call. Every subsequent call,
 * plus flush()/isKeyFrame()/setSkipRender(), is forwarded to that same
 * decoder for the rest of the stream - never re-detected.
 *
 * setOutput()/setVideoFormat() are applied to every registered decoder
 * eagerly (cheap - just stores pointers/an enum, no allocation), but
 * begin() is only ever called on the one decoder actually selected -
 * matching MultiDecoder's own memory-efficient lazy-init rationale, since
 * a decoder's begin() is what allocates its picture buffers.
 *
 * @ingroup video
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class MultiVideoDecoder : public VideoDecoder, public VideoInfoSource {
 public:
  /// Registers a decoder for `format` - see the class comment for when/
  /// how it's later selected (VideoInfoSource match, or its own
  /// VideoDecoder::isValid() as a content-sniffing fallback). Call before
  /// the first write() reaches this object.
  ///
  /// Replaces, rather than adds to, any decoder already registered for
  /// `format` - at most one entry per format, so e.g. registering
  /// H264DecoderESP32S3 for VideoFormat::H264 actually overrides a
  /// previously-registered H264Decoder instead of silently losing to it:
  /// both write()'s VideoInfoSource-format lookup and its isValid()
  /// fallback loop match the first entry found for a format, so a stale
  /// second entry would otherwise be permanently unreachable dead weight,
  /// never actually selected.
  void addDecoder(VideoDecoder &decoder, VideoFormat format) {
    for (int i = 0; i < decoders.size(); i++) {
      if (decoders[i].format == format) {
        decoders.erase(i);
        break;
      }
    }
    DecoderInfo info;
    info.decoder = &decoder;
    info.format = format;
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
    for (int i = 0; i < decoders.size(); i++) decoders[i].decoder->setOutput(out);
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
          if (decoders[i].decoder->isValid(data, len)) {
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
  };

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
