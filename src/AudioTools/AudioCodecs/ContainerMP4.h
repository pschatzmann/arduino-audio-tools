#pragma once

#include <algorithm>
#include <cstring>

#include "AudioTools/AudioCodecs/AudioCodecsBase.h"
#include "AudioTools/AudioCodecs/CodecWAV.h"
#include "AudioTools/AudioCodecs/ContainerCommon.h"
#include "AudioTools/AudioCodecs/M4ACommonDemuxer.h"
#include "AudioTools/AudioCodecs/MP4Parser.h"
#include "AudioTools/CoreAudio/Buffers.h"
#include "AudioTools/Video/Video.h"

namespace audio_tools {

/**
 * @brief DemuxerMP4 extracts both the audio and (H.264) video track from a
 * general, interleaved MP4/ISO-BMFF stream - unlike M4AAudioDemuxer/
 * M4AAudioFileDemuxer, which only handle a single (audio) track.
 *
 * This is a streaming (forward-only) demuxer: it does not need a seekable
 * source, so it works directly over e.g. an HTTP download (URLStream) or an
 * SD file read sequentially with StreamCopy - the same way DemuxerAVI is
 * used. It relies on the file being "faststart" muxed (moov before mdat,
 * e.g. `ffmpeg ... -movflags +faststart`) - the same requirement MP4Parser
 * and the M4A demuxers already document.
 *
 * How the interleaving is resolved without seeking: once 'moov' has been
 * fully parsed, every track's stsz (sample sizes) + stsc (sample-to-chunk)
 * + stco/co64 (absolute chunk offsets) tables are known. Those chunk
 * offsets already reveal the exact physical byte order the file was muxed
 * in - i.e. the order 'mdat' bytes will arrive in over a plain sequential
 * stream. So instead of seeking to the next-needed chunk, a "merge
 * schedule" (which track's chunk comes next, and how many samples it
 * holds) is computed once, and then simply followed by consuming that many
 * bytes off the incoming stream for each successive sample.
 *
 * @note v1 scope: one video track (H.264/avc1 only - HEVC is detected and
 * skipped with a warning) and one audio track (AAC or ALAC). Video output
 * is the raw H.264 Annex-B elementary stream (SPS/PPS from avcC converted
 * to Annex-B and prepended before the first frame) via VideoOutput - there
 * is no pixel decoder anywhere in this library, same scope boundary as
 * DemuxerAVI. No stts/timing support yet - write() dispatches samples as
 * soon as they are complete; pacing is the caller's responsibility.
 *
 * @ingroup codecs
 * @ingroup decoder
 * @ingroup video
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class DemuxerMP4 : public Demuxer {
 public:
  using Codec = M4ACommonDemuxer::Codec;

  /// This class only demuxes - it does not decode audio itself. Point
  /// setOutputAudio() at an EncodedAudioStream (wrapping whatever
  /// AudioDecoder matches the track's codec - AAC/ALAC) if you need the
  /// audio track decoded; the raw payload (ADTS-wrapped for AAC, raw for
  /// ALAC) is written through as-is otherwise. For ALAC, the decoder also
  /// needs the magic cookie exposed via audioALACMagicCookie() - configure
  /// your decoder with it (e.g. AudioDecoder::setCodecConfig()) once it's
  /// available (after the audio track's 'stsd' has been parsed, i.e. once
  /// getAudioInfo().format != AudioFormat::UNKNOWN). Point setOutputVideo() at
  /// a Print (e.g. a VideoOutput) to receive the demuxed H.264 Annex-B video
  /// track; leave unset to ignore video.
  DemuxerMP4() { setupParser(); }

  ~DemuxerMP4() { freeTracks(); }

  const char *mime() override { return "video/mp4"; }

  bool begin() override {
    freeTracks();
    schedule.clear();
    current_track = nullptr;
    p_video_track = nullptr;
    p_audio_track = nullptr;
    schedule_index = 0;
    samples_left_in_entry = 0;
    mdat_seen = false;
    box_accum_active = false;
    box_accum.clear();
    stsz_header_pending = true;
    sample_buffer.clear();
    sample_buffer_filled = 0;
    current_sample_size = 0;
    current_sample_track = nullptr;
    is_wav_header_sent = false;
    total_bytes_received = 0;
    playback_start_set = false;
    parser.begin();
    is_active = true;
    return true;
  }

  void end() override { is_active = false; }

  operator bool() override { return is_active; }


  /// Defines the audio output stream - e.g. an EncodedAudioStream wrapping
  /// an AudioDecoder that matches the audio track's codec, or any other
  /// Print if you want the raw payload as-is.
  void setOutputAudio(Print& out) override { p_output_audio = &out; }

  /// Satisfies the AudioWriter/AudioDecoder interface - needed so
  /// EncodedAudioOutput/EncodedAudioStream's polymorphic AudioDecoder*
  /// wiring (which calls setOutput() through that base class pointer) still
  /// reaches the audio output correctly; delegates to setOutputAudio().
  void setOutput(Print& out) override { setOutputAudio(out); }

  void setOutputVideo(Print& out) override { p_video_out = &out; }

  /// Common video info (width/height/format/frame_size/total_file_size),
  /// analogous to audioInfo() - the same VideoInfo type is also provided
  /// by DemuxerAVI. format is VideoFormat::H264 (the only video codec
  /// this class demuxes) once the video track's 'stsd' has been parsed,
  /// UNKNOWN before that or for an unsupported codec (e.g. HEVC).
  /// frame_size is always 0 (H264 frame size varies) - kept for symmetry
  /// with DemuxerAVI, whose raw pixel formats do have a fixed per-frame
  /// size. total_file_size has no declared-upfront equivalent in MP4 (no
  /// single header field states it, unlike AVI's RIFF header), so it
  /// reports the number of bytes received via write() so far instead -
  /// only equal to the true total once the whole stream has been fed in.
  VideoInfo getVideoInfo() override {
    VideoInfo result;
    result.total_file_size = total_bytes_received;
    // width==0 means 'avc1'/'hev1' hasn't been parsed yet (p_video_track can
    // already be set from 'hdlr' alone) - keep the all-UNKNOWN default then
    if (p_video_track == nullptr || p_video_track->width == 0) return result;
    result.width = p_video_track->width;
    result.height = p_video_track->height;
    result.format = p_video_track->is_hevc_unsupported ? VideoFormat::UNKNOWN
                                                       : VideoFormat::H264;
    result.frame_size = (uint32_t)videoFrameSizeBytes(
        result.format, result.width, result.height);
    return result;
  }
  /// Common audio info (sample_rate/channels/bits_per_sample), extended
  /// with the parsed codec format tag - the same AudioInfoFormat type is
  /// also provided by DemuxerAVI. Named getAudioInfo() rather than
  /// audioInfo() because AudioDecoder already declares a virtual
  /// AudioInfo audioInfo() (returning the plain, unextended type by
  /// value) - by-value virtual returns can't be covariantly widened in
  /// C++, so reusing that name for a subclass-returning version isn't
  /// possible (it would be a hard "invalid covariant return type" error,
  /// not silent hiding).
  AudioInfoFormat getAudioInfo() override {
    AudioInfoFormat result(info);
    result.format = audio_format;
    return result;
  }
  /// ALAC magic cookie (the 'alac' box payload, without its own size/type
  /// prefix) needed to configure an external ALAC decoder - empty unless
  /// getAudioInfo().format == AudioFormat::ALAC.
  Vector<uint8_t>& audioALACMagicCookie() {
    static Vector<uint8_t> empty;
    return p_audio_track ? p_audio_track->alacMagicCookie : empty;
  }

  /// Partial-write contract (like AVIDecoder::write()): MP4Parser's own
  /// internal buffer has finite capacity, so a single call may accept
  /// fewer bytes than requested - the caller is expected to call write()
  /// again with the remainder, exactly like the underlying Print contract.
  size_t write(const uint8_t* data, size_t len) override {
    if (!is_active) return 0;
    size_t written = parser.write(data, len);
    total_bytes_received += written;
    return written;
  }

 protected:
  /// sample-to-chunk table entry (stsc)
  struct StscEntry {
    uint32_t first_chunk;
    uint32_t samples_per_chunk;
  };

  /// time-to-sample table entry (stts): the next 'sample_count' samples
  /// each have a duration of 'sample_delta' ticks (in the track's own
  /// 'mdhd' timescale)
  struct SttsEntry {
    uint32_t sample_count;
    uint32_t sample_delta;
  };

  /// One track (audio or video) as found in 'moov'
  struct Track {
    enum class Kind { Unknown, Audio, Video } kind = Kind::Unknown;

    // audio
    Codec audio_codec = Codec::Unknown;
    int aacProfile = 2, sampleRateIdx = 4, channelCfg = 2;
    Vector<uint8_t> alacMagicCookie;

    // video (H.264 only in v1)
    uint16_t width = 0, height = 0;
    uint8_t nal_length_size = 4;
    Vector<uint8_t> sps_pps_annexb;
    bool avc_config_sent = false;
    bool is_hevc_unsupported = false;

    // sample tables, populated while parsing 'moov'
    Vector<uint32_t> sample_sizes;
    uint32_t fixed_sample_size = 0;
    uint32_t fixed_sample_count = 0;
    Vector<StscEntry> stsc;
    Vector<uint64_t> chunk_offsets;

    // playback timing: this track's own 'mdhd' timescale (ticks/second)
    // and its 'stts' time-to-sample table - both needed to turn a sample
    // index into a scheduled presentation time.
    uint32_t timescale = 0;
    Vector<SttsEntry> stts;

    // runtime cursor, used while walking the merge schedule
    uint32_t next_sample_index = 0;

    // runtime cursor for nextPtsTicks(), advanced once per dispatched
    // sample - kept separate from next_sample_index so it stays valid
    // even if next_sample_index is ever used for random access
    uint32_t stts_entry_idx = 0;
    uint32_t stts_entry_remaining = 0;
    uint64_t next_pts_ticks = 0;

    uint32_t sampleSize(uint32_t idx) {
      if (fixed_sample_size > 0) return fixed_sample_size;
      if (idx >= sample_sizes.size()) return 0;
      return sample_sizes[idx];
    }

    uint32_t sampleCount() {
      return fixed_sample_size > 0 ? fixed_sample_count
                                   : (uint32_t)sample_sizes.size();
    }

    /// Returns the next sample's scheduled presentation time (in this
    /// track's timescale ticks, i.e. relative to the track's own start)
    /// and advances the internal cursor - call exactly once per
    /// dispatched sample, in order. Returns 0 (no pacing) if 'stts'
    /// wasn't parsed.
    uint64_t nextPtsTicks() {
      uint64_t pts = next_pts_ticks;
      if (stts_entry_idx < stts.size()) {
        if (stts_entry_remaining == 0)
          stts_entry_remaining = stts[stts_entry_idx].sample_count;
        next_pts_ticks += stts[stts_entry_idx].sample_delta;
        if (stts_entry_remaining > 0) {
          stts_entry_remaining--;
          if (stts_entry_remaining == 0) stts_entry_idx++;
        }
      }
      return pts;
    }
  };

  /// One entry of the merge schedule: "next up is this track's next N
  /// samples" (N samples = 1 chunk)
  struct ScheduleEntry {
    Track* track;
    uint32_t sample_count;
  };

  MP4Parser parser;
  // heap-allocated (not Vector<Track>): current_track/p_video_track/
  // p_audio_track are captured *during* moov parsing, while more trak
  // boxes (and thus more push_back calls) may still arrive - Vector<T>
  // reallocates its backing array on growth, which would dangle any
  // pointer taken directly into it. Pointers to heap objects stay valid
  // regardless of how the Vector<Track*> itself grows.
  Vector<Track*> tracks;
  Track* current_track =
      nullptr;  ///< track currently being parsed (scoped by 'trak')

  void freeTracks() {
    for (size_t i = 0; i < tracks.size(); i++) delete tracks[i];
    tracks.clear();
  }
  Track* p_video_track = nullptr;  ///< first video track found
  Track* p_audio_track = nullptr;  ///< first audio track found

  Vector<ScheduleEntry> schedule;
  size_t schedule_index = 0;  ///< index into 'schedule'
  uint32_t samples_left_in_entry = 0;
  bool mdat_seen = false;

  Print* p_output_audio = nullptr;
  Print* p_video_out = nullptr;
  /// Codec format tag of the (first) audio track - AudioFormat::UNKNOWN
  /// until its 'stsd' has been parsed. Kept separately from AudioInfo
  /// (see AudioInfoFormat) since AudioInfo itself has no format field.
  AudioFormat audio_format = AudioFormat::UNKNOWN;
  bool send_wav_header = false;
  bool send_wav_header_explicit = false;
  bool is_wav_header_sent = false;
  /// Bytes received via write() so far this stream - MP4 has no declared
  /// total-file-size field to report instead (see getVideoInfo()).
  uint32_t total_bytes_received = 0;
  bool is_active = false;

  // video playback pacing: wall-clock reference captured at the first
  // dispatched video sample, against which each subsequent sample's
  // scheduled presentation time (from 'mdhd'+'stts') is compared - see
  // dispatchVideo().
  bool playback_start_set = false;
  uint32_t playback_start_ms = 0;

  // sample accumulation (mdat streaming)
  Vector<uint8_t> sample_buffer;
  size_t sample_buffer_filled = 0;
  uint32_t current_sample_size = 0;
  Track* current_sample_track = nullptr;
  bool current_sample_is_first_for_track = false;

  // scratch accumulation buffer, reused sequentially for stsz/stsc/stco/co64
  // (only one of these is ever "in progress" at a time in a linear parse)
  SingleBuffer<uint8_t> box_accum{0};
  // Explicit "are we mid-accumulation" state for box_accum, decoupled from
  // its fill level: box_accum.available()==0 is NOT a reliable "start of a
  // new box" signal, because some handlers (onStsz) legitimately drain it
  // to empty via clearArray() *while still mid-box* (e.g. right after
  // consuming a fixed-size sub-header, with more payload still to come) -
  // available()==0 in that case would be misread as a fresh box starting.
  bool box_accum_active = false;
  bool stsz_header_pending = true;

  /// Call at the top of every box_accum-using handler. Returns true the
  /// first time it's called for a given box (i.e. right when accumulation
  /// starts), false on continuations - callers that need to (re)initialize
  /// per-box state (e.g. onStsz's stsz_header_pending) should key off that.
  bool beginBoxAccum(const MP4Parser::Box& box) {
    if (box_accum_active) return false;
    box_accum.resize(box.size);
    box_accum.clear();
    box_accum_active = true;
    return true;
  }

  /// Call once a box_accum-using handler has fully processed a complete box.
  void endBoxAccum() {
    box_accum.clear();
    box_accum_active = false;
  }

  Vector<uint8_t> nal_tmp;  ///< scratch buffer for Annex-B conversion

  static uint32_t readU32(const uint8_t* p) {
    return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
  }
  static uint64_t readU64(const uint8_t* p) {
    return ((uint64_t)readU32(p) << 32) | readU32(p + 4);
  }
  static uint16_t readU16(const uint8_t* p) { return (p[0] << 8) | p[1]; }

  void setupParser() {
    parser.setReference(this);
    // suppress MP4Parser's default behavior of printing every box we don't
    // otherwise handle (mvhd, tkhd, stts, ...) to Serial/stdout
    parser.setCallback([](MP4Parser::Box&, void*) {});

    parser.setCallback(
        "trak",
        [](MP4Parser::Box& box, void* ref) {
          static_cast<DemuxerMP4*>(ref)->onTrak();
        },
        false);

    parser.setCallback(
        "hdlr",
        [](MP4Parser::Box& box, void* ref) {
          static_cast<DemuxerMP4*>(ref)->onHdlr(box);
        },
        false);

    parser.setCallback(
        "stsd",
        [](MP4Parser::Box& box, void* ref) {
          static_cast<DemuxerMP4*>(ref)->onStsd(box);
        },
        false);

    parser.setCallback(
        "mp4a",
        [](MP4Parser::Box& box, void* ref) {
          static_cast<DemuxerMP4*>(ref)->onMp4a(box);
        },
        false);
    parser.setCallback(
        "alac",
        [](MP4Parser::Box& box, void* ref) {
          static_cast<DemuxerMP4*>(ref)->onAlac(box);
        },
        false);
    parser.setCallback(
        "esds",
        [](MP4Parser::Box& box, void* ref) {
          static_cast<DemuxerMP4*>(ref)->onEsds(box);
        },
        false);

    parser.setCallback(
        "avc1",
        [](MP4Parser::Box& box, void* ref) {
          static_cast<DemuxerMP4*>(ref)->onAvc1(box);
        },
        false);
    parser.setCallback(
        "hev1",
        [](MP4Parser::Box& box, void* ref) {
          static_cast<DemuxerMP4*>(ref)->onHevc(box);
        },
        false);
    parser.setCallback(
        "hvc1",
        [](MP4Parser::Box& box, void* ref) {
          static_cast<DemuxerMP4*>(ref)->onHevc(box);
        },
        false);
    parser.setCallback(
        "avcC",
        [](MP4Parser::Box& box, void* ref) {
          static_cast<DemuxerMP4*>(ref)->onAvcC(box);
        },
        false);

    parser.setCallback(
        "mdhd",
        [](MP4Parser::Box& box, void* ref) {
          static_cast<DemuxerMP4*>(ref)->onMdhd(box);
        },
        false);
    parser.setCallback(
        "stts",
        [](MP4Parser::Box& box, void* ref) {
          static_cast<DemuxerMP4*>(ref)->onStts(box);
        },
        false);

    parser.setCallback(
        "stsz",
        [](MP4Parser::Box& box, void* ref) {
          static_cast<DemuxerMP4*>(ref)->onStsz(box);
        },
        false);
    parser.setCallback(
        "stsc",
        [](MP4Parser::Box& box, void* ref) {
          static_cast<DemuxerMP4*>(ref)->onStsc(box);
        },
        false);
    parser.setCallback(
        "stco",
        [](MP4Parser::Box& box, void* ref) {
          static_cast<DemuxerMP4*>(ref)->onStco(box, false);
        },
        false);
    parser.setCallback(
        "co64",
        [](MP4Parser::Box& box, void* ref) {
          static_cast<DemuxerMP4*>(ref)->onStco(box, true);
        },
        false);

    parser.setCallback(
        "mdat",
        [](MP4Parser::Box& box, void* ref) {
          static_cast<DemuxerMP4*>(ref)->onMdat(box);
        },
        false);
  }

  // ---- moov / track parsing ----

  void onTrak() {
    Track* t = new Track();
    tracks.push_back(t);
    current_track = t;
  }

  void onHdlr(MP4Parser::Box& box) {
    if (current_track == nullptr) return;
    beginBoxAccum(box);
    box_accum.writeArray(box.data, box.available);
    if (!box.is_complete || box_accum.available() < 12) return;
    const uint8_t* handler = box_accum.data() + 8;
    if (memcmp(handler, "vide", 4) == 0) {
      current_track->kind = Track::Kind::Video;
      if (p_video_track == nullptr) p_video_track = current_track;
    } else if (memcmp(handler, "soun", 4) == 0) {
      current_track->kind = Track::Kind::Audio;
      if (p_audio_track == nullptr) p_audio_track = current_track;
    }
    endBoxAccum();
  }

  void onStsd(MP4Parser::Box& box) {
    beginBoxAccum(box);
    box_accum.writeArray(box.data, box.available);
    if (box.is_complete && box_accum.available() >= 8) {
      // sample entries (mp4a/alac/avc1/hev1/...) parsed generically, same
      // mechanism as M4ACommonDemuxer::onStsd
      parser.parseString(box_accum.data() + 8, box_accum.available() - 8,
                         box.file_offset + 8 + 8, box.level + 1);
      endBoxAccum();
    }
  }

  void onMp4a(const MP4Parser::Box& box) {
    if (current_track == nullptr || !box.is_complete) return;
    current_track->aacProfile = 2;     // default AAC LC
    current_track->sampleRateIdx = 4;  // default 44100 Hz
    current_track->channelCfg = 2;     // default stereo
    current_track->audio_codec = Codec::AAC;
    setupAudioInfo(AudioFormat::AAC, box.data, box.data_size);
    // AudioSampleEntry fixed header is 28 bytes (incl. the 8 bytes already
    // stripped as the box header) - esds (child box) follows
    int pos = 36 - 8;
    if (box.data_size > (size_t)pos)
      parser.parseString(box.data + pos, box.data_size - pos,
                         box.file_offset + 8 + pos, box.level + 1);
  }

  /// Populates the shared AudioInfo (channels/sample_rate/format/mime) from
  /// an AudioSampleEntry's fixed 28-byte header (ISO/IEC 14496-12 12.2.3):
  /// channelcount at rel-offset 16-17, samplerate (16.16 fixed point,
  /// upper 16 bits = Hz) at rel-offset 24-27 - the same layout for any
  /// audio sample entry (mp4a, alac, ...), so this is codec-agnostic.
  void setupAudioInfo(AudioFormat format, const uint8_t* entryData,
                      size_t entrySize) {
    if (entrySize < 28) return;
    info.channels = readU16(entryData + 16);
    info.sample_rate = readU16(entryData + 24);
    info.bits_per_sample = 16;
    audio_format = format;
    info.logInfo();
    if (!send_wav_header_explicit) send_wav_header = isWavFormat(audio_format);
    if (send_wav_header) sendWavHeader();
    notifyAudioChange(info);
  }
  
  /// Overrides the automatic decision of whether a synthetic WAV header is
  /// sent to the audio output before any audio payload. By default this is
  /// decided automatically from the parsed codec: on whenever
  /// isWavFormat() of the parsed format is true (raw PCM), off otherwise
  /// (AAC/ALAC, which have their own decoder and don't expect a WAV
  /// header). Call this to force it either way instead.
  void setSendWavHeader(bool flag) override {
    send_wav_header = flag;
    send_wav_header_explicit = true;
  }

  /// Synthesizes a valid WAV header (via the shared WAVHeader writer) from
  /// the parsed AudioInfo and sends it to p_output_audio, once, before any
  /// real audio payload - so a WAVDecoder-based output can bootstrap itself
  /// from it. Length is written as streamed/unknown, since the track's
  /// length is not known upfront while demuxing. Only ever relevant for a
  /// raw-PCM ('lpcm') audio track - not currently produced by this class
  /// (AAC/ALAC are the only codecs parsed), but kept symmetric with
  /// DemuxerAVI for when PCM tracks are added.
  void sendWavHeader() {
    if (is_wav_header_sent || p_output_audio == nullptr) return;
    WAVAudioInfo winfo(info);
    winfo.format = audio_format;
    winfo.is_streamed = true;
    winfo.block_align = info.channels * (info.bits_per_sample / 8);
    winfo.byte_rate = info.sample_rate * winfo.block_align;
    WAVHeader wav_header;
    wav_header.setAudioInfo(winfo);
    wav_header.writeHeader(p_output_audio);
    is_wav_header_sent = true;
  }

  void onEsds(const MP4Parser::Box& box) {
    if (current_track == nullptr) return;
    M4ACommonDemuxer::ESDSParser esdsParser;
    if (!esdsParser.parse(box.data, box.data_size)) {
      LOGE("Failed to parse esds box");
      return;
    }
    current_track->aacProfile = esdsParser.audioObjectType;
    current_track->sampleRateIdx = esdsParser.samplingRateIndex;
    current_track->channelCfg = esdsParser.channelConfiguration;
  }

  void onAlac(const MP4Parser::Box& box) {
    if (current_track == nullptr || !box.is_complete) return;
    current_track->audio_codec = Codec::ALAC;
    setupAudioInfo(AudioFormat::ALAC, box.data, box.data_size);
    MP4Parser::Box alac;
    if (parser.findBox("alac", box.data, box.data_size, alac)) {
      current_track->alacMagicCookie.resize(alac.data_size - 4);
      memcpy(current_track->alacMagicCookie.data(), alac.data + 4,
             alac.data_size - 4);
    }
  }

  void onAvc1(const MP4Parser::Box& box) {
    if (current_track == nullptr || !box.is_complete) return;
    current_track->kind = Track::Kind::Video;
    if (p_video_track == nullptr) p_video_track = current_track;
    if (box.data_size < 28) return;
    current_track->width = readU16(box.data + 24);
    current_track->height = readU16(box.data + 26);
    // VisualSampleEntry fixed header is 78 bytes (already excl. the 8 byte
    // box header) - avcC (child box) follows
    const int pos = 78;
    if (box.data_size > (size_t)pos)
      parser.parseString(box.data + pos, box.data_size - pos,
                         box.file_offset + 8 + pos, box.level + 1);
  }

  void onHevc(const MP4Parser::Box& box) {
    if (current_track == nullptr) return;
    current_track->kind = Track::Kind::Video;
    if (p_video_track == nullptr) p_video_track = current_track;
    if (!current_track->is_hevc_unsupported) {
      LOGE("HEVC (hev1/hvc1) video is not supported - only H.264 (avc1)");
      current_track->is_hevc_unsupported = true;
    }
  }

  void onAvcC(const MP4Parser::Box& box) {
    if (current_track == nullptr || !box.is_complete || box.data_size < 6)
      return;
    const uint8_t* d = box.data;
    current_track->nal_length_size = (d[4] & 0x03) + 1;
    int numSps = d[5] & 0x1F;
    size_t pos = 6;
    auto appendUnit = [&](const uint8_t* unit, size_t len) {
      static const uint8_t startCode[4] = {0, 0, 0, 1};
      auto& v = current_track->sps_pps_annexb;
      size_t off = v.size();
      v.resize(off + 4 + len);
      memcpy(v.data() + off, startCode, 4);
      memcpy(v.data() + off + 4, unit, len);
    };
    for (int i = 0; i < numSps && pos + 2 <= box.data_size; i++) {
      uint16_t len = readU16(d + pos);
      pos += 2;
      if (pos + len > box.data_size) break;
      appendUnit(d + pos, len);
      pos += len;
    }
    if (pos >= box.data_size) return;
    int numPps = d[pos++];
    for (int i = 0; i < numPps && pos + 2 <= box.data_size; i++) {
      uint16_t len = readU16(d + pos);
      pos += 2;
      if (pos + len > box.data_size) break;
      appendUnit(d + pos, len);
      pos += len;
    }
  }

  /// 'mdhd' (in 'mdia', scoped by the enclosing 'trak'): captures this
  /// track's own timescale (ticks/second), needed together with 'stts' to
  /// compute each sample's scheduled presentation time.
  void onMdhd(MP4Parser::Box& box) {
    if (current_track == nullptr) return;
    beginBoxAccum(box);
    box_accum.writeArray(box.data, box.available);
    if (!box.is_complete) return;
    if (box_accum.available() < 4) {
      endBoxAccum();
      return;
    }
    uint8_t version = box_accum.data()[0];
    // v0: creation(4)+modification(4)+timescale(4)+duration(4)
    // v1: creation(8)+modification(8)+timescale(4)+duration(8)
    size_t timescaleOffset = version == 1 ? 20 : 12;
    if (box_accum.available() >= timescaleOffset + 4) {
      current_track->timescale = readU32(box_accum.data() + timescaleOffset);
    }
    endBoxAccum();
  }

  /// 'stts' (time-to-sample): run-length list of {sample_count,
  /// sample_delta} - see Track::nextPtsTicks().
  void onStts(MP4Parser::Box& box) {
    if (current_track == nullptr) return;
    beginBoxAccum(box);
    box_accum.writeArray(box.data, box.available);
    if (!box.is_complete) return;
    if (box_accum.available() < 8) {
      endBoxAccum();
      return;
    }
    uint32_t entryCount = readU32(box_accum.data() + 4);
    const uint8_t* p = box_accum.data() + 8;
    size_t avail = box_accum.available() - 8;
    for (uint32_t i = 0; i < entryCount && (i + 1) * 8 <= avail; i++) {
      SttsEntry e;
      e.sample_count = readU32(p + i * 8);
      e.sample_delta = readU32(p + i * 8 + 4);
      current_track->stts.push_back(e);
    }
    endBoxAccum();
  }

  void onStsz(MP4Parser::Box& box) {
    if (current_track == nullptr) return;
    if (beginBoxAccum(box))
      stsz_header_pending = true;  // starting a fresh stsz box
    box_accum.writeArray(box.data, box.available);

    // The 12-byte header (version+flags, sampleSize, sampleCount) must be
    // fully consumed *once* before any bytes are interpreted as per-sample
    // sizes. Using "no sizes pushed yet" as a stand-in for "header not
    // parsed yet" (as tried previously) is wrong: in the variable-size
    // case, both are simultaneously true right after the header IS parsed,
    // so under fine-grained incremental delivery the per-sample loop below
    // could start consuming still-unparsed header bytes as if they were
    // sample sizes. An explicit flag avoids that ambiguity.
    if (stsz_header_pending) {
      if (box_accum.available() < 12)
        return;  // wait for the rest of the header
      Track* t = current_track;
      uint32_t sampleSize = readU32(box_accum.data() + 4);
      uint32_t sampleCount = readU32(box_accum.data() + 8);
      box_accum.clearArray(12);
      stsz_header_pending = false;
      if (sampleSize != 0) {
        t->fixed_sample_size = sampleSize;
        t->fixed_sample_count = sampleCount;
      }
      // else: sample_sizes is filled below via push_back, one entry at a
      // time as bytes arrive - do NOT pre-resize it (Vector::push_back
      // always appends, unlike BaseBuffer::write() which fills pre-sized
      // capacity in place)
    }
    // incrementally consume per-sample sizes (fixed-size case has none left
    // to read; the table is only present in the box when sampleSize==0)
    if (current_track->fixed_sample_size == 0) {
      while (box_accum.available() >= 4) {
        current_track->sample_sizes.push_back(readU32(box_accum.data()));
        box_accum.clearArray(4);
      }
    }
    if (box.is_complete) endBoxAccum();
  }

  void onStsc(MP4Parser::Box& box) {
    if (current_track == nullptr) return;
    beginBoxAccum(box);
    box_accum.writeArray(box.data, box.available);
    if (!box.is_complete) return;
    if (box_accum.available() < 8) {
      endBoxAccum();
      return;
    }
    uint32_t entryCount = readU32(box_accum.data() + 4);
    const uint8_t* p = box_accum.data() + 8;
    size_t avail = box_accum.available() - 8;
    for (uint32_t i = 0; i < entryCount && (i + 1) * 12 <= avail; i++) {
      StscEntry e;
      e.first_chunk = readU32(p + i * 12);
      e.samples_per_chunk = readU32(p + i * 12 + 4);
      current_track->stsc.push_back(e);
    }
    endBoxAccum();
  }

  void onStco(MP4Parser::Box& box, bool is64) {
    if (current_track == nullptr) return;
    beginBoxAccum(box);
    box_accum.writeArray(box.data, box.available);
    if (!box.is_complete) return;
    if (box_accum.available() < 8) {
      endBoxAccum();
      return;
    }
    uint32_t entryCount = readU32(box_accum.data() + 4);
    const uint8_t* p = box_accum.data() + 8;
    size_t avail = box_accum.available() - 8;
    size_t entrySize = is64 ? 8 : 4;
    for (uint32_t i = 0; i < entryCount && (i + 1) * entrySize <= avail; i++) {
      uint64_t off = is64 ? readU64(p + i * 8) : readU32(p + i * 4);
      current_track->chunk_offsets.push_back(off);
    }
    endBoxAccum();
  }

  // ---- mdat / schedule / sample dispatch ----

  void onMdat(MP4Parser::Box& box) {
    if (!mdat_seen) {
      mdat_seen = true;
      buildSchedule();
    }
    feed(box.data, box.available);
  }

  /// Merge all tracks' (chunk_offset, samples_in_chunk) entries, sorted
  /// ascending by chunk_offset, into a single consumption order.
  void buildSchedule() {
    // (track*, chunk_index, offset) working list
    struct Item {
      Track* track;
      uint32_t chunk_index;
      uint64_t offset;
    };
    Vector<Item> items;
    for (size_t ti = 0; ti < tracks.size(); ti++) {
      Track* t = tracks[ti];
      if (t->kind == Track::Kind::Unknown) continue;
      if (t->kind == Track::Kind::Video && t != p_video_track) continue;
      if (t->kind == Track::Kind::Audio && t != p_audio_track) continue;
      for (size_t ci = 0; ci < t->chunk_offsets.size(); ci++) {
        items.push_back({t, (uint32_t)ci, t->chunk_offsets[ci]});
      }
    }
    // simple insertion sort by offset (small N: chunk count is typically
    // well under a few thousand for embedded-relevant clip lengths)
    for (size_t i = 1; i < items.size(); i++) {
      Item key = items[i];
      long j = (long)i - 1;
      while (j >= 0 && items[j].offset > key.offset) {
        items[j + 1] = items[j];
        j--;
      }
      items[j + 1] = key;
    }
    for (size_t i = 0; i < items.size(); i++) {
      Track* t = items[i].track;
      uint32_t samples = samplesInChunk(*t, items[i].chunk_index);
      if (samples > 0) schedule.push_back({t, samples});
    }
    schedule_index = 0;
    samples_left_in_entry = schedule.size() > 0 ? schedule[0].sample_count : 0;
  }

  /// Number of samples in the given (0-based) chunk index, per 'stsc'
  uint32_t samplesInChunk(Track& t, uint32_t chunkIndex0) {
    uint32_t chunk1 = chunkIndex0 + 1;  // stsc uses 1-based chunk numbers
    uint32_t result = 0;
    for (size_t i = 0; i < t.stsc.size(); i++) {
      if (t.stsc[i].first_chunk <= chunk1) {
        result = t.stsc[i].samples_per_chunk;
      } else {
        break;
      }
    }
    return result;
  }

  /// Advances to the next schedule entry once the current one is exhausted
  void advanceScheduleIfNeeded() {
    while (samples_left_in_entry == 0 && schedule_index < schedule.size()) {
      schedule_index++;
      samples_left_in_entry = schedule_index < schedule.size()
                                  ? schedule[schedule_index].sample_count
                                  : 0;
    }
  }

  /// Feeds raw mdat bytes; accumulates until the current sample is
  /// complete, dispatches it, and moves on - possibly switching tracks at
  /// chunk boundaries per the merge schedule.
  void feed(const uint8_t* data, size_t len) {
    size_t pos = 0;
    while (pos < len) {
      if (current_sample_track == nullptr) {
        advanceScheduleIfNeeded();
        if (schedule_index >= schedule.size()) return;  // nothing more expected
        Track* t = schedule[schedule_index].track;
        uint32_t size = t->sampleSize(t->next_sample_index);
        if (size == 0) {
          // no more sizes for this track - stop
          samples_left_in_entry = 0;
          continue;
        }
        current_sample_track = t;
        current_sample_size = size;
        current_sample_is_first_for_track = (t->next_sample_index == 0);
        // Vector::resize() sets the logical size immediately (unlike a
        // capacity reserve), so track how much of it is actually filled
        // separately rather than relying on sample_buffer.size().
        sample_buffer.resize(size);
        sample_buffer_filled = 0;
      }

      size_t need = current_sample_size - sample_buffer_filled;
      size_t take = std::min(need, len - pos);
      memcpy(sample_buffer.data() + sample_buffer_filled, data + pos, take);
      sample_buffer_filled += take;
      pos += take;

      if (sample_buffer_filled >= current_sample_size) {
        dispatchSample(*current_sample_track, sample_buffer.data(),
                       sample_buffer_filled, current_sample_is_first_for_track);
        current_sample_track->next_sample_index++;
        samples_left_in_entry--;
        current_sample_track = nullptr;
      }
    }
  }

  void dispatchSample(Track& t, const uint8_t* data, size_t size,
                      bool isFirst) {
    if (&t == p_video_track) {
      dispatchVideo(t, data, size, isFirst);
    } else if (&t == p_audio_track) {
      dispatchAudio(t, data, size, isFirst);
    }
  }

  void dispatchVideo(Track& t, const uint8_t* data, size_t size, bool isFirst) {
    // nextPtsTicks() must run unconditionally, in sample order, to keep
    // the track's timing cursor correct regardless of what we do below -
    // call it before any early return.
    uint64_t ptsTicks = t.nextPtsTicks();
    if (p_video_out == nullptr || t.is_hevc_unsupported) return;

    if (t.timescale > 0 && !t.stts.empty()) {
      uint32_t scheduledMs = (uint32_t)((ptsTicks * 1000) / t.timescale);
      uint32_t nowMs = millis();
      if (!playback_start_set) {
        playback_start_ms = nowMs;
        playback_start_set = true;
      }
      uint32_t scheduledWallMs = playback_start_ms + scheduledMs;
      if (nowMs > scheduledWallMs) {
        LOGW("DemuxerMP4: video frame %u ms late - skipping",
             (unsigned)(nowMs - scheduledWallMs));
        return;
      } else if (nowMs < scheduledWallMs) {
        LOGI("DemuxerMP4: video frame %u ms early",
             (unsigned)(scheduledWallMs - nowMs));
      }
    }

    // convert AVCC length-prefixed NAL units to Annex-B start codes
    nal_tmp.clear();
    size_t pos = 0;
    size_t lenSize = t.nal_length_size;
    while (pos + lenSize <= size) {
      uint32_t nalLen = 0;
      for (size_t i = 0; i < lenSize; i++)
        nalLen = (nalLen << 8) | data[pos + i];
      pos += lenSize;
      if (pos + nalLen > size) break;
      size_t off = nal_tmp.size();
      static const uint8_t startCode[4] = {0, 0, 0, 1};
      nal_tmp.resize(off + 4 + nalLen);
      memcpy(nal_tmp.data() + off, startCode, 4);
      memcpy(nal_tmp.data() + off + 4, data + pos, nalLen);
      pos += nalLen;
    }

    bool prependConfig =
        isFirst && !t.avc_config_sent && t.sps_pps_annexb.size() > 0;
    if (prependConfig) {
      p_video_out->write(t.sps_pps_annexb.data(), t.sps_pps_annexb.size());
      t.avc_config_sent = true;
    }
    if (nal_tmp.size() > 0) p_video_out->write(nal_tmp.data(), nal_tmp.size());
    p_video_out->flush();
  }

  void dispatchAudio(Track& t, const uint8_t* data, size_t size, bool isFirst) {
    if (p_output_audio == nullptr) return;
    if (t.audio_codec == Codec::AAC) {
      uint8_t adts[7];
      writeAdtsHeader(adts, t.aacProfile, t.sampleRateIdx, t.channelCfg,
                      (int)size);
      p_output_audio->write(adts, sizeof(adts));
      p_output_audio->write(data, size);
    } else {
      // ALAC (magic cookie is exposed via audioALACMagicCookie() for the
      // caller to configure their own decoder with) and any other codec:
      // raw payload as-is.
      p_output_audio->write(data, size);
    }
  }

  static void writeAdtsHeader(uint8_t* adts, int aacProfile, int sampleRateIdx,
                              int channelCfg, int frameLen) {
    adts[0] = 0xFF;
    adts[1] = 0xF1;
    adts[2] = ((aacProfile - 1) << 6) | (sampleRateIdx << 2) |
              ((channelCfg >> 2) & 0x1);
    adts[3] = ((channelCfg & 0x3) << 6) | ((frameLen + 7) >> 11);
    adts[4] = ((frameLen + 7) >> 3) & 0xFF;
    adts[5] = (((frameLen + 7) & 0x7) << 5) | 0x1F;
    adts[6] = 0xFC;
  }
};

/// @brief Minimal ISO-BMFF box builder: appends bytes to an in-memory
/// buffer, with beginBox()/endBox() taking care of the 4-byte size prefix
/// (backpatched once the box's content is known) - used internally by
/// MuxerMP4 to assemble 'moov' once and each 'moof' per fragment.
class MP4BoxWriter {
 public:
  Vector<uint8_t> buffer;

  void clear() { buffer.clear(); }
  size_t size() { return buffer.size(); }
  const uint8_t* data() { return buffer.data(); }

  void u8(uint8_t v) { buffer.push_back(v); }
  void u16(uint16_t v) {
    u8((uint8_t)(v >> 8));
    u8((uint8_t)v);
  }
  void u24(uint32_t v) {
    u8((uint8_t)(v >> 16));
    u8((uint8_t)(v >> 8));
    u8((uint8_t)v);
  }
  void u32(uint32_t v) {
    u8((uint8_t)(v >> 24));
    u8((uint8_t)(v >> 16));
    u8((uint8_t)(v >> 8));
    u8((uint8_t)v);
  }
  void fourcc(const char* type) { bytes((const uint8_t*)type, 4); }
  void bytes(const uint8_t* data, size_t len) {
    size_t off = buffer.size();
    buffer.resize(off + len);
    memcpy(buffer.data() + off, data, len);
  }
  void zeros(size_t len) {
    size_t off = buffer.size();
    buffer.resize(off + len);
    memset(buffer.data() + off, 0, len);
  }
  void cstr(const char* s) {
    while (*s) u8((uint8_t)*s++);
    u8(0);
  }

  /// Writes the 4-byte size placeholder + fourcc, returns the box's start
  /// position (pass to endBox() once its content has been written).
  size_t beginBox(const char* type) {
    size_t pos = buffer.size();
    u32(0);
    fourcc(type);
    return pos;
  }
  /// Backpatches the size placeholder from beginBox() with the box's
  /// actual total size (header + content), now that it's known.
  void endBox(size_t pos) {
    uint32_t sz = (uint32_t)(buffer.size() - pos);
    buffer[pos] = (uint8_t)(sz >> 24);
    buffer[pos + 1] = (uint8_t)(sz >> 16);
    buffer[pos + 2] = (uint8_t)(sz >> 8);
    buffer[pos + 3] = (uint8_t)sz;
  }
};

/// @brief Video track configuration for MuxerMP4 - update before calling
/// begin().

/**
 * @brief Fragmented MP4 (fMP4) container encoder: muxes an already-encoded
 * H.264 video stream (Annex-B access units, converted internally to AVCC
 * for the 'mdat' samples) and an optional raw AAC audio stream into a
 * fragmented MP4 stream written to a Print (a local File to record, or
 * e.g. a network Client to publish a live stream to an HTTP/TCP client).
 *
 * Unlike a classic single-'moov' MP4 (which needs the complete stsz/stco
 * sample tables known upfront, and thus either a seekable output to place
 * 'moov' before 'mdat', or buffering the entire recording in memory),
 * fragmented MP4 writes 'ftyp'+'moov' once (with empty sample tables plus
 * an 'mvex' box that tells the player to expect fragments) and then a
 * self-contained 'moof'+'mdat' pair per frame - no seeking required, the
 * same Print-only, forward-only design DemuxerMP4 already assumes on the
 * read side and MuxerAVI uses for AVI.
 *
 * Usage:
 * @code
 * MuxerMP4 mux(client); // any Print: File, WiFiClient, ...
 * MuxerVideoConfig cfg;
 * cfg.width = 640;
 * cfg.height = 480;
 * cfg.fps = 25;
 * cfg.format = VideoFormat::H264;  // or MJPEG/YUV422/RGB565/I420
 * mux.setVideoInfo(cfg);
 * mux.setAudioInfo(AudioInfoFormat(44100, 1, 16));  // optional; AAC by
 *                                                    // default, or pass
 *                                                    // AudioFormat::PCM as
 *                                                    // the 4th argument
 * mux.begin();
 * // for each encoded H.264 access unit (Annex-B) - SPS/PPS are captured
 * // automatically, no separate setup call needed:
 * mux.addVideoFrame(h264_data, h264_len);
 * // for each raw (ADTS-less) AAC frame:
 * mux.addAudioFrame(aac_data, aac_len);
 *
 * // Alternatively, drive both tracks through the single write() call
 * // (e.g. from generic StreamCopy-style code, mirroring MuxerAVI):
 * mux.setStreamType(StreamContentType::Video);
 * mux.write(h264_data, h264_len);
 * mux.setStreamType(StreamContentType::Audio);
 * mux.write(aac_data, aac_len);  // audio: each write() is one full frame
 * @endcode
 *
 * @note v1 scope: one video track, either VideoFormat::H264 (default -
 * matching the only video codec DemuxerMP4 reads back; write via
 * addVideoFrame(), Annex-B access units) or one of the same raw/MJPEG
 * formats MuxerAVI supports (DemuxerMP4 cannot read any of these back -
 * muxer-only for now): VideoFormat::MJPEG (addJpegFrame(), one complete
 * JPEG image per frame; 'mp4v'/'esds' sample entry, objectTypeIndication
 * 0x6C), VideoFormat::YUV422 (addYUV422Frame(), packed YUY2/YUYV; 'yuvs'
 * sample entry), VideoFormat::RGB565 (addRGB565Frame(), 16-bit 5-6-5;
 * 'L565' sample entry), or VideoFormat::I420 (addI420Frame(), planar
 * 4:2:0; 'I420' sample entry). All five formats were verified to produce
 * a file ffprobe/ffmpeg parses and decodes cleanly (probe_score 100) -
 * see the individual addXxxFrame() methods for exactly what was checked
 * against real ffmpeg-generated reference data. Plus one optional audio
 * track, either AAC (the default - raw, ADTS-less
 * frames, matching the 'mdat' sample convention MP4 itself uses, not the
 * ADTS-wrapped convention DemuxerMP4 hands to its own audio output; config
 * limited to the common 2-byte AudioSpecificConfig case, LC profile,
 * standard sample rates) or raw PCM (setAudioInfo() with format =
 * AudioFormat::PCM - little-endian signed samples, 'sowt' sample entry, no
 * encoder needed; only 16-bit has been validated). No mfra index / seeking
 * metadata is written (not required for playback). SPS/PPS (needed for H264's
 * 'avcC') are captured automatically from the first video frame(s) that carry
 * them - typical H.264 encoders prepend them to (at least) the first access
 * unit, so no separate setup call is needed; frames arriving before SPS/PPS
 * have been seen are dropped (logged), as is any audio arriving before that
 * same point, since 'moov'
 * - which describes both tracks - cannot be written until then. MJPEG
 * needs no such stream-derived config, so for it 'moov' is already
 * written by the time begin() returns.
 * @ingroup codecs
 * @ingroup encoder
 * @ingroup video
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class MuxerMP4 : public Muxer {
 public:
  MuxerMP4() { audio_info.format = AudioFormat::AAC; }
  MuxerMP4(Print& out) : MuxerMP4() { setOutput(out); }

  const char* mime() override { return "video/mp4"; }

  /// Defines the output: e.g. a local File or a network Client
  void setOutput(Print& out) override { p_out = &out; }

  /// Defines the video track configuration - call before begin()
  void setVideoInfo(MuxerVideoConfig config) override { video_cfg = config; }

  /// Provides the video track configuration
  MuxerVideoConfig getVideoInfo() override { return video_cfg; }

  /// Adds an (optional) interleaved audio track. 'info.format' selects the
  /// audio track's codec: AudioFormat::AAC (the default if left unset)
  /// writes an 'mp4a'/'esds' sample entry and expects raw (ADTS-less) AAC
  /// frames via addAudioFrame(); AudioFormat::PCM writes a 'sowt'
  /// (little-endian signed PCM) sample entry instead and expects raw
  /// interleaved PCM sample data - simplest option since it needs no
  /// encoder at all, but only 16-bit signed PCM has been validated. Call
  /// before begin().
  void setAudioInfo(AudioInfoFormat info) override {
    if (info.format == AudioFormat::UNKNOWN) info.format = audio_info.format;
    audio_info = info;
    has_audio = true;
  }
  /// Provides read/write access to the audio track's AudioInfoFormat
  AudioInfoFormat& audioInfo() override { return audio_info; }
  /// Defines the AAC profile (audio object type, default 2 = AAC LC) used
  /// to build the 'esds' AudioSpecificConfig - call before begin(). Not
  /// used for AudioFormat::PCM.
  void setAudioProfile(int aacProfile) { audio_profile = aacProfile; }

  /// Prepares the encoder. 'ftyp'+'moov' (with an 'mvex' box signalling
  /// fragments will follow) is not written yet at this point - SPS/PPS
  /// (needed for 'avcC', part of 'moov') are only known once captured from
  /// the video stream itself, so it is written lazily, right before the
  /// first fragment that can actually be produced (see addVideoFrame()).
  /// Call after configuring video (and audio, if any) and before writing
  /// any frames.
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
    if (video_cfg.format != VideoFormat::H264 &&
        video_cfg.format != VideoFormat::MJPEG &&
        video_cfg.format != VideoFormat::YUV422 &&
        video_cfg.format != VideoFormat::RGB565 &&
        video_cfg.format != VideoFormat::I420) {
      LOGE(
          "unsupported video format: %d - MuxerMP4 writes H264, MJPEG, "
          "YUV422, RGB565 or I420",
          (int)video_cfg.format);
      return false;
    }
    video_timescale = 90000;
    video_sample_duration =
        video_cfg.fps > 0 ? (uint32_t)(video_timescale / video_cfg.fps) : 0;
    audio_timescale = has_audio ? audio_info.sample_rate : 0;
    // AAC: fixed 1024 samples/frame (LC). PCM: duration varies per call
    // (addAudioFrame() computes it from the actual byte length instead),
    // so this default is not used for PCM - trun always states it
    // explicitly per fragment regardless of codec.
    audio_sample_duration = (audio_info.format == AudioFormat::PCM) ? 0 : 1024;

    sps_data.clear();
    pps_data.clear();
    moov_written = false;
    video_seq = 0;
    video_base_time = 0;
    audio_seq = 0;
    audio_base_time = 0;
    fragment_seq = 0;
    video_frame_count = 0;
    audio_frame_count = 0;
    is_open = true;
    // MJPEG needs no stream-derived config (unlike H264's SPS/PPS), so
    // this writes 'moov' immediately; for H264 it's a no-op here and
    // happens lazily once SPS/PPS have been captured (see addVideoFrame()).
    tryWriteMoov();
    return true;
  }

  /// Closes the encoder: no trailer is required for playback
  void end() override { is_open = false; }

  operator bool() override { return is_open; }

  /// Selects whether write() feeds the video or the audio track, mirroring
  /// MuxerAVI's setStreamType(). Defaults to StreamContentType::Video;
  /// switch to StreamContentType::Audio (and back) around calls when using
  /// MuxerMP4 as a plain sink for both.
  void setStreamType(StreamContentType type) override {
    write_stream_type = type;
  }
  /// The track write() currently targets (see setStreamType())
  StreamContentType streamType() override { return write_stream_type; }

  /// VideoOutput API / generic sink: writes one complete frame to
  /// whichever track streamType() currently selects (see
  /// setStreamType()). Each call is dispatched immediately - the caller
  /// must hand over the complete frame in a single call (video: to the
  /// addXxxFrame() matching getVideoInfo().format; audio: to
  /// addAudioFrame()). flush() is a no-op; there is no accumulation
  /// across write() calls.
  size_t write(const uint8_t* data, size_t len) override {
    if (write_stream_type == StreamContentType::Audio) {
      return addAudioFrame(data, len);
    }

    switch (video_cfg.format) {
      case VideoFormat::MJPEG:
        return addJpegFrame(data, len);
      case VideoFormat::YUV422:
        return addYUV422Frame(data, len);
      case VideoFormat::RGB565:
        return addRGB565Frame(data, len);
      case VideoFormat::I420:
        return addI420Frame(data, len);
      default:
        return addVideoFrame(data, len);
    }
  }

  /// Writes one complete H.264 access unit (Annex-B: SPS/PPS NALs, if
  /// present, are captured for 'avcC' - see setVideoConfigData() - and
  /// then stripped, since they end up described there instead) as a
  /// single 'moof'+'mdat' fragment. Until SPS/PPS have been seen (from
  /// this or an earlier call), 'moov' cannot be written yet, so the frame
  /// is dropped (logged) rather than buffered. No-op (returns 0) unless
  /// getVideoInfo().format is VideoFormat::H264 - use addJpegFrame() for
  /// VideoFormat::MJPEG instead.
  /// @param isKeyFrame marks the sample as a sync sample (IDR) in the
  /// fragment's 'trun' flags - matters for players seeking into the
  /// stream; pass false for P/B (non-IDR) frames if known.
  size_t addVideoFrame(const uint8_t* data, size_t len,
                       bool isKeyFrame = true) override {
    if (!is_open || video_cfg.format != VideoFormat::H264) return 0;
    setVideoConfigData(data, len);
    if (!tryWriteMoov()) {
      LOGW("dropping video frame: SPS/PPS not seen yet");
      return 0;
    }
    nal_tmp.clear();
    forEachAnnexBNal(data, len, [this](const uint8_t* nal, size_t nalLen) {
      if (nalLen == 0) return;
      uint8_t nalType = nal[0] & 0x1F;
      if (nalType == 7 || nalType == 8) return;  // SPS/PPS: already in avcC
      size_t off = nal_tmp.size();
      nal_tmp.resize(off + 4 + nalLen);
      uint8_t* p = nal_tmp.data() + off;
      p[0] = (uint8_t)(nalLen >> 24);
      p[1] = (uint8_t)(nalLen >> 16);
      p[2] = (uint8_t)(nalLen >> 8);
      p[3] = (uint8_t)nalLen;
      memcpy(p + 4, nal, nalLen);
    });
    if (nal_tmp.size() == 0) return 0;
    writeMoofMdat(kVideoTrackId, nal_tmp.data(), nal_tmp.size(),
                  video_sample_duration, isKeyFrame, video_base_time);
    video_base_time += video_sample_duration;
    video_frame_count++;
    return len;
  }

  /// Writes one complete Motion-JPEG frame (a full, already-encoded JPEG
  /// image, e.g. as produced by an ESP32-CAM or other hardware JPEG
  /// encoder) as a single 'moof'+'mdat' fragment - no-op (returns 0)
  /// unless getVideoInfo().format is VideoFormat::MJPEG.
  size_t addJpegFrame(const uint8_t* data, size_t len) override {
    if (video_cfg.format != VideoFormat::MJPEG) return 0;
    return addRawFrame(data, len);
  }

  /// Writes one packed 4:2:2 YUV frame (YUY2/YUYV byte order) as a single
  /// 'moof'+'mdat' fragment. Expects exactly width*height*2 bytes -
  /// mismatches are logged, not rejected. No-op (returns 0) unless
  /// getVideoInfo().format is VideoFormat::YUV422.
  /// @note Muxer-only (DemuxerMP4 cannot read this back). Its 'yuvs'
  /// sample entry matches what ffmpeg itself writes for YUY2-in-MOV, and
  /// a muxed file round-trips cleanly through ffprobe/ffmpeg (correct
  /// pix_fmt, probe_score 100, decodes without error).
  size_t addYUV422Frame(const uint8_t* data, size_t len) override {
    checkRawFrame(VideoFormat::YUV422, len);
    return addRawFrame(data, len);
  }

  /// Writes one uncompressed RGB565 (16-bit, 5-6-5) frame as a single
  /// 'moof'+'mdat' fragment. Expects exactly width*height*2 bytes -
  /// mismatches are logged, not rejected. No-op (returns 0) unless
  /// getVideoInfo().format is VideoFormat::RGB565.
  /// @note Muxer-only; see addYUV422Frame()'s note - its 'L565' sample
  /// entry matches what ffmpeg itself writes/reads for rgb565le-in-MOV.
  size_t addRGB565Frame(const uint8_t* data, size_t len) override {
    checkRawFrame(VideoFormat::RGB565, len);
    return addRawFrame(data, len);
  }

  /// Writes one planar 4:2:0 YUV (I420/IYUV: full-res Y, then quarter-res
  /// U, then quarter-res V) frame as a single 'moof'+'mdat' fragment.
  /// Expects exactly width*height*3/2 bytes - mismatches are logged, not
  /// rejected. No-op (returns 0) unless getVideoInfo().format is
  /// VideoFormat::I420.
  /// @note Muxer-only; see addYUV422Frame()'s note - its 'I420' sample
  /// entry round-trips correctly through ffprobe/ffmpeg despite ffmpeg's
  /// own rawvideo *encoder* having no direct pix_fmt+tag combination that
  /// produces one (a limitation of that one encoder path, not of the
  /// 'I420' tag itself - a hand-built file with the correct box structure
  /// decodes fine).
  size_t addI420Frame(const uint8_t* data, size_t len) override {
    checkRawFrame(VideoFormat::I420, len);
    return addRawFrame(data, len);
  }

  /// Writes one complete audio frame as a single 'moof'+'mdat' fragment.
  /// For AudioFormat::AAC (default): one raw (ADTS-less) AAC frame - the
  /// AAC config is described once via 'esds', unlike AVI/ADTS where it is
  /// repeated per frame. For AudioFormat::PCM: any number of interleaved
  /// PCM samples (e.g. one read buffer's worth) - the fragment's duration
  /// is derived from len, so callers may pass differently-sized chunks
  /// from call to call. Like addVideoFrame(), frames arriving before
  /// 'moov' can be written (i.e. before the video track's SPS/PPS have
  /// been seen) are dropped (logged).
  size_t addAudioFrame(const uint8_t* data, size_t len) override {
    if (!is_open || !has_audio || len == 0) return 0;
    if (!moov_written) {
      LOGW(
          "dropping audio frame: moov not written yet (video SPS/PPS not "
          "seen)");
      return 0;
    }
    uint32_t duration = audio_sample_duration;  // AAC: fixed 1024
    if (audio_info.format == AudioFormat::PCM) {
      int bytesPerFrame =
          audio_info.channels * (audio_info.bits_per_sample / 8);
      duration = bytesPerFrame > 0 ? (uint32_t)(len / bytesPerFrame) : 0;
    }
    writeMoofMdat(kAudioTrackId, data, len, duration,
                  /*isKeyFrame*/ true, audio_base_time);
    audio_base_time += duration;
    audio_frame_count++;
    return len;
  }

  /// Number of video frames (fragments) written so far
  uint32_t videoFrameCount() { return video_frame_count; }
  /// Number of audio frames (fragments) written so far
  uint32_t audioFrameCount() { return audio_frame_count; }

 protected:
  static const uint32_t kVideoTrackId = 1;
  static const uint32_t kAudioTrackId = 2;

  Print* p_out = nullptr;
  MuxerVideoConfig video_cfg;
  Vector<uint8_t> sps_data;
  Vector<uint8_t> pps_data;

  AudioInfoFormat audio_info;
  bool has_audio = false;
  int audio_profile = 2;  // AAC LC

  bool is_open = false;
  /// True once 'ftyp'+'moov' has actually been written (deferred until
  /// SPS/PPS are known - see tryWriteMoov()).
  bool moov_written = false;
  StreamContentType write_stream_type = StreamContentType::Video;
  uint32_t video_timescale = 90000;
  uint32_t video_sample_duration = 0;
  uint32_t audio_timescale = 0;
  uint32_t audio_sample_duration = 1024;

  uint32_t video_seq = 0;
  uint32_t video_base_time = 0;
  uint32_t audio_seq = 0;
  uint32_t audio_base_time = 0;
  uint32_t fragment_seq = 0;
  uint32_t video_frame_count = 0;
  uint32_t audio_frame_count = 0;

  Vector<uint8_t> nal_tmp;  ///< scratch buffer for Annex-B -> AVCC conversion
  MP4BoxWriter box;  ///< scratch buffer, reused for 'moov' and each 'moof'

  /// Scans an Annex-B video frame for SPS/PPS NALs and captures them (for
  /// the 'avcC' box) if found - called automatically from addVideoFrame()
  /// for every frame, so whichever frame(s) happen to carry them (typically
  /// the first, and/or every keyframe) fill in the config. Does not clear
  /// previously-captured data when a frame contains neither.
  void setVideoConfigData(const uint8_t* spsPpsAnnexB, size_t len) {
    forEachAnnexBNal(spsPpsAnnexB, len,
                     [this](const uint8_t* nal, size_t nalLen) {
                       if (nalLen == 0) return;
                       uint8_t nalType = nal[0] & 0x1F;
                       if (nalType == 7) {
                         sps_data.resize(nalLen);
                         memcpy(sps_data.data(), nal, nalLen);
                       } else if (nalType == 8) {
                         pps_data.resize(nalLen);
                         memcpy(pps_data.data(), nal, nalLen);
                       }
                     });
  }

  /// Writes 'ftyp'+'moov', once SPS/PPS are available for H264 (MJPEG
  /// needs no equivalent stream-derived config, so it's ready immediately)
  /// - a no-op once already written. Returns true if 'moov' has been (now
  /// or previously) written, i.e. fragments may be written.
  bool tryWriteMoov() {
    if (moov_written) return true;
    if (video_cfg.format == VideoFormat::H264 &&
        (sps_data.size() == 0 || pps_data.size() == 0)) {
      return false;
    }
    writeFtypMoov();
    moov_written = true;
    return true;
  }

  void checkVideoFormat(VideoFormat expected) {
    if (video_cfg.format != expected) {
      LOGW("getVideoInfo().format does not match the addXxxFrame() called");
    }
  }

  /// Validates both the configured format and (for fixed-size raw
  /// formats) that len matches the width*height based expectation.
  void checkRawFrame(VideoFormat expected, size_t len) {
    checkVideoFormat(expected);
    size_t expectedSize =
        videoFrameSizeBytes(expected, video_cfg.width, video_cfg.height);
    if (expectedSize > 0 && len != expectedSize) {
      LOGW("frame size %d does not match the expected %d bytes for %d x %d",
           (int)len, (int)expectedSize, (int)video_cfg.width,
           (int)video_cfg.height);
    }
  }

  /// Writes one complete frame's bytes through as-is (no NAL-style framing
  /// needed - used by MJPEG, which is self-delimiting, and the raw pixel
  /// formats YUV422/RGB565/I420) as a single 'moof'+'mdat' fragment. Every
  /// such frame is independently decodable, so it is always marked as a
  /// sync sample. None of these formats need stream-derived config (unlike
  /// H264's SPS/PPS), so 'moov' is already written by the time begin()
  /// returns - tryWriteMoov() here is just a safety net.
  size_t addRawFrame(const uint8_t* data, size_t len) {
    if (!is_open || len == 0) return 0;
    if (!tryWriteMoov()) return 0;
    writeMoofMdat(kVideoTrackId, data, len, video_sample_duration,
                  /*isKeyFrame*/ true, video_base_time);
    video_base_time += video_sample_duration;
    video_frame_count++;
    return len;
  }

  /// Calls callback(nalDataPtr, nalDataLen) for every NAL unit found in an
  /// Annex-B buffer (data spans exclude the 00 00 01 / 00 00 00 01 start
  /// code itself).
  template <typename F>
  static void forEachAnnexBNal(const uint8_t* data, size_t len, F callback) {
    size_t i = 0;
    while (i + 3 <= len) {
      size_t scLen = 0;
      if (i + 4 <= len && data[i] == 0 && data[i + 1] == 0 &&
          data[i + 2] == 0 && data[i + 3] == 1) {
        scLen = 4;
      } else if (data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 1) {
        scLen = 3;
      }
      if (scLen == 0) {
        i++;
        continue;
      }
      size_t nalStart = i + scLen;
      size_t j = nalStart;
      size_t nextStart = len;
      while (j + 3 <= len) {
        if (data[j] == 0 && data[j + 1] == 0 &&
            (data[j + 2] == 1 ||
             (j + 3 < len && data[j + 2] == 0 && data[j + 3] == 1))) {
          nextStart = j;
          break;
        }
        j++;
      }
      if (nextStart > nalStart) callback(data + nalStart, nextStart - nalStart);
      i = nextStart;
    }
  }

  static int aacSampleRateIndex(uint32_t sampleRate) {
    static const uint32_t rates[13] = {96000, 88200, 64000, 48000, 44100,
                                       32000, 24000, 22050, 16000, 12000,
                                       11025, 8000,  7350};
    for (int i = 0; i < 13; i++)
      if (rates[i] == sampleRate) return i;
    return 4;  // default: 44100
  }

  void writeAvcC(MP4BoxWriter& b) {
    size_t pos = b.beginBox("avcC");
    b.u8(1);            // configurationVersion
    b.u8(sps_data[1]);  // AVCProfileIndication
    b.u8(sps_data[2]);  // profile_compatibility
    b.u8(sps_data[3]);  // AVCLevelIndication
    b.u8(0xFF);         // reserved(6) + lengthSizeMinusOne(2) = 4-byte lengths
    b.u8(0xE1);         // reserved(3) + numOfSPS(5) = 1
    b.u16((uint16_t)sps_data.size());
    b.bytes(sps_data.data(), sps_data.size());
    b.u8(1);  // numOfPPS
    b.u16((uint16_t)pps_data.size());
    b.bytes(pps_data.data(), pps_data.size());
    b.endBox(pos);
  }

  /// 'esds' for the 'mp4v' MJPEG sample entry: objectTypeIndication 0x6C
  /// (ISO/IEC 10918-1, i.e. JPEG) with no DecoderSpecificInfo, since each
  /// JPEG sample is fully self-contained (its own quantization/Huffman
  /// tables) - unlike AAC, there is no shared out-of-band config. Layout
  /// verified against what ffmpeg itself writes for MJPEG-in-MP4.
  void writeEsdsMjpeg(MP4BoxWriter& b) {
    size_t pos = b.beginBox("esds");
    b.u32(0);  // version + flags
    b.u8(0x03);
    // size: ES_ID(2)+flags(1)+DecoderConfigDescr(15 incl. its own tag+size)
    // +SLConfigDescr(3 incl. its own tag+size) = 21
    b.u8(21);
    b.u16(kVideoTrackId);  // ES_ID
    b.u8(0);               // flags
    b.u8(0x04);
    b.u8(13);    // size: objType(1)+streamType(1)+bufSize(3)+maxBr(4)+avgBr(4)
    b.u8(0x6C);  // objectTypeIndication: JPEG (ISO/IEC 10918-1)
    b.u8((4 << 2) | 1);  // streamType: visual(4), reserved bit set
    b.u24(0);            // bufferSizeDB
    b.u32(0);            // maxBitrate (0 = unspecified)
    b.u32(0);            // avgBitrate
    b.u8(0x06);
    b.u8(1);
    b.u8(0x02);
    b.endBox(pos);
  }

  void writeEsds(MP4BoxWriter& b) {
    int sampleRateIdx = aacSampleRateIndex(audio_info.sample_rate);
    uint8_t asc[2];
    asc[0] = (uint8_t)((audio_profile << 3) | (sampleRateIdx >> 1));
    asc[1] = (uint8_t)(((sampleRateIdx & 1) << 7) | (audio_info.channels << 3));

    size_t pos = b.beginBox("esds");
    b.u32(0);  // version + flags
    // ES_Descriptor
    b.u8(0x03);
    // size: ES_ID(2)+flags(1)+DecoderConfigDescr(19 incl. its own tag+size)
    // +SLConfigDescr(3 incl. its own tag+size) = 25
    b.u8(25);
    b.u16(kAudioTrackId);  // ES_ID
    b.u8(0);               // flags
    // DecoderConfigDescr
    b.u8(0x04);
    b.u8(
        17);  // size:
              // objType(1)+streamType(1)+bufSize(3)+maxBr(4)+avgBr(4)+DecSpecificInfo(4)
    b.u8(0x40);          // objectTypeIndication: AAC
    b.u8((5 << 2) | 1);  // streamType: audio(5), upstream=0, reserved=1
    b.u24(0);            // bufferSizeDB
    b.u32(128000);       // maxBitrate
    b.u32(128000);       // avgBitrate
    // DecSpecificInfo (the 2-byte AudioSpecificConfig)
    b.u8(0x05);
    b.u8(2);
    b.bytes(asc, 2);
    // SLConfigDescr
    b.u8(0x06);
    b.u8(1);
    b.u8(0x02);
    b.endBox(pos);
  }

  void writeMvhd(MP4BoxWriter& b) {
    size_t pos = b.beginBox("mvhd");
    b.u32(0);           // version + flags
    b.u32(0);           // creation_time
    b.u32(0);           // modification_time
    b.u32(1000);        // timescale
    b.u32(0);           // duration (unknown - fragmented)
    b.u32(0x00010000);  // rate 1.0
    b.u16(0x0100);      // volume 1.0
    b.u16(0);           // reserved
    b.u32(0);
    b.u32(0);  // reserved[2]
    // unity matrix
    static const uint32_t identity[9] = {
        0x00010000, 0, 0, 0, 0x00010000, 0, 0, 0, 0x40000000};
    for (int i = 0; i < 9; i++) b.u32(identity[i]);
    b.zeros(24);                                               // pre_defined[6]
    b.u32(has_audio ? kAudioTrackId + 1 : kVideoTrackId + 1);  // next_track_ID
    b.endBox(pos);
  }

  void writeTkhd(MP4BoxWriter& b, uint32_t trackId, bool isVideo) {
    size_t pos = b.beginBox("tkhd");
    b.u32(0x000007);  // version 0 + flags: enabled|in_movie|in_preview
    b.u32(0);         // creation_time
    b.u32(0);         // modification_time
    b.u32(trackId);
    b.u32(0);  // reserved
    b.u32(0);  // duration (unknown)
    b.u32(0);
    b.u32(0);                     // reserved[2]
    b.u16(0);                     // layer
    b.u16(0);                     // alternate_group
    b.u16(isVideo ? 0 : 0x0100);  // volume
    b.u16(0);                     // reserved
    static const uint32_t identity[9] = {
        0x00010000, 0, 0, 0, 0x00010000, 0, 0, 0, 0x40000000};
    for (int i = 0; i < 9; i++) b.u32(identity[i]);
    b.u32(isVideo ? ((uint32_t)video_cfg.width << 16) : 0);
    b.u32(isVideo ? ((uint32_t)video_cfg.height << 16) : 0);
    b.endBox(pos);
  }

  void writeMdhd(MP4BoxWriter& b, uint32_t timescale) {
    size_t pos = b.beginBox("mdhd");
    b.u32(0);  // version + flags
    b.u32(0);  // creation_time
    b.u32(0);  // modification_time
    b.u32(timescale);
    b.u32(0);       // duration (unknown)
    b.u16(0x55C4);  // language: und
    b.u16(0);
    b.endBox(pos);
  }

  void writeHdlr(MP4BoxWriter& b, const char* handlerType, const char* name) {
    size_t pos = b.beginBox("hdlr");
    b.u32(0);  // version + flags
    b.u32(0);  // pre_defined
    b.fourcc(handlerType);
    b.zeros(12);  // reserved
    b.cstr(name);
    b.endBox(pos);
  }

  void writeDinf(MP4BoxWriter& b) {
    size_t pos = b.beginBox("dinf");
    size_t drefPos = b.beginBox("dref");
    b.u32(0);  // version + flags
    b.u32(1);  // entry_count
    size_t urlPos = b.beginBox("url ");
    b.u32(1);  // version + flags: self-contained (data in this file)
    b.endBox(urlPos);
    b.endBox(drefPos);
    b.endBox(pos);
  }

  void writeVideoStbl(MP4BoxWriter& b) {
    size_t pos = b.beginBox("stbl");
    size_t stsdPos = b.beginBox("stsd");
    b.u32(0);  // version + flags
    b.u32(1);  // entry_count
    // VisualSampleEntry: same fixed 78-byte header for every fourCC below
    const char* fourcc = "avc1";
    uint16_t depth = 0x0018;  // 24, nominal - not critical for playback
    switch (video_cfg.format) {
      case VideoFormat::MJPEG:
        fourcc = "mp4v";
        break;
      case VideoFormat::YUV422:
        fourcc = "yuvs";  // matches what ffmpeg itself writes for YUY2-in-MOV
        break;
      case VideoFormat::RGB565:
        fourcc = "L565";  // matches what ffmpeg itself writes for rgb565le
        depth = 16;       // actual bit depth, unlike the nominal 24 above
        break;
      case VideoFormat::I420:
        fourcc = "I420";  // see addI420Frame()'s note: unverified in MP4
        break;
      default:
        break;  // H264: 'avc1'
    }
    size_t entryPos = b.beginBox(fourcc);
    b.zeros(6);   // reserved
    b.u16(1);     // data_reference_index
    b.u16(0);     // pre_defined
    b.u16(0);     // reserved
    b.zeros(12);  // pre_defined[3]
    b.u16(video_cfg.width);
    b.u16(video_cfg.height);
    b.u32(0x00480000);  // horizresolution 72dpi
    b.u32(0x00480000);  // vertresolution 72dpi
    b.u32(0);           // reserved
    b.u16(1);           // frame_count
    b.zeros(32);        // compressorname
    b.u16(depth);
    b.u16(0xFFFF);  // pre_defined
    switch (video_cfg.format) {
      case VideoFormat::MJPEG:
        writeEsdsMjpeg(b);
        break;
      case VideoFormat::YUV422:
      case VideoFormat::RGB565:
      case VideoFormat::I420:
        break;  // raw formats: no child config box needed
      default:
        writeAvcC(b);  // H264
        break;
    }
    b.endBox(entryPos);
    b.endBox(stsdPos);
    // empty sample tables - samples are described per-fragment instead
    size_t sttsPos = b.beginBox("stts");
    b.u32(0);
    b.u32(0);
    b.endBox(sttsPos);
    size_t stscPos = b.beginBox("stsc");
    b.u32(0);
    b.u32(0);
    b.endBox(stscPos);
    size_t stszPos = b.beginBox("stsz");
    b.u32(0);
    b.u32(0);
    b.u32(0);
    b.endBox(stszPos);
    size_t stcoPos = b.beginBox("stco");
    b.u32(0);
    b.u32(0);
    b.endBox(stcoPos);
    b.endBox(pos);
  }

  /// Writes the common 28-byte AudioSampleEntry fixed header shared by
  /// 'mp4a' (AAC) and 'sowt' (PCM) - the same layout DemuxerMP4 itself
  /// reads (see its setupAudioInfo()): reserved+data_reference_index,
  /// reserved, channelcount, samplesize, pre_defined+reserved, samplerate.
  void writeAudioSampleEntryHeader(MP4BoxWriter& b) {
    b.zeros(6);  // reserved
    b.u16(1);    // data_reference_index
    b.zeros(8);  // reserved[2]
    b.u16((uint16_t)audio_info.channels);
    b.u16((uint16_t)audio_info.bits_per_sample);
    b.u16(0);  // pre_defined
    b.u16(0);  // reserved
    b.u32((uint32_t)audio_info.sample_rate << 16);
  }

  void writeAudioStbl(MP4BoxWriter& b) {
    size_t pos = b.beginBox("stbl");
    size_t stsdPos = b.beginBox("stsd");
    b.u32(0);  // version + flags
    b.u32(1);  // entry_count
    if (audio_info.format == AudioFormat::PCM) {
      // 'sowt': little-endian signed PCM - the fixed AudioSampleEntry
      // header alone is a complete sample description, no child box
      // needed (unlike 'mp4a', which needs 'esds' for the AAC config).
      size_t sowtPos = b.beginBox("sowt");
      writeAudioSampleEntryHeader(b);
      b.endBox(sowtPos);
    } else {
      size_t mp4aPos = b.beginBox("mp4a");
      writeAudioSampleEntryHeader(b);
      writeEsds(b);
      b.endBox(mp4aPos);
    }
    b.endBox(stsdPos);
    size_t sttsPos = b.beginBox("stts");
    b.u32(0);
    b.u32(0);
    b.endBox(sttsPos);
    size_t stscPos = b.beginBox("stsc");
    b.u32(0);
    b.u32(0);
    b.endBox(stscPos);
    size_t stszPos = b.beginBox("stsz");
    b.u32(0);
    b.u32(0);
    b.u32(0);
    b.endBox(stszPos);
    size_t stcoPos = b.beginBox("stco");
    b.u32(0);
    b.u32(0);
    b.endBox(stcoPos);
    b.endBox(pos);
  }

  void writeTrak(MP4BoxWriter& b, uint32_t trackId, bool isVideo) {
    size_t pos = b.beginBox("trak");
    writeTkhd(b, trackId, isVideo);
    size_t mdiaPos = b.beginBox("mdia");
    writeMdhd(b, isVideo ? video_timescale : audio_timescale);
    writeHdlr(b, isVideo ? "vide" : "soun",
              isVideo ? "VideoHandler" : "SoundHandler");
    size_t minfPos = b.beginBox("minf");
    if (isVideo) {
      size_t vmhdPos = b.beginBox("vmhd");
      b.u32(1);    // version + flags
      b.zeros(8);  // graphicsmode + opcolor
      b.endBox(vmhdPos);
    } else {
      size_t smhdPos = b.beginBox("smhd");
      b.u32(0);  // version + flags
      b.u16(0);  // balance
      b.u16(0);  // reserved
      b.endBox(smhdPos);
    }
    writeDinf(b);
    if (isVideo) {
      writeVideoStbl(b);
    } else {
      writeAudioStbl(b);
    }
    b.endBox(minfPos);
    b.endBox(mdiaPos);
    b.endBox(pos);
  }

  void writeMvex(MP4BoxWriter& b) {
    size_t pos = b.beginBox("mvex");
    size_t trexVideoPos = b.beginBox("trex");
    b.u32(0);  // version + flags
    b.u32(kVideoTrackId);
    b.u32(1);  // default_sample_description_index
    b.u32(video_sample_duration);
    b.u32(0);  // default_sample_size
    b.u32(0);  // default_sample_flags
    b.endBox(trexVideoPos);
    if (has_audio) {
      size_t trexAudioPos = b.beginBox("trex");
      b.u32(0);
      b.u32(kAudioTrackId);
      b.u32(1);
      b.u32(audio_sample_duration);
      b.u32(0);
      b.u32(0);
      b.endBox(trexAudioPos);
    }
    b.endBox(pos);
  }

  void writeFtypMoov() {
    box.clear();
    size_t ftypPos = box.beginBox("ftyp");
    box.fourcc("isom");
    box.u32(0x200);
    box.fourcc("isom");
    box.fourcc("iso5");
    box.fourcc("iso6");
    box.fourcc("mp41");
    box.endBox(ftypPos);

    size_t moovPos = box.beginBox("moov");
    writeMvhd(box);
    writeTrak(box, kVideoTrackId, true);
    if (has_audio) writeTrak(box, kAudioTrackId, false);
    writeMvex(box);
    box.endBox(moovPos);

    p_out->write(box.data(), box.size());
  }

  /// Writes one 'moof' (mfhd + traf{tfhd,tfdt,trun}) + 'mdat' fragment for
  /// a single sample. data_offset in 'trun' is relative to the start of
  /// 'moof' (the "default-base-is-moof" convention) - computed by first
  /// assembling 'moof' in the scratch buffer (its size is independent of
  /// the sample bytes themselves) and backpatching the offset field.
  void writeMoofMdat(uint32_t trackId, const uint8_t* data, size_t len,
                     uint32_t sampleDuration, bool isKeyFrame,
                     uint32_t baseTime) {
    if (p_out == nullptr) return;
    box.clear();
    size_t moofPos = box.beginBox("moof");
    size_t mfhdPos = box.beginBox("mfhd");
    box.u32(0);  // version + flags
    box.u32(fragment_seq);
    box.endBox(mfhdPos);

    size_t trafPos = box.beginBox("traf");
    size_t tfhdPos = box.beginBox("tfhd");
    box.u32(0x020000);  // version 0 + flags: default-base-is-moof
    box.u32(trackId);
    box.endBox(tfhdPos);

    size_t tfdtPos = box.beginBox("tfdt");
    box.u32(0);  // version 0 + flags
    box.u32(baseTime);
    box.endBox(tfdtPos);

    bool isVideo = (trackId == kVideoTrackId);
    // data-offset | sample-duration | sample-size (always explicit per
    // fragment - relying on trex's constant default doesn't hold once a
    // track's per-sample duration can vary, as with PCM audio)
    uint32_t trunFlags = 0x000001 | 0x000100 | 0x000200;
    if (isVideo) trunFlags |= 0x000400;  // + sample-flags

    size_t trunPos = box.beginBox("trun");
    box.u32(trunFlags);
    box.u32(1);  // sample_count
    size_t dataOffsetFieldPos = box.size();
    box.u32(0);  // data_offset placeholder
    // per-sample fields, in the fixed order the spec mandates: duration,
    // size, flags
    box.u32(sampleDuration);
    box.u32((uint32_t)len);
    if (isVideo) {
      // sample_depends_on / sample_is_non_sync_sample
      box.u32(isKeyFrame ? 0x02000000 : 0x01010000);
    }
    box.endBox(trunPos);
    box.endBox(trafPos);
    box.endBox(moofPos);

    uint32_t dataOffset = (uint32_t)(box.size() + 8);  // + mdat header
    box.buffer[dataOffsetFieldPos] = (uint8_t)(dataOffset >> 24);
    box.buffer[dataOffsetFieldPos + 1] = (uint8_t)(dataOffset >> 16);
    box.buffer[dataOffsetFieldPos + 2] = (uint8_t)(dataOffset >> 8);
    box.buffer[dataOffsetFieldPos + 3] = (uint8_t)dataOffset;

    p_out->write(box.data(), box.size());

    uint8_t mdatHeader[8];
    uint32_t mdatSize = (uint32_t)(len + 8);
    mdatHeader[0] = (uint8_t)(mdatSize >> 24);
    mdatHeader[1] = (uint8_t)(mdatSize >> 16);
    mdatHeader[2] = (uint8_t)(mdatSize >> 8);
    mdatHeader[3] = (uint8_t)mdatSize;
    mdatHeader[4] = 'm';
    mdatHeader[5] = 'd';
    mdatHeader[6] = 'a';
    mdatHeader[7] = 't';
    p_out->write(mdatHeader, 8);
    p_out->write(data, len);

    fragment_seq++;
  }
};

}  // namespace audio_tools
