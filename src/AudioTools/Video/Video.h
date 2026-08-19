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
inline bool isH264KeyFrame(const uint8_t *data, size_t len) {
  for (size_t i = 0; i + 3 < len; i++) {
    if (data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 1) {
      if ((data[i + 3] & 0x1F) == 5) return true;  // IDR slice
      i += 2;
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

  bool operator==(const VideoInfo &alt) const {
    return width == alt.width && height == alt.height && fps == alt.fps &&
           format == alt.format && frame_size == alt.frame_size &&
           total_file_size == alt.total_file_size;
  }
  bool operator!=(const VideoInfo &alt) const { return !(*this == alt); }
  /// True if width/height are both defined
  operator bool() const { return width > 0 && height > 0; }
  void logInfo(const char *source = "") {
    LOGI("%s width: %d / height: %d / fps: %f / format: %d / frame_size: %d / "
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
  virtual const uint8_t *nextFrame(size_t &len) = 0;
  /// Provides the width/height/fps/format (and, if known, frame_size/
  /// total_file_size) of the frames returned by nextFrame() - width,
  /// height, fps and format must be set, since VideoMuxerWithTasks uses
  /// them directly to configure the Muxer it writes to instead of
  /// requiring the caller to duplicate the same values there.
  virtual VideoInfo videoInfo() = 0;

  /// @brief Write the frame to the indicated destination
  bool writeTo(Print &out) {
    size_t len;
    const uint8_t *frame = nextFrame(len);
    if (frame == nullptr || len == 0) return false;
    size_t written = out.write(frame, len);
    return written = len;
  }
};


class VideoInfoSource {
 public:
  virtual VideoInfo videoInfo() = 0;
};


class VideoAudioSync;

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
  virtual size_t write(const uint8_t *data, size_t len) = 0;
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
  /// Optional: a demuxer that owns a VideoAudioSync (frame pacing) calls
  /// this once, typically from its setOutputVideo(VideoOutput&), so a
  /// decoder implementation can consult the sync object's admission policy
  /// (VideoAudioSync::shouldAdmitFrame()) before spending decode time on a
  /// frame, and its render-skip policy (isBehindSchedule()) before handing
  /// a decoded picture to its own downstream VideoOutput - see H264Decoder
  /// for the concrete use. Default no-op: most VideoOutput implementations
  /// (e.g. a raw display sink) don't need pacing awareness themselves,
  /// only decoders sitting between a demuxer and a display do.
  virtual void setSyncSource(VideoAudioSync *sync) {}
};


/**
 * @brief Logic to Synchronize video and audio output: This is the minimum
 * implementatin which actually does not synchronize, but directly processes the
 * data. No additinal memory is used! Provide your own optimized platform
 * specific implementation
 * @ingroup video
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class VideoAudioSync {
 public:
  /// Process the audio data
  virtual void writeAudio(Print *out, uint8_t *data, size_t size) {
    out->write(data, size);
  }

  /// Adds a delay after playing a frame to process with the correct frame rate.
  /// If the playing is too slow we return the mod to select the frames
  virtual void delayVideoFrame(int32_t microsecondsPerFrame,
                               uint32_t time_used_ms) {
    uint32_t delay_ms = microsecondsPerFrame / 1000;
    delay(delay_ms);
  }

  /// True if the last delayVideoFrame() call found playback already behind
  /// the target wall-clock schedule (nothing left to wait out) - a plain,
  /// honest state query with no side effects. For deciding whether to
  /// actually skip a frame's display refresh, use shouldSkipRender()
  /// instead: unlike this method, it never recommends skipping forever.
  /// Default false: this base class has no schedule to fall behind on.
  virtual bool isBehindSchedule() const { return false; }

  /// Render-skip policy, consulted by a decoder (via
  /// VideoOutput::setSyncSource()) right before handing a freshly decoded
  /// picture to its display output - the cheapest defense against
  /// falling behind: the frame is always fully decoded regardless (so any
  /// reference chain a codec maintains stays correct), only the display
  /// refresh itself is skipped. Unlike a plain isBehindSchedule() check,
  /// this must never recommend skipping indefinitely: with nothing ever
  /// rendered, delayVideoFrame()'s wall-clock schedule can look
  /// perpetually "behind" even once the pipeline has actually caught up,
  /// since nothing decoded further ever proves it - the exact trap
  /// shouldAdmitFrame() also has to guard against (see its comment).
  /// Mutates state (unlike isBehindSchedule()): call it at most once per
  /// frame. Default false: this base class never skips.
  virtual bool shouldSkipRender() { return false; }

  /// Signed lateness from the last delayVideoFrame() call: positive = this
  /// far ahead of schedule (about to wait that long), negative = this far
  /// behind. Finer-grained than isBehindSchedule(), for policies that need
  /// to distinguish "mildly late" from "badly late" (see
  /// shouldAdmitFrame()). Default 0: this base class has no schedule.
  virtual int32_t latenessMs() const { return 0; }

  /// Frame-admission policy, consulted by a decoder (via
  /// VideoOutput::setSyncSource()) before it spends decode time on a
  /// frame - lets sustained CPU pressure be handled by dropping decode
  /// work itself, not just the display refresh (see
  /// VideoOutput::setSkipRender() for that cheaper first line of
  /// defense). `isKeyFrame` is the decoder's codec-specific answer to "can
  /// this frame be decoded on its own, resetting any inter-frame
  /// dependency chain" (e.g. an H.264 IDR slice) - a decoder that has no
  /// such notion (e.g. MJPEG, where every frame is independent) can just
  /// always pass true. Returning false means the decoder must skip this
  /// frame's decode entirely (not even a partial attempt), so any
  /// reference chain the codec maintains will drift/degrade until the
  /// next isKeyFrame==true frame is admitted - an accepted trade-off for
  /// staying in real time, not a bug. Default true: this base class never
  /// drops frames.
  virtual bool shouldAdmitFrame(bool isKeyFrame) { return true; }
};

/**
 * @brief Logic to Synchronize video and audio output: we use a buffer to store
 * the audio and instead of delaying the frames with delay() we play audio. The
 * bufferSize defines the audio buffer in bytes. The correctionMs is used to
 * slow down or speed up the playback of the video to prevent any audio buffer
 * underflows.
 * @ingroup video
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class VideoAudioBufferedSync : public VideoAudioSync {
 public:
  VideoAudioBufferedSync(int bufferSize, int correctionMs) {
    ring_buffer.resize(bufferSize);
    correction_ms = correctionMs;
  }

  /// Process the audio data
  void writeAudio(Print *out, uint8_t *data, size_t size) {
    p_out = out;
    if (ring_buffer.availableForWrite() < size) {
      int bytes_to_play = size - ring_buffer.availableForWrite();
      uint8_t audio[bytes_to_play];
      ring_buffer.readArray(audio, bytes_to_play);
      p_out->write(audio, bytes_to_play);
    }
    size_t written = ring_buffer.writeArray(data, size);
    assert(written == size);
  }

  /// Adds a delay after playing a frame to process with the correct frame rate.
  /// If the playing is too slow we return the mod to select the frames
  void delayVideoFrame(int32_t microsecondsPerFrame, uint32_t time_used_ms) {
    // p_out is only ever set inside writeAudio() - if this is called
    // before any audio has been written yet (e.g. a file's first few
    // dispatched chunks happen to be video-only), there's nothing to
    // drain and no sink to drain it to; skip the wait entirely rather
    // than dereferencing a null p_out in the loop below.
    if (p_out == nullptr) return;
    uint32_t delay_ms = microsecondsPerFrame / 1000;
    uint64_t timeout = millis() + delay_ms + correction_ms;

    // 'p_out' here is the same Print DemuxerMP4::setOutputAudio() was
    // given - for an encoded codec (AAC, MP3, ...) that's a decoder's
    // input, not a raw PCM sink, so what's buffered in ring_buffer is
    // still-encoded bitstream bytes. Draining it 8 bytes at a time
    // (the previous approach) fed those bytes into the decoder in
    // fragments far smaller than one ADTS/frame boundary, repeated
    // thousands of times a second - reproducibly desynced libhelix's AAC
    // decoder on a real HE-AAC/SBR file, sending it down a path that
    // computed garbage (negative) band-count parameters and hung
    // indefinitely inside its SBR patch-building code. Draining the
    // current backlog in one shot (in generously-sized gulps, not
    // byte-by-byte) keeps writes closer to how the bytes were originally
    // framed by dispatchAudio() and avoided the hang in testing; only the
    // wait for the remaining window is spun out afterwards, with no
    // further writes into the decoder.
    uint8_t audio[512];
    int n;
    while ((n = ring_buffer.readArray(audio, sizeof(audio))) > 0) {
      p_out->write(audio, n);
    }
    while (millis() < timeout) {
      delay(1);
    }
  }

 protected:
  RingBuffer<uint8_t> ring_buffer{0};
  Print *p_out = nullptr;
  int correction_ms = 0;
};

/**
 * @brief Logic to Synchronize video and audio output: mirrors DemuxerMP4's
 * approach (dispatchVideo() in ContainerMP4.h) rather than using a ring
 * buffer - audio is written straight through (the default writeAudio() is
 * kept as-is), relying on the audio output itself (e.g. PortAudioStream's
 * blocking write) to set the real-time pace. Video pacing is computed
 * against a wall-clock schedule anchored at the first frame instead of a
 * fixed delay() every call: if we're already behind schedule, no delay is
 * added (letting the pipeline catch up instead of falling further behind);
 * if we're ahead, only the remaining time until the next scheduled frame is
 * waited out.
 *
 * Also owns the frame-admission policy (shouldAdmitFrame()): on top of
 * delayVideoFrame()'s after-the-fact pacing, a decoder can ask *before*
 * spending decode time on a frame whether to bother at all. Three
 * escalating tiers, cheapest first:
 *   1. Mildly behind (isBehindSchedule()): still decode every frame (the
 *      reference chain stays intact), just skip the display refresh -
 *      see VideoOutput::setSkipRender().
 *   2. Behind by more than pFrameSkipLatenessMs: stop decoding non-key
 *      frames for the rest of the current GOP - cheaper than #1 alone,
 *      but the decoder's reference chain now drifts until the next key
 *      frame, so this trades a slightly wrong picture for actually
 *      catching up.
 *   3. Still behind by more than gopSkipLatenessMs right as a new key
 *      frame arrives: skip that entire GOP's decode, key frame included.
 * Tiers 2-3 are visible on screen (drift/frozen picture) in exchange for
 * never blocking audio delivery on video decode that's already too slow
 * to matter - see setFrameAdmissionThresholds() to tune or disable them.
 * @ingroup video
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class VideoAudioClockSync : public VideoAudioSync {
 public:
  /// Waits only for the remaining time until the next frame's scheduled
  /// wall-clock instant - if already behind schedule, returns immediately
  /// instead of adding a full fixed delay on top of the lag.
  void delayVideoFrame(int32_t microsecondsPerFrame,
                        uint32_t time_used_ms) override {
    uint32_t nowMs = millis();
    if (!playback_start_set) {
      playback_start_ms = nowMs;
      playback_start_set = true;
      frame_index = 0;
    }
    uint32_t scheduledWallMs =
        playback_start_ms +
        (uint32_t)((uint64_t)frame_index * microsecondsPerFrame / 1000);
    frame_index++;
    // Positive: this frame is ahead of schedule by this many ms (about to
    // wait that long); negative: already late by this many ms (playback
    // is lagging - decode/render is taking longer than the frame period).
    int32_t aheadMs = (int32_t)scheduledWallMs - (int32_t)nowMs;
    LOGI("video frame %u: %s%d ms (decode+render took %u ms)", frame_index,
         aheadMs >= 0 ? "early by " : "late by ",
         aheadMs >= 0 ? aheadMs : -aheadMs, (unsigned)time_used_ms);
    last_ahead_ms = aheadMs;
    total_frames++;
    total_decode_write_ms += time_used_ms;
    if (aheadMs < p_frame_skip_lateness_ms) skip_p_frames_in_gop = true;
    if (nowMs < scheduledWallMs) {
      delay(scheduledWallMs - nowMs);
    }
    // else: already behind schedule - don't add to the lag
  }

  /// Running count of delayVideoFrame() calls since this object was
  /// created - one per video frame the demuxer has finished dispatching,
  /// including frames whose decode was skipped by shouldAdmitFrame() (see
  /// its class comment): delayVideoFrame() still runs for those, to keep
  /// frame_index in sync with the real stream position.
  uint32_t totalFrames() const { return total_frames; }

  /// Sum of every delayVideoFrame() call's time_used_ms - the wall-clock
  /// cost of decoding+writing each frame, NOT including the pacing
  /// delay() this class may add on top. Divide by totalFrames() for the
  /// true average per-frame processing cost, independent of how much idle
  /// waiting happened around it.
  uint64_t totalDecodeWriteMs() const { return total_decode_write_ms; }

  /// Reflects whether the *previous* frame's delayVideoFrame() call found
  /// playback behind schedule - checked by the demuxer before starting the
  /// *next* frame's decode, so it doubles as "were we already late going
  /// into this frame".
  bool isBehindSchedule() const override { return last_ahead_ms < 0; }

  int32_t latenessMs() const override { return last_ahead_ms; }

  /// Duty-cycled render-skip decision - see the base class comment for
  /// why a plain isBehindSchedule() check isn't safe here. Forces a
  /// render through at least every maxConsecutiveRenderSkips+1'th call
  /// regardless of lateness (tunable via setFrameAdmissionThresholds()),
  /// mirroring shouldAdmitFrame()'s tier-3 GOP duty cycle.
  bool shouldSkipRender() override {
    bool wantSkip = last_ahead_ms < 0;
    if (wantSkip && consecutive_render_skips >= max_consecutive_render_skips) {
      wantSkip = false;
    }
    consecutive_render_skips = wantSkip ? consecutive_render_skips + 1 : 0;
    return wantSkip;
  }

  /// See the class comment for tiers 2-3. `isKeyFrame` starting a new GOP
  /// re-evaluates both the P-frame cutoff (reset) and the whole-GOP
  /// decision (from the lateness seen at this exact boundary); a false
  /// result for a non-key frame reflects a cutoff latched by an earlier
  /// delayVideoFrame() call within the *same* GOP, not necessarily the
  /// current lateness.
  ///
  /// Skipping every key frame's decode is deliberately never allowed to
  /// latch forever: lateness is measured against a wall-clock schedule
  /// that only advances by actually decoding+rendering, so once nothing
  /// is admitted, there is no way left for the pipeline to prove it has
  /// caught up and the lockout would never lift on its own - the picture
  /// would freeze permanently on the last decoded frame while audio kept
  /// playing normally forever after. maxConsecutiveGopSkips (see
  /// setFrameAdmissionThresholds()) forces a GOP through periodically
  /// regardless of measured lateness, trading a bit of decode cost for a
  /// guarantee the picture keeps refreshing - and giving the pipeline an
  /// actual chance to observe whether it's caught up.
  bool shouldAdmitFrame(bool isKeyFrame) override {
    if (isKeyFrame) {
      skip_p_frames_in_gop = false;
      bool wantSkip = last_ahead_ms < gop_skip_lateness_ms;
      if (wantSkip && consecutive_gop_skips >= max_consecutive_gop_skips) {
        wantSkip = false;
      }
      skip_gop = wantSkip;
      consecutive_gop_skips = skip_gop ? consecutive_gop_skips + 1 : 0;
    }
    if (skip_gop) return false;
    if (!isKeyFrame && skip_p_frames_in_gop) return false;
    return true;
  }

  /// Tunes tiers 2-3 (see class comment) - both lateness thresholds in ms,
  /// both expected <= 0 (0 disables that tier: it triggers on any
  /// lateness at all). gopSkipMs should be more negative than
  /// pFrameSkipMs (a whole GOP is only worth sacrificing once cutting
  /// P-frames alone isn't enough) - not enforced, but an inverted pair
  /// means tier 3 fires before tier 2 ever gets a chance to.
  /// maxConsecutiveGopSkips caps how many GOPs in a row tier 3 may skip
  /// before one is forced through regardless of lateness (see
  /// shouldAdmitFrame()'s comment on why this must never be unbounded) -
  /// e.g. 5 means at least 1 GOP in every 6 is always decoded.
  void setFrameAdmissionThresholds(int32_t pFrameSkipMs, int32_t gopSkipMs,
                                    int maxConsecutiveGopSkips = 5) {
    p_frame_skip_lateness_ms = pFrameSkipMs;
    gop_skip_lateness_ms = gopSkipMs;
    max_consecutive_gop_skips = maxConsecutiveGopSkips;
  }

  /// Caps how many display refreshes in a row shouldSkipRender() may skip
  /// before one is forced through regardless of lateness - e.g. 10 (the
  /// default) means at least 1 render in every 11 always happens. Lower
  /// this for a more frequently-updating (but slower-to-catch-up) picture
  /// under sustained overload; see shouldSkipRender()'s comment for why a
  /// finite cap is required, not just a good idea.
  void setMaxConsecutiveRenderSkips(int maxSkips) {
    max_consecutive_render_skips = maxSkips;
  }

 protected:
  bool playback_start_set = false;
  uint32_t playback_start_ms = 0;
  uint32_t frame_index = 0;
  int32_t last_ahead_ms = 0;
  bool skip_p_frames_in_gop = false;
  bool skip_gop = false;
  int consecutive_gop_skips = 0;
  uint32_t total_frames = 0;
  uint64_t total_decode_write_ms = 0;
  int32_t p_frame_skip_lateness_ms = -150;
  int32_t gop_skip_lateness_ms = -400;
  int max_consecutive_gop_skips = 5;
  int consecutive_render_skips = 0;
  int max_consecutive_render_skips = 10;
};

}  // namespace audio_tools
