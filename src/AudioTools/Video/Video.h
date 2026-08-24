#pragma once
#include "AudioTools/CoreAudio/AudioOutput.h"
#include "AudioTools/CoreAudio/Buffers.h"
#include "AudioTools/Video/CodecCopy.h"
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

/// @brief Video codec/pixel-format identifier, shared by two unrelated
/// uses: the (single) video stream of a container (DemuxerAVI/MuxerAVI and
/// DemuxerMP4 - much like AudioFormat is shared across the audio demuxers,
/// despite its WAV-flavored name), and the decoded-picture pixel format
/// VideoDecoder::setVideoFormat() selects. H264 is the primary target for
/// muxing; the others are AVI-specific conveniences or decoder-only.
/// - H264/MPEG4: compressed, variable frame size -> use addVideoFrame()
/// - MJPEG: one complete JPEG image per frame -> use addJpegFrame()
/// - RAW: uncompressed 24-bit BGR -> use addVideoFrame()
/// - YUV422: packed 4:2:2 (YUY2/YUYV), 16 bit/pixel -> use addYUV422Frame()
/// - RGB565: uncompressed 16-bit RGB (5-6-5) -> use addRGB565Frame()
/// - RGB666: uncompressed 18-bit RGB, 3 bytes/pixel (each byte's 6
///   significant bits left-justified) - decoder pixel output only, no
///   dedicated Muxer addXxxFrame()
/// - RGB888: uncompressed 24-bit RGB, 3 bytes/pixel, full precision -
///   decoder pixel output only, no dedicated Muxer addXxxFrame()
/// - I420: planar 4:2:0 YUV (aka IYUV/YUV420), 12 bit/pixel ->
///   use addI420Frame()
/// - MPEG1: ISO/IEC 11172-2 compressed video, variable frame size -> use
///   addVideoFrame() (ContainerMPG's MuxerMPG/DemuxerMPG)
/// - UNKNOWN: decoder only - the codec did not match any of the above
/// @ingroup video
enum class VideoFormat {
  H264,
  MJPEG,
  MPEG4,
  RAW,
  YUV422,
  RGB565,
  RGB666,
  RGB888,
  I420,
  MPEG1,
  UNKNOWN
};

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
 * @brief Basic video information (width/height/codec/frame size), analogous
 * to AudioInfo - common to both DemuxerAVI and DemuxerMP4, accessible via
 * their getVideoInfo() getter.
 * @ingroup video
 */
struct VideoInfo {
  /// Frame width in pixels
  uint16_t width = 0;
  /// Frame height in pixels
  uint16_t height = 0;
  /// Frames/sec, if known - 0 if not (e.g. not yet determined, or a
  /// VideoFrameSource that doesn't have a fixed rate).
  float fps = 0;
  /// Video codec - VideoFormat::UNKNOWN if not (yet) determined
  VideoFormat format = VideoFormat::UNKNOWN;
  /// Fixed size of a single frame in bytes - only meaningful for
  /// uncompressed formats (RAW/YUV422/RGB565/I420); 0 for compressed
  /// formats (H264/MJPEG/MPEG4), whose frame size varies per frame.
  uint32_t frame_size = 0;
  /// Total size of the container file/stream in bytes, if known. For
  /// DemuxerAVI this comes from the RIFF header's declared size (0 if the
  /// stream is unbounded/streamed). MP4 has no equivalent declared field,
  /// so DemuxerMP4 reports the number of bytes received so far instead
  /// (only equal to the true total once the whole stream has been fed in).
  uint32_t total_file_size = 0;

  bool operator==(const VideoInfo& alt) const {
    return width == alt.width && height == alt.height && fps == alt.fps &&
           format == alt.format && frame_size == alt.frame_size &&
           total_file_size == alt.total_file_size;
  }
  bool operator!=(const VideoInfo& alt) const { return !(*this == alt); }
  /// True if width/height are both defined
  operator bool() const { return width > 0 && height > 0; }
  void logInfo(const char* source = "") {
    LOGI(
        "%s width: %d / height: %d / fps: %f / format: %d / frame_size: %d / "
        "total_file_size: %d",
        source, (int)width, (int)height, fps, (int)format, (int)frame_size,
        (int)total_file_size);
  }
};

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

class VideoInfoSource {
 public:
  virtual VideoInfo videoInfo() = 0;
};

/**
 * @brief Abstract class for video playback. This class is used to assemble a
 * complete video frame in memory. A video frame is written via one or more
 * write() calls, then finalized with flush() - implementations use flush()
 * to know a frame is complete (there is no separate frame-size hint, unlike
 * a length-prefixed chunk format).
 * @ingroup video
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class VideoOutput {
 public:
  virtual size_t write(const uint8_t* data, size_t len) = 0;
  /// Finalizes the frame most recently written via one or more write()
  /// calls - see class comment. Default no-op for implementations that
  /// display/decode synchronously in write() instead.
  virtual void flush() {}
  /// Hint to skip the expensive part of displaying the next frame(s) (e.g.
  /// the panel refresh) while still accepting and fully processing write()
  /// calls - used to recover from falling behind the playback schedule
  /// without breaking a codec's decode state (e.g. H.264 inter-prediction
  /// reference chain, which requires every frame to still be decoded even
  /// if it's never shown). Default no-op: implementations that can't skip
  /// rendering cheaply just ignore it and always render.
  virtual void setSkipRender(bool skip) {}

  /// True if `data` (one complete encoded frame, as handed to write())
  /// is a keyframe/sync-sample - self-contained, decodable without any
  /// earlier frame. Used e.g. by PacedVideoOutput to decide which
  /// frames are safe to drop, and whether it's safe to resume decoding
  /// after abandoning a backlog (see its own class comment). Default
  /// false: a plain VideoOutput doesn't know or care about codec
  /// structure - override this in a decoder for the bitstream format it
  /// actually parses (see H264Decoder/H264DecoderESP32S3's
  /// isH264KeyFrame()-based override, MPGDecoder's isMpeg1KeyFrame()-
  /// based one). Getting this right matters beyond bookkeeping: a target
  /// whose frames are never recognized as keyframes can leave a caller
  /// like PacedVideoOutput unable to ever resume after a resync.
  virtual bool isKeyFrame(const uint8_t* data, size_t len) { return false; }

  /// Optional: returns the time (ms) spent in the last write() call
  virtual uint32_t getWriteTimeMs() const { return 0; }

  /// True if the most recent write()+flush() call actually produced a
  /// displayable picture - default true, matching every synchronous
  /// decoder (H264Decoder, MJPEGDecoder, ...), which always decodes and
  /// pushes pixels fully within that one call. Override this only if
  /// your decoder can legitimately accept/decode a frame's bytes without
  /// emitting a picture during that same call - e.g. MPGDecoder, whose
  /// B-picture display-order reordering can hold a just-decoded picture
  /// back and instead emit an earlier one (or nothing at all) from a
  /// given write(), see its own override. Used by PacedVideoOutput to
  /// avoid counting/timing a call that did no real rendering work as a
  /// rendered frame - without this, its outputFPS()/frameCountI()/
  /// frameCountP()/avgFrameMs() would overcount for such a decoder.
  virtual bool hadOutput() const { return true; }
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

// PacedVideoOutput derives from VideoOutput (defined above), so this must
// come after the closing brace - see PacedVideoOutput.h's
// own #include "AudioTools/Video/Video.h" for the other half of this
// mutually-including-but-order-safe-via-pragma-once pair.
#include "AudioTools/Video/PacedVideoOutput.h"
