#pragma once
#include <algorithm>
#include <string.h>
#include "AudioTools/AudioCodecs/AudioCodecsBase.h"
#include "AudioTools/AudioCodecs/AudioFormat.h"
#include "AudioTools/AudioCodecs/ContainerCommon.h"
#include "AudioTools/CoreAudio/AudioBasic/Collections/Vector.h"
#include "AudioTools/Video/Video.h"

namespace audio_tools {

// packet_start_code / stream_id values used by the ISO/IEC 11172-1 (MPEG-1)
// Program Stream (also reused, unchanged, by MPEG-2 Program Streams)
static const uint8_t MPG_PACK_START_CODE = 0xBA;
static const uint8_t MPG_SYSTEM_HEADER_START_CODE = 0xBB;
static const uint8_t MPG_PROGRAM_END_CODE = 0xB9;
static const uint8_t MPG_PROGRAM_STREAM_MAP = 0xBC;
static const uint8_t MPG_PRIVATE_STREAM_1 = 0xBD;
static const uint8_t MPG_PADDING_STREAM = 0xBE;
static const uint8_t MPG_PRIVATE_STREAM_2 = 0xBF;
/// First (and, in this single-track implementation, only) video stream_id
static const uint8_t MPG_VIDEO_STREAM_ID = 0xE0;
/// First (and, in this single-track implementation, only) audio stream_id
static const uint8_t MPG_AUDIO_STREAM_ID = 0xC0;
/// MPEG system clock runs at 90 kHz - the unit PTS/DTS/SCR are expressed in
static const uint32_t MPG_CLOCK_HZ = 90000;

/**
 * @brief Minimal byte accumulator used by DemuxerMPG to parse start-code
 * delimited units that may straddle write() calls - analogous to (but
 * simpler than, since Program Stream framing has no nested LIST/CHUNK
 * structure) ContainerAVI.h's ParseBuffer.
 * @ingroup codecs
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class MPGParseBuffer {
 public:
  void resize(size_t size) { vec.resize(size + 4); }
  size_t writeArray(const uint8_t *data, size_t len) {
    size_t to_write = min(availableToWrite(), len);
    memmove(vec.data() + count, data, to_write);
    count += to_write;
    return to_write;
  }
  void consume(size_t len) {
    if (len > count) len = count;
    memmove(vec.data(), vec.data() + len, count - len);
    count -= len;
  }
  uint8_t *data() { return vec.data(); }
  size_t available() { return count; }
  size_t availableToWrite() { return vec.size() - count; }
  void clear() { count = 0; }

  /// Index of the next 00 00 01 start-code prefix at/after 'from', or -1
  long indexOfStartCode(size_t from = 0) {
    if (count < 3 || from + 3 > count) return -1;
    for (size_t i = from; i + 2 < count; i++) {
      if (vec[i] == 0 && vec[i + 1] == 0 && vec[i + 2] == 1) return (long)i;
    }
    return -1;
  }

 protected:
  Vector<uint8_t> vec{0};
  size_t count = 0;
};

/**
 * @brief MPEG-1 System (Program) Stream Demuxer, as defined by ISO/IEC
 * 11172-1: splits the pack_header/system_header/PES_packet framing apart
 * and forwards the raw MPEG-1 video (ISO/IEC 11172-2) and MPEG-1 audio
 * (ISO/IEC 11172-3, Layer I/II/III) elementary streams to setOutputVideo()/
 * setOutputAudio(). Both elementary streams are self-delimiting (the video
 * ES has its own picture_start_code sequence, the audio ES its own frame
 * sync word) so, like DemuxerAVI/DemuxerMP4, no separate decoding is done
 * here - point the outputs at whatever decoder understands the codec (e.g.
 * an MPEG1 video decoder, or an EncodedAudioStream wrapping an MP3/MP2
 * AudioDecoder).
 *
 * Scope (v1): a single MPEG-1 video track (stream_id 0xE0) and a single
 * MPEG-1 audio track (stream_id 0xC0) - the common case for files produced
 * by encoders targeting this container (see MuxerMPG). Only the plain
 * ISO/IEC 11172-1 pack_header (fixed 12 bytes, no stuffing) and PES header
 * (PTS/DTS-or-none, no stuffing bytes, no STD_buffer field) are recognized;
 * MPEG-2-style Program Streams (marker pattern '01' at the start of the
 * pack header) are not supported.
 *
 * The audio track is forwarded as a plain, continuous elementary-stream
 * byte stream (PES packet boundaries are not required to line up with MP3/
 * MP2 frame boundaries - the decoder resyncs on its own frame sync word),
 * so setOutputAudio() can point straight at an EncodedAudioStream wrapping
 * any MPEG audio AudioDecoder (e.g. MP3DecoderHelix):
 * @code
 * DemuxerMPG demuxer;
 * MP3DecoderHelix mp3;
 * EncodedAudioStream audioOut(&i2s, &mp3);  // MP3 -> PCM -> i2s
 * demuxer.setOutputAudio(audioOut);
 * demuxer.setOutputVideo(videoSink);
 * demuxer.begin();
 * // feed raw .mpg bytes:
 * demuxer.write(mpgData, len);
 * @endcode
 *
 * @ingroup codecs
 * @ingroup decoder
 * @ingroup video
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class DemuxerMPG : public Demuxer {
 public:
  /// @param bufferSize internal parse buffer size - must be able to hold at
  /// least one full pack_header/system_header/PES header (a few dozen bytes)
  DemuxerMPG(int bufferSize = 1024) { parse_buffer.resize(bufferSize); }

  /// Provides the container mime
  const char *mimeVideo() override { return "video/mpeg"; }
  /// Provides the audio mime - "audio/mpeg" covers MPEG-1 Layer I/II/III
  /// alike (there's no separately registered "audio/mp2"), but once the
  /// audio ES's own frame header has been probed (see probeAudioHeader())
  /// a Layer II stream - the common case for this container, and what
  /// MuxerMPG itself produces - gets an explicit RFC 6381 codecs
  /// parameter naming the layer. Layer I/III/not-yet-probed still report
  /// the plain generic mime.
  const char *mime() override {
    if (audio_layer == 2) return "audio/mpeg; codecs=\"mpeg1-layer2\"";
    return toMime(AudioFormat::MP3);
  }

  bool begin() override {
    parse_buffer.clear();
    is_parsing_active = true;
    unit_open = false;
    is_skip_unit = false;
    unbounded = false;
    video_frame_open = false;
    audio_frame_open = false;
    unit_len = 0;
    unit_holds_video = false;
    video_header_parsed = false;
    audio_header_parsed = false;
    video_probe.clear();
    audio_probe.clear();
    video_width = 0;
    video_height = 0;
    video_fps = 0;
    audio_sample_rate = 0;
    audio_channels = 0;
    audio_layer = 0;
    total_bytes = 0;
    playback_start_set = false;
    return true;
  }

  void end() override {
    if (video_frame_open) flushVideoUnit();
    if (audio_frame_open) flushAudioUnit();
    video_frame_open = false;
    audio_frame_open = false;
    is_parsing_active = false;
  }

  operator bool() override { return is_parsing_active; }

  /// Defines the audio output stream - e.g. an EncodedAudioStream wrapping
  /// an MP3/MP2 AudioDecoder, or any other Print if you want the raw
  /// payload as-is.
  void setOutputAudio(Print &out) override { p_output_audio = &out; }

  /// Satisfies the AudioWriter/AudioDecoder interface - needed so
  /// EncodedAudioOutput/EncodedAudioStream's polymorphic AudioDecoder*
  /// wiring reaches the audio output correctly; delegates to
  /// setOutputAudio().
  void setOutput(Print &out) override { setOutputAudio(out); }

  /// Defines the video output - e.g. a VideoOutput implementation, or any
  /// other Print if you want the raw MPEG-1 video ES as-is.
  void setOutputVideo(Print &out) override { p_output_video = &out; }
  void setOutputVideo(VideoOutput &out) { p_output_video_video = &out; }

  /// Not applicable: MPEG-1 audio (Layer I/II/III) is compressed and
  /// self-framed (its own sync word), unlike PCM - it never needs a
  /// synthetic WAV header. Provided only to satisfy the Demuxer interface.
  void setSendWavHeader(bool flag) override { (void)flag; }

  /// Overrides the audio/video pacing mechanism - same role as
  /// DemuxerMP4::setVideoAudioSync(); defaults to a VideoAudioBufferedSync
  /// so this container gets the same real-time pacing MP4 does instead of
  /// dispatching audio/video as fast as bytes can be parsed (previously the
  /// PES PTS was parsed and immediately discarded - see parsePes()).
  void setVideoAudioSync(VideoAudioSync &sync) { p_synch = &sync; }

  /// Common video info (width/height/fps/format), parsed from the video
  /// ES's own sequence_header (ISO/IEC 11172-2) once seen - all zero/
  /// VideoFormat::MPEG1 until then.
  VideoInfo getVideoInfo() override {
    VideoInfo result;
    result.width = video_width;
    result.height = video_height;
    result.fps = video_fps;
    result.format = VideoFormat::MPEG1;
    result.frame_size = 0;  // compressed: size varies per frame
    result.total_file_size = total_bytes;
    return result;
  }

  /// Common audio info, parsed from the audio ES's own MPEG audio frame
  /// header once seen - falls back to 44100/stereo if not yet available.
  AudioInfoFormat getAudioInfo() override {
    AudioInfoFormat result;
    result.sample_rate = audio_sample_rate > 0 ? audio_sample_rate : 44100;
    result.channels = audio_channels > 0 ? audio_channels : 2;
    result.bits_per_sample = 16;  // nominal - the codec is compressed
    result.format = AudioFormat::MP3;
    return result;
  }

  size_t write(const uint8_t *data, size_t len) override {
    size_t result = parse_buffer.writeArray(data, len);
    total_bytes += result;
    if (is_parsing_active) {
      while (parse()) {
      }
    }
    return result;
  }

 protected:
  bool is_parsing_active = true;
  MPGParseBuffer parse_buffer;
  Print *p_output_audio = nullptr;
  Print *p_output_video = nullptr;
  VideoOutput *p_output_video_video = nullptr;
  uint32_t total_bytes = 0;

  // current streamed unit (PES payload, or a skipped block's payload)
  bool unit_open = false;
  bool is_skip_unit = false;
  bool is_video_unit = false;
  bool unbounded = false;  // PES_packet_length == 0 (video only)
  size_t remaining = 0;

  // video access-unit framing: a new PES with a PTS starts a new picture
  bool video_frame_open = false;
  // audio access-unit framing: a new PES with a PTS starts a new audio frame
  bool audio_frame_open = false;

  // Accumulates one complete access unit (a whole picture, or a whole
  // audio frame) across every PES fragment and every parse_buffer-sized
  // write() chunk that contributes to it, so the decoder downstream gets
  // one single write() call per unit instead of an arbitrary number of
  // partial ones - see deliver()/flushVideoUnit()/flushAudioUnit(). A
  // container-demuxed stream's chunk boundaries don't line up with
  // picture/frame boundaries at all (the parse buffer above is a small,
  // fixed size; a real access unit is routinely several times larger),
  // and feeding a video decoder like TinyMPGDecoder a boundary-split
  // buffer under its default whole-buffer contract can silently
  // reconstruct a picture from stale/wrong macroblocks rather than
  // failing cleanly - see that project's own TinyMPGDecoder::write() doc
  // comment, which calls this exact demuxer out by name.
  //
  // One buffer shared between audio and video rather than one each: a
  // muxer (see MuxerMPG) writes one access unit's PES fragments back to
  // back, so in practice audio and video are never both mid-unit at the
  // same time - one buffer, sized to whichever unit turns out biggest
  // (routinely a video picture, far larger than one audio frame), covers
  // both. unit_holds_video says which type currently owns its contents
  // (only meaningful while unit_len > 0); if some other encoder's output
  // ever *did* interleave the two mid-unit, deliver() flushes whichever
  // unit is already in the buffer first rather than letting the new
  // bytes corrupt it - a premature flush of that one unit, not silent
  // corruption.
  //
  // unit_len is tracked separately from the Vector's own size(), which is
  // deliberately over-allocated for amortized growth - Vector::resize()
  // always allocates exactly the requested size with no spare capacity
  // (AudioBasic/Collections/Vector.h), so resizing it to the exact new
  // length on every single delivered fragment - one picture can need a
  // dozen of these - would reallocate and copy everything accumulated so
  // far *every single call*, the same O(N^2) cost this project's
  // ChunkedSampleTableStore (SampleTableStore.h) exists to avoid for
  // MP4's sample tables.
  Vector<uint8_t> unit_buffer;
  size_t unit_len = 0;
  bool unit_holds_video = false;

  // lazily parsed from the elementary streams themselves
  bool video_header_parsed = false;
  bool audio_header_parsed = false;
  uint16_t video_width = 0;
  uint16_t video_height = 0;
  float video_fps = 0;
  uint32_t audio_sample_rate = 0;
  uint8_t audio_channels = 0;
  uint8_t audio_layer = 0;  ///< 0=not yet probed/reserved, else 1/2/3
  Vector<uint8_t> video_probe;
  Vector<uint8_t> audio_probe;
  static const size_t VIDEO_PROBE_MAX = 64;

  // audio/video pacing (see setVideoAudioSync()) - defaults to the same
  // buffered strategy DemuxerMP4 uses.
  VideoAudioBufferedSync default_synch{10 * 1024, -20};
  VideoAudioSync *p_synch = &default_synch;
  bool playback_start_set = false;
  uint32_t playback_start_ms = 0;

  /// Parses/consumes as much as is currently buffered; returns true if
  /// progress was made (call again - more units may already be buffered),
  /// false if it needs more data (wait for the next write()).
  bool parse() {
    if (unit_open) return continuePayload();

    if (parse_buffer.available() < 4) return false;
    uint8_t *buf = parse_buffer.data();
    if (!(buf[0] == 0 && buf[1] == 0 && buf[2] == 1)) {
      long idx = parse_buffer.indexOfStartCode();
      if (idx < 0) {
        size_t avail = parse_buffer.available();
        if (avail > 2) parse_buffer.consume(avail - 2);
        return false;
      }
      parse_buffer.consume((size_t)idx);
      if (parse_buffer.available() < 4) return false;
      buf = parse_buffer.data();
    }

    uint8_t id = buf[3];
    if (id == MPG_PACK_START_CODE) return parsePack();
    if (id == MPG_SYSTEM_HEADER_START_CODE) return parseSystemHeader();
    if (id == MPG_PROGRAM_END_CODE) {
      parse_buffer.consume(4);
      if (video_frame_open) flushVideoUnit();
      if (audio_frame_open) flushAudioUnit();
      video_frame_open = false;
      audio_frame_open = false;
      LOGI("MPEG_program_end_code");
      return true;
    }
    if (id == MPG_PROGRAM_STREAM_MAP || id == MPG_PRIVATE_STREAM_1 ||
        id == MPG_PADDING_STREAM || id == MPG_PRIVATE_STREAM_2)
      return parseSkip();
    if (id >= 0xC0 && id <= 0xDF) return parsePes(id, false);
    if (id >= 0xE0 && id <= 0xEF) return parsePes(id, true);

    // reserved/unrecognized id: drop this byte and resync on the next
    // start code
    parse_buffer.consume(1);
    return true;
  }

  bool parsePack() {
    if (parse_buffer.available() < 12) return false;
    parse_buffer.consume(12);
    return true;
  }

  bool parseSystemHeader() {
    if (parse_buffer.available() < 6) return false;
    uint8_t *buf = parse_buffer.data();
    uint16_t header_length = ((uint16_t)buf[4] << 8) | buf[5];
    size_t total = 6 + header_length;
    if (parse_buffer.available() < total) return false;
    parse_buffer.consume(total);
    return true;
  }

  /// program_stream_map / private_stream_1 / padding_stream /
  /// private_stream_2: same PES_packet_length-prefixed framing, payload
  /// discarded.
  bool parseSkip() {
    if (parse_buffer.available() < 6) return false;
    uint8_t *buf = parse_buffer.data();
    uint16_t len = ((uint16_t)buf[4] << 8) | buf[5];
    parse_buffer.consume(6);
    if (len == 0) return true;
    unit_open = true;
    is_skip_unit = true;
    is_video_unit = false;
    unbounded = false;
    remaining = len;
    return continuePayload();
  }

  bool parsePes(uint8_t id, bool isVideo) {
    // need the fixed 6-byte PES header plus 1 byte to peek the optional
    // header's shape
    if (parse_buffer.available() < 7) return false;
    uint8_t *buf = parse_buffer.data();
    uint16_t pes_len = ((uint16_t)buf[4] << 8) | buf[5];
    uint8_t peek = buf[6];

    size_t optional_len;
    bool has_pts = false;
    if (peek == 0x0F) {
      optional_len = 1;
    } else if ((peek >> 4) == 0x02) {
      optional_len = 5;
      has_pts = true;
    } else if ((peek >> 4) == 0x03) {
      optional_len = 10;
      has_pts = true;
    } else {
      LOGW("DemuxerMPG: unsupported PES optional header 0x%02X - treating as no-timestamp",
           (int)peek);
      optional_len = 1;
    }
    if (parse_buffer.available() < 6 + optional_len) return false;

    uint64_t pts = 0;
    if (has_pts) pts = readTimestamp(buf + 6);
    parse_buffer.consume(6 + optional_len);
    (void)id;

    bool is_unbounded = false;
    size_t payload_len = 0;
    if (pes_len == 0) {
      // PES_packet_length == 0 ("unbounded") is only meaningful for video
      is_unbounded = isVideo;
    } else if (pes_len < optional_len) {
      payload_len = 0;  // malformed - be defensive rather than underflow
    } else {
      payload_len = (size_t)pes_len - optional_len;
    }

    if (!isVideo && has_pts) {
      // a PTS marks the first fragment of a new audio access unit - close
      // out the previous one (see audio_unit_buffer's comment)
      if (audio_frame_open) flushAudioUnit();
      audio_frame_open = true;
    }

    bool skip_late_picture = false;
    if (isVideo && has_pts) {
      // a PTS marks the first fragment of a new access unit - close out
      // the previous one
      if (video_frame_open) flushVideoUnit();
      video_frame_open = true;

      // Pace this new picture against wall-clock time using its PTS (a
      // MPG_CLOCK_HZ=90kHz tick count) - mirrors DemuxerMP4::dispatchVideo()'s
      // schedule-anchored wait.
      uint32_t scheduledMs = (uint32_t)(pts * 1000 / MPG_CLOCK_HZ);
      uint32_t nowMs = millis();
      if (!playback_start_set) {
        playback_start_ms = nowMs;
        playback_start_set = true;
      }
      uint32_t scheduledWallMs = playback_start_ms + scheduledMs;
      if (nowMs > scheduledWallMs) {
        // Only skip if this picture is never used as a reference (a
        // B-picture, coding_type==3) - see peekPictureCodingType(). An
        // I/P picture (or a coding type we can't determine yet) is
        // decoded immediately instead, no wait either since we're
        // already behind.
        int codingType = peekPictureCodingType(parse_buffer.data(),
                                                parse_buffer.available());
        if (codingType == 3) {
          LOGW("DemuxerMPG: video picture %u ms late - skipping (B-picture)",
               (unsigned)(nowMs - scheduledWallMs));
          skip_late_picture = true;
        } else {
          LOGW("DemuxerMPG: video picture %u ms late - decoding anyway (%s)",
               (unsigned)(nowMs - scheduledWallMs),
               codingType < 0 ? "coding type unknown" : "referenced by later pictures");
        }
      } else if (nowMs < scheduledWallMs) {
        uint32_t waitMs = scheduledWallMs - nowMs;
        p_synch->delayVideoFrame((int32_t)(std::min(waitMs, 200u) * 1000), 0);
      }
    }

    unit_open = true;
    is_skip_unit = skip_late_picture;
    is_video_unit = isVideo;
    unbounded = is_unbounded;
    remaining = payload_len;
    return continuePayload();
  }

  bool continuePayload() {
    if (unbounded) {
      long idx = parse_buffer.indexOfStartCode();
      size_t avail = parse_buffer.available();
      if (idx >= 0) {
        deliver((size_t)idx);
        parse_buffer.consume((size_t)idx);
        unit_open = false;
        return true;
      }
      if (avail > 2) {
        deliver(avail - 2);
        parse_buffer.consume(avail - 2);
      }
      return false;
    }
    size_t avail = parse_buffer.available();
    size_t to_write = min(avail, remaining);
    deliver(to_write);
    parse_buffer.consume(to_write);
    remaining -= to_write;
    if (remaining == 0) {
      unit_open = false;
      return true;
    }
    return false;
  }

  /// Appends one delivered fragment to the shared access-unit accumulator
  /// instead of writing it straight through - see unit_buffer's comment.
  /// The unit is only actually written once it's complete, from
  /// flushVideoUnit()/flushAudioUnit().
  void deliver(size_t len) {
    if (len == 0 || is_skip_unit) return;
    uint8_t *data = parse_buffer.data();
    // Defensive: the buffer already holds an in-progress unit of the
    // *other* type - real interleaving, not expected from MuxerMPG's own
    // output but not this demuxer's to assume of every encoder. Flush it
    // now rather than let these bytes corrupt it.
    if (unit_len > 0 && unit_holds_video != is_video_unit) {
      if (unit_holds_video) flushVideoUnit(); else flushAudioUnit();
    }
    if (is_video_unit) {
      if (!video_header_parsed) probeVideoHeader(data, len);
    } else {
      if (!audio_header_parsed) probeAudioHeader(data, len);
    }
    unit_holds_video = is_video_unit;
    appendTo(data, len);
  }

  /// Appends to unit_buffer with amortized (doubling) growth - see its
  /// comment for why an exact-size resize() per call would be O(N^2).
  void appendTo(const uint8_t *data, size_t len) {
    size_t needed = unit_len + len;
    if (needed > (size_t)unit_buffer.size()) {
      size_t newCap = unit_buffer.size() == 0 ? 8192 : (size_t)unit_buffer.size() * 2;
      if (newCap < needed) newCap = needed;
      unit_buffer.resize(newCap);
    }
    memcpy(unit_buffer.data() + unit_len, data, len);
    unit_len += len;
  }

  /// Writes out the accumulated unit_buffer as one single, complete
  /// write() call if it currently holds a video picture, then resets it
  /// for the next unit. Call once a picture is known to be complete (a
  /// new video PES with a PTS starts, or the stream/end() is closing) -
  /// never mid-picture. A no-op (other than the downstream flush() call)
  /// if the buffer currently holds audio instead - see deliver().
  void flushVideoUnit() {
    if (unit_len > 0 && unit_holds_video) {
      if (p_output_video != nullptr)
        p_output_video->write(unit_buffer.data(), unit_len);
      if (p_output_video_video != nullptr)
        p_output_video_video->write(unit_buffer.data(), unit_len);
      unit_len = 0;
    }
    if (p_output_video != nullptr) p_output_video->flush();
    if (p_output_video_video != nullptr) p_output_video_video->flush();
  }

  /// Writes out the accumulated unit_buffer as one single, complete
  /// write() call (routed through p_synch, same as before - see
  /// setVideoAudioSync()) if it currently holds an audio frame, then
  /// resets it for the next unit. A no-op if the buffer currently holds
  /// video instead - see deliver().
  void flushAudioUnit() {
    if (unit_len == 0 || unit_holds_video) return;
    if (p_output_audio != nullptr)
      p_synch->writeAudio(p_output_audio, unit_buffer.data(), unit_len);
    unit_len = 0;
  }

  /// Reads a single 5-byte MPEG-1 timestamp field (PTS or DTS - both use
  /// the same bit layout, only the leading 4-bit id nibble differs, which
  /// is not part of this 5-byte window) at 'p'.
  static uint64_t readTimestamp(const uint8_t *p) {
    uint64_t ts = 0;
    ts |= ((uint64_t)(p[0] >> 1) & 0x07) << 30;
    ts |= ((uint64_t)((((uint16_t)p[1] << 8) | p[2]) >> 1) & 0x7FFF) << 15;
    ts |= (uint64_t)((((uint16_t)p[3] << 8) | p[4]) >> 1) & 0x7FFF;
    return ts;
  }

  /// Scans 'data' (the still-buffered start of a new video access unit)
  /// for the picture_start_code (00 00 01 00, ISO/IEC 11172-2 clause
  /// 2.4.3.3) and returns its picture_coding_type (1=I, 2=P, 3=B), or -1
  /// if the picture_start_code isn't within what's currently buffered
  /// (e.g. a GOP/sequence header ahead of it hasn't fully arrived yet).
  /// Only B-pictures are never used as a prediction reference by any
  /// other picture in MPEG-1 - used before skipping a late picture in
  /// parsePes(): dropping an I or P picture would corrupt every later
  /// picture's decode until the next keyframe, not just the one skipped
  /// (same reasoning as DemuxerMP4::isSafeToSkipAvcc()).
  static int peekPictureCodingType(const uint8_t *data, size_t len) {
    for (size_t i = 0; i + 6 <= len; i++) {
      if (data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 1 &&
          data[i + 3] == 0x00) {
        return (data[i + 5] >> 3) & 0x07;
      }
    }
    return -1;
  }

  /// Accumulates the first bytes of the video ES looking for its
  /// sequence_header (00 00 01 B3) to determine width/height/fps -
  /// harmless to keep calling once found, since video_header_parsed then
  /// short-circuits it.
  void probeVideoHeader(const uint8_t *data, size_t len) {
    size_t old_size = (size_t)video_probe.size();
    size_t take = old_size < VIDEO_PROBE_MAX ? VIDEO_PROBE_MAX - old_size : 0;
    if (take > len) take = len;
    for (size_t i = 0; i < take; i++) {
      uint8_t b = data[i];
      video_probe.push_back(b);
    }

    size_t n = (size_t)video_probe.size();
    for (size_t i = 0; i + 7 < n; i++) {
      if (video_probe[i] == 0 && video_probe[i + 1] == 0 &&
          video_probe[i + 2] == 1 && video_probe[i + 3] == 0xB3) {
        uint8_t b0 = video_probe[i + 4], b1 = video_probe[i + 5],
                b2 = video_probe[i + 6], b3 = video_probe[i + 7];
        video_width = (uint16_t)(((uint16_t)b0 << 4) | (b1 >> 4));
        video_height = (uint16_t)(((uint16_t)(b1 & 0x0F) << 8) | b2);
        static const float rates[16] = {0,       23.976f, 24.0f, 25.0f,
                                         29.97f,  30.0f,   50.0f, 59.94f,
                                         60.0f,   0,       0,     0,
                                         0,       0,       0,     0};
        video_fps = rates[b3 & 0x0F];
        video_header_parsed = true;
        break;
      }
    }
    if (video_header_parsed || n >= VIDEO_PROBE_MAX) {
      video_probe.clear();
      video_probe.shrink_to_fit();
    }
  }

  /// Inspects the first 4 bytes of the audio ES for an MPEG audio frame
  /// header (ISO/IEC 11172-3) to determine sample_rate/channels.
  void probeAudioHeader(const uint8_t *data, size_t len) {
    size_t old_size = (size_t)audio_probe.size();
    size_t take = old_size < 4 ? 4 - old_size : 0;
    if (take > len) take = len;
    for (size_t i = 0; i < take; i++) {
      uint8_t b = data[i];
      audio_probe.push_back(b);
    }
    if ((size_t)audio_probe.size() < 4) return;

    uint8_t b0 = audio_probe[0], b1 = audio_probe[1], b2 = audio_probe[2],
            b3 = audio_probe[3];
    if (b0 == 0xFF && (b1 & 0xE0) == 0xE0) {
      uint8_t version = (b1 >> 3) & 0x03;  // 11=MPEG1, 10=MPEG2, 00=MPEG2.5
      uint8_t layer_bits = (b1 >> 1) & 0x03;  // 01=Layer III, 10=Layer II, 11=Layer I
      uint8_t sr_index = (b2 >> 2) & 0x03;
      uint8_t mode = (b3 >> 6) & 0x03;  // 11=mono
      static const uint32_t rates_v1[4] = {44100, 48000, 32000, 0};
      static const uint32_t rates_v2[4] = {22050, 24000, 16000, 0};
      static const uint32_t rates_v25[4] = {11025, 12000, 8000, 0};
      uint32_t rate = 0;
      if (version == 0x03)
        rate = rates_v1[sr_index];
      else if (version == 0x02)
        rate = rates_v2[sr_index];
      else if (version == 0x00)
        rate = rates_v25[sr_index];
      if (rate > 0) {
        audio_sample_rate = rate;
        audio_channels = (mode == 0x03) ? 1 : 2;
        // layer_bits -> standard layer number (I/II/III), 0 if reserved
        static const uint8_t layers[4] = {0, 3, 2, 1};
        audio_layer = layers[layer_bits];
        audio_header_parsed = true;
      }
    } else {
      // not a frame sync where we expected one - stop probing so we don't
      // keep re-checking every write(); getAudioInfo() falls back to
      // 44100/stereo
      audio_header_parsed = true;
    }
    audio_probe.clear();
    audio_probe.shrink_to_fit();
  }
};

/**
 * @brief MPEG-1 System (Program) Stream Encoder, as defined by ISO/IEC
 * 11172-1: muxes an already-encoded MPEG-1 video elementary stream (ISO/IEC
 * 11172-2) and an optional MPEG-1 audio elementary stream (ISO/IEC 11172-3,
 * Layer I/II/III) into pack_header/system_header/PES_packet framing written
 * to a Print (a local File to record, or e.g. a network Client to publish a
 * live stream).
 *
 * This is a *streaming* writer, like MuxerAVI: mux_rate is written as an
 * unspecified placeholder (all bits set) and there is no index/seek table -
 * sufficient for sequential playback but not for random access. Each
 * addVideoFrame()/addAudioFrame() call is one complete access unit; large
 * video frames are automatically split across multiple pack/PES units (PES
 * payload is limited to 16 bits) - only the first fragment of an access
 * unit carries a PTS, exactly what DemuxerMPG uses to find frame
 * boundaries again. SCR/PTS are derived from the frame/sample counters and
 * the configured fps/sample_rate (MPEG-1 audio access units are assumed to
 * be 1152 samples, true for Layer II and Layer III), not a wall clock.
 *
 * Usage:
 * @code
 * MuxerMPG mpg(client); // any Print: File, WiFiClient, ...
 * MuxerVideoConfig cfg;
 * cfg.width = 352;
 * cfg.height = 288;
 * cfg.fps = 25;
 * cfg.format = VideoFormat::MPEG1;
 * mpg.setVideoInfo(cfg);
 * mpg.begin();
 * // for each encoded MPEG-1 picture (a run of ES bytes up to the next
 * // picture_start_code):
 * mpg.addVideoFrame(mpeg1_picture_data, len);
 * @endcode
 *
 * An MPEG audio track can be fed directly from an AudioEncoder (e.g.
 * MP3EncoderLAME) - Muxer is itself a Print, so pointing the encoder's
 * output at the muxer and toggling setStreamType() around the PCM write()
 * is all that is needed:
 * @code
 * mpg.setAudioInfo(AudioInfoFormat(44100, 2, 16, AudioFormat::MP3));
 * MP3EncoderLAME mp3;
 * mp3.setOutput(mpg);
 * mp3.begin(AudioInfo(44100, 2, 16));
 * mpg.setStreamType(StreamContentType::Audio);  // mpg.write() -> addAudioFrame()
 * mp3.write(pcm_data, len);
 * @endcode
 * (VideoMuxer/VideoMuxerWithTasks do this setStreamType() toggling, and the
 * video-side driving, automatically - MuxerMPG implements the same Muxer
 * interface as MuxerAVI/MuxerMP4, so it plugs into them unmodified.) Note
 * that the SCR/PTS this muxer writes assume each addAudioFrame()/write()
 * call is exactly one 1152-sample MPEG audio frame; an encoder whose
 * write() calls don't align to real frame boundaries (LAME's often don't)
 * will still produce a byte-correct, fully decodable elementary stream -
 * only the fine-grained timestamp hinting in the file becomes approximate.
 *
 * @ingroup codecs
 * @ingroup encoder
 * @ingroup video
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class MuxerMPG : public Muxer {
 public:
  MuxerMPG() {
    video_cfg.format = VideoFormat::MPEG1;
    audio_info.format = AudioFormat::MP3;
  }
  MuxerMPG(Print &out) : MuxerMPG() { setOutput(out); }

  const char *mimeVideo() override { return "video/mpeg"; }

  /// Defines the output: e.g. a local File or a network Client
  void setOutput(Print &out) override { p_out = &out; }

  /// Defines the video track configuration - call before begin(). Set
  /// config.format = VideoFormat::MPEG1 (the only format this container
  /// carries as a real video track).
  void setVideoInfo(MuxerVideoConfig config) override { video_cfg = config; }
  MuxerVideoConfig getVideoInfo() override { return video_cfg; }

  /// Adds an (optional) audio track. info.format selects the codec tag
  /// (default/only meaningful choice: AudioFormat::MP3, used generically
  /// here for any MPEG-1 Layer I/II/III elementary stream). Call before
  /// begin().
  void setAudioInfo(AudioInfoFormat info) override {
    audio_info = info;
    if (audio_info.format == AudioFormat::UNKNOWN)
      audio_info.format = AudioFormat::MP3;
    has_audio = true;
  }
  AudioInfoFormat &audioInfo() override { return audio_info; }

  /// Writes the initial pack_header + system_header. Call after
  /// configuring video (and audio, if any) and before writing any frames.
  bool begin() override {
    if (p_out == nullptr) {
      LOGE("output not defined");
      return false;
    }
    if (video_cfg.width == 0 || video_cfg.height == 0) {
      LOGE("invalid video size: %d x %d", (int)video_cfg.width,
           (int)video_cfg.height);
      return false;
    }
    video_frame_count = 0;
    audio_frame_count = 0;
    last_scr = 0;
    writePackHeader(0);
    writeSystemHeader();
    is_open = true;
    return true;
  }

  /// Selects whether write() feeds the video or the audio track.
  void setStreamType(StreamContentType type) override {
    write_stream_type = type;
  }
  StreamContentType streamType() override { return write_stream_type; }

  /// Writes the MPEG_program_end_code trailer and closes the muxer.
  void end() override {
    if (is_open && p_out != nullptr) {
      static const uint8_t end_code[4] = {0x00, 0x00, 0x01,
                                           MPG_PROGRAM_END_CODE};
      p_out->write(end_code, 4);
    }
    is_open = false;
  }

  operator bool() override { return is_open; }

  /// VideoOutput API / generic sink: writes one complete frame to whichever
  /// track streamType() currently selects.
  size_t write(const uint8_t *data, size_t len) override {
    if (write_stream_type == StreamContentType::Audio)
      return addAudioFrame(data, len);
    return addVideoFrame(data, len);
  }

  /// Writes one complete MPEG-1 picture (a run of video ES bytes, normally
  /// starting at its picture_start_code) as one or more pack/PES units.
  /// @param isKeyFrame accepted for interface compatibility with
  /// Muxer::addVideoFrame() - the MPEG-1 video ES already encodes I/P/B
  /// picture_coding_type itself, so there is no separate container-level
  /// flag to write it into (same convention as MuxerAVI).
  size_t addVideoFrame(const uint8_t *data, size_t len,
                       bool isKeyFrame = true) override {
    (void)isKeyFrame;
    if (!is_open || data == nullptr || len == 0) return 0;
    size_t written = writeAccessUnit(MPG_VIDEO_STREAM_ID, data, len, videoPts());
    video_frame_count++;
    return written;
  }

  /// Program Stream has no defined mapping for raw/JPEG pixel data - writes
  /// the buffer through as if it were an MPEG-1 video ES fragment, with a
  /// warning, purely to satisfy the Muxer interface.
  size_t addJpegFrame(const uint8_t *data, size_t len) override {
    LOGW("MuxerMPG: MJPEG is not supported by MPEG-1 Program Stream");
    return addVideoFrame(data, len);
  }
  size_t addYUV422Frame(const uint8_t *data, size_t len) override {
    LOGW("MuxerMPG: raw YUV422 is not supported by MPEG-1 Program Stream");
    return addVideoFrame(data, len);
  }
  size_t addRGB565Frame(const uint8_t *data, size_t len) override {
    LOGW("MuxerMPG: raw RGB565 is not supported by MPEG-1 Program Stream");
    return addVideoFrame(data, len);
  }
  size_t addI420Frame(const uint8_t *data, size_t len) override {
    LOGW("MuxerMPG: raw I420 is not supported by MPEG-1 Program Stream");
    return addVideoFrame(data, len);
  }

  /// Writes one complete MPEG-1 audio frame (e.g. one Layer II/III frame,
  /// starting at its sync word) as a single pack/PES unit. Ignored
  /// (returns 0) if no audio track was configured via setAudioInfo().
  size_t addAudioFrame(const uint8_t *data, size_t len) override {
    if (!is_open || !has_audio || data == nullptr || len == 0) return 0;
    size_t written = writeAccessUnit(MPG_AUDIO_STREAM_ID, data, len, audioPts());
    audio_frame_count++;
    return written;
  }

  uint32_t videoFrameCount() { return video_frame_count; }
  uint32_t audioFrameCount() { return audio_frame_count; }

 protected:
  /// Video/audio PES payload is chunked at this size so the 16-bit
  /// PES_packet_length field (and the optional-header bytes) always fit,
  /// with margin to spare - real encoders commonly interleave in similarly
  /// sized packets rather than one huge PES per access unit.
  static const size_t MAX_PES_PAYLOAD = 4096;
  /// MPEG-1 Layer II/III audio frames are always 1152 samples
  static const uint32_t AUDIO_SAMPLES_PER_FRAME = 1152;

  Print *p_out = nullptr;
  MuxerVideoConfig video_cfg;
  AudioInfoFormat audio_info;
  bool has_audio = false;
  bool is_open = false;
  StreamContentType write_stream_type = StreamContentType::Video;
  uint32_t video_frame_count = 0;
  uint32_t audio_frame_count = 0;
  uint64_t last_scr = 0;
  /// mux_rate: 22-bit field, unit 50 bytes/sec - all-ones is used here as
  /// an "unspecified/streaming" placeholder, the same convention MuxerAVI
  /// uses for its unknown RIFF/movi sizes.
  static const uint32_t MUX_RATE = 0x3FFFFF;

  uint64_t videoPts() {
    float fps = video_cfg.fps > 0 ? video_cfg.fps : 25.0f;
    return (uint64_t)((double)video_frame_count * MPG_CLOCK_HZ / fps);
  }
  uint64_t audioPts() {
    uint32_t sr = audio_info.sample_rate > 0 ? audio_info.sample_rate : 44100;
    return (uint64_t)((double)audio_frame_count * AUDIO_SAMPLES_PER_FRAME *
                       MPG_CLOCK_HZ / sr);
  }

  uint64_t clampScr(uint64_t scr) {
    if (scr < last_scr) scr = last_scr;
    last_scr = scr;
    return scr;
  }

  void writePackHeader(uint64_t scr_in) {
    uint64_t scr = clampScr(scr_in);
    static const uint8_t start[4] = {0x00, 0x00, 0x01, MPG_PACK_START_CODE};
    p_out->write(start, 4);
    uint8_t b[8];
    b[0] = (uint8_t)(0x20 | (((scr >> 30) & 0x07) << 1) | 0x01);
    b[1] = (uint8_t)((scr >> 22) & 0xFF);
    b[2] = (uint8_t)((((scr >> 15) & 0x7F) << 1) | 0x01);
    b[3] = (uint8_t)((scr >> 7) & 0xFF);
    b[4] = (uint8_t)(((scr & 0x7F) << 1) | 0x01);
    b[5] = (uint8_t)(0x80 | ((MUX_RATE >> 15) & 0x7F));
    b[6] = (uint8_t)((MUX_RATE >> 7) & 0xFF);
    b[7] = (uint8_t)(((MUX_RATE & 0x7F) << 1) | 0x01);
    p_out->write(b, 8);
  }

  void writeStreamEntry(uint8_t id, bool isVideo) {
    // P-STD buffer bound: informational only - 46 (x1024 = ~46KB) for
    // video, 4 (x128 = 512 bytes, ~1 audio frame) for audio
    uint16_t bound = isVideo ? 46 : 4;
    uint8_t b[3];
    b[0] = id;
    b[1] = (uint8_t)(0xC0 | (isVideo ? 0x20 : 0x00) | ((bound >> 8) & 0x1F));
    b[2] = (uint8_t)(bound & 0xFF);
    p_out->write(b, 3);
  }

  void writeSystemHeader() {
    int num_streams = 1 + (has_audio ? 1 : 0);
    uint16_t header_length = (uint16_t)(6 + 3 * num_streams);
    static const uint8_t start[4] = {0x00, 0x00, 0x01,
                                      MPG_SYSTEM_HEADER_START_CODE};
    p_out->write(start, 4);
    uint8_t len_b[2] = {(uint8_t)(header_length >> 8),
                         (uint8_t)header_length};
    p_out->write(len_b, 2);

    uint16_t audio_bound = has_audio ? 1 : 0;
    uint16_t video_bound = 1;
    uint8_t b[6];
    // marker(1) + mux_rate(22) + marker(1)
    b[0] = (uint8_t)(0x80 | ((MUX_RATE >> 15) & 0x7F));
    b[1] = (uint8_t)((MUX_RATE >> 7) & 0xFF);
    b[2] = (uint8_t)(((MUX_RATE & 0x7F) << 1) | 0x01);
    // audio_bound(6) + fixed_flag(1)=0 + CSPS_flag(1)=0
    b[3] = (uint8_t)((audio_bound & 0x3F) << 2);
    // audio_locked(1)=0 + video_locked(1)=0 + marker(1)=1 + video_bound(5)
    b[4] = (uint8_t)(0x20 | (video_bound & 0x1F));
    b[5] = 0xFF;  // reserved_byte
    p_out->write(b, 6);

    writeStreamEntry(MPG_VIDEO_STREAM_ID, true);
    if (has_audio) writeStreamEntry(MPG_AUDIO_STREAM_ID, false);
  }

  void writeTimestampField(uint8_t id4, uint64_t ts) {
    uint8_t b[5];
    b[0] = (uint8_t)((id4 << 4) | (((ts >> 30) & 0x07) << 1) | 0x01);
    uint16_t mid = (uint16_t)((((ts >> 15) & 0x7FFF) << 1) | 0x01);
    uint16_t low = (uint16_t)(((ts & 0x7FFF) << 1) | 0x01);
    b[1] = (uint8_t)(mid >> 8);
    b[2] = (uint8_t)mid;
    b[3] = (uint8_t)(low >> 8);
    b[4] = (uint8_t)low;
    p_out->write(b, 5);
  }

  /// Writes one pack_header + PES header + payload chunk (<= a few KB).
  size_t writeElementaryChunk(uint8_t stream_id, const uint8_t *data,
                              size_t len, bool withPts, uint64_t pts) {
    writePackHeader(pts);
    uint16_t optional_len = withPts ? 5 : 1;
    uint16_t pes_len = (uint16_t)(len + optional_len);
    uint8_t hdr[6] = {0,
                       0,
                       1,
                       stream_id,
                       (uint8_t)(pes_len >> 8),
                       (uint8_t)pes_len};
    p_out->write(hdr, 6);
    if (withPts) {
      writeTimestampField(0x02, pts);
    } else {
      // avoid Print::write(uint8_t): some Print/Stream implementations
      // (e.g. BaseStream-derived ones) buffer single-byte writes
      // separately from the write(data, len) path below, which would
      // reorder this marker byte after the payload - write it as a
      // one-element array instead so it goes through the same path.
      static const uint8_t no_ts = 0x0F;
      p_out->write(&no_ts, 1);
    }
    return p_out->write(data, len);
  }

  /// Splits one access unit (a full encoded video picture, or one audio
  /// frame) across as many pack/PES units as needed; only the first
  /// carries a PTS.
  size_t writeAccessUnit(uint8_t stream_id, const uint8_t *data, size_t len,
                        uint64_t pts) {
    size_t offset = 0;
    size_t written = 0;
    bool first = true;
    do {
      size_t chunk = len - offset;
      if (chunk > MAX_PES_PAYLOAD) chunk = MAX_PES_PAYLOAD;
      written +=
          writeElementaryChunk(stream_id, data + offset, chunk, first, pts);
      offset += chunk;
      first = false;
    } while (offset < len);
    return written;
  }
};

}  // namespace audio_tools
