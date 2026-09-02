#pragma once
#include "AudioTools/CoreAudio/AudioOutput.h"
#include "AudioTools/CoreAudio/Buffers.h"
#include "AudioTools/Video/CodecCopy.h"
#include "AudioTools/Video/VideoOutput.h"
#include "AudioTools/Video/PacedVideoOutput.h"
#include "stdint.h"

/**
 * @defgroup video Video
 * @ingroup main
 * @brief Video playback
 */

namespace audio_tools {

/// @brief Which track write() feeds, for muxers (MuxerAVI, MuxerMP4) that
/// double as a plain, generic Print-like sink for both tracks - see
/// setStreamType()/streamType() on those classes.
/// @ingroup video
enum StreamContentType { Audio, Video };

/// @brief Fixed per-frame size (bytes) for a raw/uncompressed VideoFormat
/// at the given resolution - 0 for compressed formats (H264/MJPEG/MPEG4) or
/// VideoFormat::UNKNOWN, since their frame size varies per frame.
/// @ingroup video
inline size_t videoFrameSizeBytes(VideoFormat format, uint16_t width,
                                  uint16_t height) {
  size_t px = (size_t)width * (size_t)height;
  switch (format) {
    case VideoFormat::RAW:
    case VideoFormat::RGB666:
    case VideoFormat::RGB888:
      return px * 3;
    case VideoFormat::YUV422:
    case VideoFormat::RGB565:
      return px * 2;
    case VideoFormat::I420:
      return px + px / 2;
    default:
      return 0;
  }
}

/// @brief True if the given Annex-B H.264 access unit contains an IDR
/// slice NAL unit (nal_unit_type 5) - the reliable way to tell a real
/// keyframe/sync-sample apart from a P-frame, since nothing in the byte
/// layout itself says so without inspecting NAL headers. Used e.g. to
/// determine Muxer::addVideoFrame()'s isKeyFrame argument for an
/// already-encoded VideoFormat::H264 frame.
/// @ingroup video
inline bool isH264KeyFrame(const uint8_t* data, size_t len) {
  for (size_t i = 0; i + 3 < len; i++) {
    if (data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 1) {
      if ((data[i + 3] & 0x1F) == 5) return true;  // IDR slice
      i += 2;
    }
  }
  return false;
}

/// @brief True if the given MPEG-1/2 video access unit's picture header
/// declares picture_coding_type == 1 (I-picture) - the MPEG equivalent of
/// isH264KeyFrame(). Layout (ISO/IEC 11172-2): a 00 00 01 00
/// picture_start_code is immediately followed by temporal_reference (10
/// bits) then picture_coding_type (3 bits), so the type field always
/// falls in bits 5-3 of the byte right after the 2-byte
/// temporal_reference span (i.e. 6 bytes into the access unit, counting
/// the start code).
/// @ingroup video
inline bool isMpeg1KeyFrame(const uint8_t* data, size_t len) {
  for (size_t i = 0; i + 5 < len; i++) {
    if (data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 1 &&
        data[i + 3] == 0x00) {
      return ((data[i + 5] >> 3) & 0x07) == 1;  // I-picture
    }
  }
  return false;
}

/**
 * @brief Pull-based provider of one already-encoded video/image frame at a
 * time - e.g. wraps a camera capture + H264Encoder/MJPEG capture pipeline.
 * Used by VideoMuxerWithTasks's video task, which calls nextFrame() once
 * per iteration, at the rate given by videoInfo().fps. Implementations
 * should produce/capture the frame here directly (not defer it): a slow
 * nextFrame() only delays whatever is pulling from it.
 * @ingroup video
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class VideoFrameSource {
 public:
  virtual ~VideoFrameSource() = default;
  /// Provides the next encoded frame: return a pointer to the frame's
  /// bytes (only required to stay valid until the next nextFrame() call)
  /// and set `len` to its size - or return nullptr if no frame is
  /// available yet, in which case the caller skips this iteration.
  virtual const uint8_t* nextFrame(size_t& len) = 0;
  /// Provides the width/height/fps/format (and, if known, frame_size/
  /// total_file_size) of the frames returned by nextFrame() - width,
  /// height, fps and format must be set, since VideoMuxerWithTasks uses
  /// them directly to configure the Muxer it writes to instead of
  /// requiring the caller to duplicate the same values there.
  virtual VideoInfo videoInfo() = 0;

  /// @brief Write the frame to the indicated destination
  bool writeTo(Print& out) {
    size_t len;
    const uint8_t* frame = nextFrame(len);
    if (frame == nullptr || len == 0) return false;
    size_t written = out.write(frame, len);
    return written = len;
  }
};

/**
 * @brief Transparent write() wrapper that measures throughput/turnaround
 * while forwarding every call unchanged to the target given in the
 * constructor - drop it into a pipeline slot (e.g. h264Decoder.setOutput(
 * meter) instead of h264Decoder.setOutput(tftOutput), with meter wrapping
 * tftOutput) to find out how that one stage is actually performing,
 * without instrumenting the stage itself.
 *
 * Derives from VideoOutput only (not also Print - see asPrint() for why),
 * so it can be passed directly wherever a VideoOutput& is expected; for a
 * plain Print& target/slot, use asPrint() instead of the object itself.
 *
 * Tracks, as running averages since construction:
 *  - count(): total write() calls forwarded.
 *  - fpsMax(): 1000 / the average time spent inside the forwarded write()
 *    call - this stage's own processing capacity (the rate it could
 *    sustain if called back-to-back with zero gap), independent of how
 *    often it's actually invoked.
 *  - fpsAvg(): 1000 / the average wall-clock gap between the start of
 *    consecutive write() calls - the actually observed call rate,
 *    including whatever idle/pacing time happens between calls elsewhere
 *    in the pipeline. Always <= fpsMax(), since each gap already contains
 *    that call's own forward time.
 * @ingroup video
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class VideoFrameMeter : public VideoOutput {
 public:
  /// @param target every write() call is forwarded here, unmodified.
  VideoFrameMeter(Print& target) : p_target(&target) {}

  /// @param target every write() call is forwarded here, unmodified -
  /// AND flush()/setSkipRender() are also forwarded, so a meter dropped in
  /// front of a VideoOutput (e.g. wrapping tftOutput in h264Decoder.
  /// setOutput(meter) instead of h264Decoder.setOutput(tftOutput)) stays
  /// transparent to a pipeline's render-skip hint instead of silently
  /// breaking it - the Print-only constructor above can't do that, since
  /// Print has no equivalent call to forward.
  VideoFrameMeter(VideoOutput& target) : p_target_video(&target) {}

  size_t write(const uint8_t* data, size_t len) override {
    uint32_t now = millis();
    if (has_last_call) {
      total_turnaround_ms += (now - last_call_ms);
    }
    last_call_ms = now;
    has_last_call = true;

    // VideoOutput and Print are unrelated types (no common base), so the
    // constructor that received a VideoOutput& only has somewhere to
    // store it as p_target_video, not p_target - check that one first.
    uint32_t forwardStart = millis();
    size_t result = 0;
    if (p_target_video != nullptr) {
      result = p_target_video->write(data, len);
    } else if (p_target != nullptr) {
      result = p_target->write(data, len);
    }
    total_forward_ms += millis() - forwardStart;

    count_++;
    return result;
  }

  void flush() override {
    if (p_target_video != nullptr) p_target_video->flush();
  }
  void setSkipRender(bool skip) override {
    if (p_target_video != nullptr) p_target_video->setSkipRender(skip);
  }

  /// Total number of write() calls forwarded so far.
  uint32_t count() const { return count_; }

  float fpsMax() const {
    return count_ > 0 && total_forward_ms > 0
               ? (1000.0f * count_) / total_forward_ms
               : 0.0f;
  }

  float fpsAvg() const {
    return count_ > 1 && total_turnaround_ms > 0
               ? (1000.0f * (count_ - 1)) / total_turnaround_ms
               : 0.0f;
  }

  /// Average time spent inside the forwarded write() call, in ms - the
  /// same figure fpsMax() is derived from (fpsMax() ==
  /// 1000/timeWriteMs()), for callers that want the raw duration instead
  /// of a rate.
  float timeWriteMs() const {
    return count_ > 0 ? (float)total_forward_ms / count_ : 0.0f;
  }

  /// Average wall-clock gap between the start of consecutive write()
  /// calls, in ms - the same figure fpsAvg() is derived from (fpsAvg() ==
  /// 1000/timeTurnaroundMs()), for callers that want the raw duration
  /// instead of a rate.
  float timeTurnaroundMs() const {
    return count_ > 1 ? (float)total_turnaround_ms / (count_ - 1) : 0.0f;
  }

  /// A Print-compatible view onto this meter - use this (e.g.
  /// someSink.setOutput(meter.asPrint())) at any call site that
  /// specifically needs a Print& rather than a VideoOutput&. Never pass
  /// the meter itself there: VideoFrameMeter derives only from
  /// VideoOutput precisely so it stays unambiguous wherever an API (this
  /// codebase has several - DemuxerAVI::setOutputVideo(),
  /// H264Decoder::setOutput(), ...) is overloaded on both Print& and
  /// VideoOutput& - a type implicitly convertible to both would make
  /// every such call ambiguous.
  Print& asPrint() { return print_view_; }

 protected:
  class PrintView : public Print {
   public:
    explicit PrintView(VideoFrameMeter& meter) : meter_(meter) {}
    size_t write(const uint8_t* data, size_t len) override {
      return meter_.write(data, len);
    }
    size_t write(uint8_t b) override { return meter_.write(&b, 1); }

   private:
    VideoFrameMeter& meter_;
  };

  Print* p_target = nullptr;
  VideoOutput* p_target_video = nullptr;
  uint32_t count_ = 0;
  uint32_t last_call_ms = 0;
  bool has_last_call = false;
  uint32_t total_forward_ms = 0;
  uint32_t total_turnaround_ms = 0;
  PrintView print_view_{*this};
};

}  // namespace audio_tools
