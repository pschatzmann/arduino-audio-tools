#pragma once
#include <string.h>
#include "AudioTools/AudioCodecs/AudioCodecsBase.h"
#include "AudioTools/AudioCodecs/AudioFormat.h"
#include "AudioTools/AudioCodecs/ContainerCommon.h"
#include "AudioTools/CoreAudio/AudioBasic/Collections/Vector.h"
#include "AudioTools/Video/Video.h"

namespace audio_tools {

// ISO/IEC 13818-1 MPEG Transport Stream framing constants
static const size_t MTS_PACKET_SIZE = 188;
static const uint8_t MTS_SYNC_BYTE = 0x47;
static const uint32_t MTS_CLOCK_HZ = 90000;  // PTS/PCR base clock

// PIDs MuxerMTS assigns its own output to - a fixed, conventional layout
// (matching what e.g. ffmpeg's own MPEG-TS muxer defaults to) rather than
// something DemuxerMTS needs to know about: DemuxerMTS discovers the real
// PIDs of whatever stream it is fed from the PAT/PMT tables themselves, so
// it works with any standards-conformant .ts file, not just ones produced
// by MuxerMTS.
static const uint16_t MTS_PID_PAT = 0x0000;
static const uint16_t MTS_PID_PMT = 0x1000;
static const uint16_t MTS_PID_VIDEO = 0x0100;
static const uint16_t MTS_PID_AUDIO = 0x0101;

// PES stream_id values (ISO/IEC 13818-1 Table 2-18) used for the video/
// audio elementary streams - the same convention ContainerMPG.h uses for
// its own (unrelated, MPEG-1 Program Stream) PES packets.
static const uint8_t MTS_STREAM_ID_VIDEO = 0xE0;
static const uint8_t MTS_STREAM_ID_AUDIO = 0xC0;

// PMT stream_type values (ISO/IEC 13818-1 Table 2-34) this container
// recognizes.
static const uint8_t MTS_STREAM_TYPE_MPEG1_VIDEO = 0x01;
static const uint8_t MTS_STREAM_TYPE_MPEG2_VIDEO = 0x02;
static const uint8_t MTS_STREAM_TYPE_MPEG1_AUDIO = 0x03;
static const uint8_t MTS_STREAM_TYPE_MPEG2_AUDIO = 0x04;
static const uint8_t MTS_STREAM_TYPE_AAC_ADTS = 0x0F;
static const uint8_t MTS_STREAM_TYPE_MPEG4_VIDEO = 0x10;
static const uint8_t MTS_STREAM_TYPE_H264 = 0x1B;
static const uint8_t MTS_STREAM_TYPE_PRIVATE = 0x06;
/// ISO/IEC 13818-1 reserves 0x06 ("PES packets containing private data")
/// for user-private use - there is no officially registered stream_type
/// for Motion-JPEG. This library (and e.g. ffmpeg's own MPEG-TS muxer)
/// uses that same value for it by convention: fine for a stream produced
/// by MuxerMTS and read back by DemuxerMTS, but it means a third-party
/// .ts file carrying an unrelated private stream on stream_type 0x06
/// (subtitles, proprietary metadata, ...) would be misidentified as
/// MJPEG video by DemuxerMTS - no descriptor distinguishes the two.
static const uint8_t MTS_STREAM_TYPE_MJPEG = MTS_STREAM_TYPE_PRIVATE;

/// True if 'st' is one of the video stream_type values recognized above.
inline bool isMtsVideoStreamType(uint8_t st) {
  return st == MTS_STREAM_TYPE_MPEG1_VIDEO || st == MTS_STREAM_TYPE_MPEG2_VIDEO ||
         st == MTS_STREAM_TYPE_MPEG4_VIDEO || st == MTS_STREAM_TYPE_H264 ||
         st == MTS_STREAM_TYPE_MJPEG;
}
/// True if 'st' is one of the audio stream_type values recognized above.
inline bool isMtsAudioStreamType(uint8_t st) {
  return st == MTS_STREAM_TYPE_MPEG1_AUDIO || st == MTS_STREAM_TYPE_MPEG2_AUDIO ||
         st == MTS_STREAM_TYPE_AAC_ADTS;
}
/// Maps a PMT video stream_type to the VideoFormat this library uses
/// elsewhere - VideoFormat::UNKNOWN if not one of the 5 recognized above
/// (MPEG-2 video has no dedicated VideoFormat value, so it is folded into
/// MPEG1 - close enough for tagging purposes, since this library never
/// decodes either).
inline VideoFormat mtsVideoFormat(uint8_t st) {
  switch (st) {
    case MTS_STREAM_TYPE_H264:
      return VideoFormat::H264;
    case MTS_STREAM_TYPE_MPEG1_VIDEO:
    case MTS_STREAM_TYPE_MPEG2_VIDEO:
      return VideoFormat::MPEG1;
    case MTS_STREAM_TYPE_MPEG4_VIDEO:
      return VideoFormat::MPEG4;
    case MTS_STREAM_TYPE_MJPEG:
      return VideoFormat::MJPEG;
    default:
      return VideoFormat::UNKNOWN;
  }
}
/// Maps a PMT audio stream_type to the AudioFormat this library uses
/// elsewhere - AudioFormat::UNKNOWN if not one of the 3 recognized above.
inline AudioFormat mtsAudioFormat(uint8_t st) {
  switch (st) {
    case MTS_STREAM_TYPE_AAC_ADTS:
      return AudioFormat::AAC;
    case MTS_STREAM_TYPE_MPEG1_AUDIO:
    case MTS_STREAM_TYPE_MPEG2_AUDIO:
      return AudioFormat::MP3;
    default:
      return AudioFormat::UNKNOWN;
  }
}
/// Maps a VideoFormat to the PMT stream_type MuxerMTS writes for it -
/// MTS_STREAM_TYPE_PRIVATE (with a warning) for anything not H264/MPEG1/
/// MPEG4/MJPEG, the only video codecs this container recognizes.
inline uint8_t mtsVideoStreamType(VideoFormat format) {
  switch (format) {
    case VideoFormat::H264:
      return MTS_STREAM_TYPE_H264;
    case VideoFormat::MPEG1:
      return MTS_STREAM_TYPE_MPEG1_VIDEO;
    case VideoFormat::MPEG4:
      return MTS_STREAM_TYPE_MPEG4_VIDEO;
    case VideoFormat::MJPEG:
      return MTS_STREAM_TYPE_MJPEG;  // see its own comment: user-private by convention
    default:
      LOGW("MuxerMTS: VideoFormat %d has no standard MPEG-TS stream_type - using private data (0x06)",
           (int)format);
      return MTS_STREAM_TYPE_PRIVATE;
  }
}
/// Maps an AudioFormat to the PMT stream_type MuxerMTS writes for it -
/// MTS_STREAM_TYPE_PRIVATE (with a warning) for anything not AAC/MP3.
inline uint8_t mtsAudioStreamType(AudioFormat format) {
  switch (format) {
    case AudioFormat::AAC:
      return MTS_STREAM_TYPE_AAC_ADTS;
    case AudioFormat::MP3:
      return MTS_STREAM_TYPE_MPEG1_AUDIO;
    default:
      LOGW("MuxerMTS: AudioFormat %d has no standard MPEG-TS stream_type - using private data (0x06)",
           (int)format);
      return MTS_STREAM_TYPE_PRIVATE;
  }
}

/// CRC-32/MPEG-2 (poly 0x04C11DB7, init 0xFFFFFFFF, MSB-first, no reflect,
/// no final XOR) - the checksum PAT/PMT/every other PSI table section
/// carries in its last 4 bytes, needed both to write valid PAT/PMT
/// sections (MuxerMTS) and, in a fuller implementation, to validate ones
/// received (kept available for that even though DemuxerMTS currently
/// trusts the sections it parses without checking it).
inline uint32_t mtsCrc32(const uint8_t *data, size_t len) {
  uint32_t crc = 0xFFFFFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= (uint32_t)data[i] << 24;
    for (int b = 0; b < 8; b++) {
      crc = (crc & 0x80000000) ? (crc << 1) ^ 0x04C11DB7 : (crc << 1);
    }
  }
  return crc;
}

/// @brief Minimal byte accumulator used by DemuxerMTS to buffer incoming
/// bytes until at least one full 188-byte TS packet is available - analogous
/// to ContainerMPG.h's MPGParseBuffer, but without any start-code search
/// (TS packets are fixed-size and found via their own resync logic in
/// DemuxerMTS::parse() instead).
/// @ingroup codecs
/// @author Phil Schatzmann
/// @copyright GPLv3
class MTSParseBuffer {
 public:
  void resize(size_t size) { vec.resize(size); }
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

 protected:
  Vector<uint8_t> vec{0};
  size_t count = 0;
};

/**
 * @brief ISO/IEC 13818-1 MPEG Transport Stream Demuxer: parses the fixed
 * 188-byte TS packet framing, follows PAT -> PMT to discover the video and
 * (optional) audio elementary stream PIDs and their codec (stream_type),
 * reassembles each PES packet's payload from the TS packets between one
 * payload_unit_start_indicator and the next, and forwards the raw
 * elementary-stream bytes to setOutputVideo()/setOutputAudio() - same
 * scope and design as DemuxerMPG (ContainerMPG.h): no decoding happens
 * here, point the outputs at whatever decoder understands the codec.
 *
 * PES packet boundaries (marked by payload_unit_start_indicator) are used
 * directly as access-unit boundaries - i.e. every new PES packet on the
 * video/audio PID flushes the previous one as one complete write() call to
 * the corresponding output, mirroring how a real TS muxer (including
 * MuxerMTS itself) packetizes one access unit per PES packet. This is
 * simpler than DemuxerMPG's own PES-length/start-code-driven framing,
 * because TS's own PID demultiplexing already separates the elementary
 * streams apart - no need to hunt for start codes in a mixed byte stream.
 *
 * @note v1 scope: a single video track and a single audio track (the
 * first of each found in the PMT) - multi-program and multi-track
 * transport streams are not supported. Recognized video stream_types are
 * H.264 (0x1B), MPEG-1/2 (0x01/0x02), MPEG-4 (0x10) and MJPEG (0x06 - see
 * MTS_STREAM_TYPE_MJPEG's own comment on why that mapping is a
 * convention, not an ISO standard one); recognized audio stream_types
 * are AAC/ADTS (0x0F) and MPEG-1/2 audio (0x03/0x04). The PES header is
 * assumed to fit
 * within the first TS packet of its PES packet (always true in practice -
 * the PES optional header used here is at most 14 bytes, far under a TS
 * packet's 184-byte payload capacity even with heavy adaptation-field
 * stuffing); a PES header split across a TS packet boundary is logged and
 * that access unit is dropped. Video width/height are not parsed (unlike
 * DemuxerMPG, which can read them from MPEG-1's own sequence_header) -
 * H.264 SPS parsing is out of scope for this library; getVideoInfo()
 * reports only the codec (VideoFormat) and total bytes seen.
 *
 * @code
 * DemuxerMTS demuxer;
 * H264Decoder h264;  // or any AudioTools H264 decoder
 * demuxer.setOutputVideo(videoOutput);
 * AACDecoderHelix aac;
 * EncodedAudioStream audioOut(&i2s, &aac);
 * demuxer.setOutputAudio(audioOut);
 * demuxer.begin();
 * // feed raw .ts bytes:
 * demuxer.write(tsData, len);
 * @endcode
 *
 * @ingroup codecs
 * @ingroup decoder
 * @ingroup video
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class DemuxerMTS : public Demuxer {
 public:
  /// @param bufferSize internal parse buffer size - must be able to hold
  /// at least one full 188-byte TS packet; a few packets' worth (the
  /// default) keeps write() from truncating a typical caller-sized chunk.
  DemuxerMTS(int bufferSize = 8 * MTS_PACKET_SIZE) { parse_buffer.resize(bufferSize); }

  const char *mimeVideo() override { return "video/mp2t"; }
  /// Provides the audio mime, derived from the PMT's audio stream_type
  /// once parsed - "audio/aac" (the common case for .ts/HLS content)
  /// before that.
  const char *mime() override {
    const char *m = toMime(mtsAudioFormat(audio_stream_type));
    return m != nullptr ? m : "audio/aac";
  }

  /// True if `data` starts with the TS sync byte (0x47), confirmed by a
  /// second sync byte 188 bytes later when enough data is available - see
  /// Demuxer::isValid().
  bool isValid(const uint8_t *data, size_t len) override {
    if (len < 1 || data[0] != MTS_SYNC_BYTE) return false;
    if (len >= MTS_PACKET_SIZE + 1) return data[MTS_PACKET_SIZE] == MTS_SYNC_BYTE;
    return true;
  }

  bool begin() override {
    parse_buffer.clear();
    is_parsing_active = true;
    pmt_pid = 0xFFFF;
    video_pid = 0xFFFF;
    audio_pid = 0xFFFF;
    video_stream_type = 0;
    audio_stream_type = 0;
    video_frame_open = false;
    audio_frame_open = false;
    video_unit_len = 0;
    audio_unit_len = 0;
    audio_header_parsed = false;
    audio_sample_rate = 0;
    audio_channels = 0;
    audio_probe.clear();
    total_bytes = 0;
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

  void setOutputAudio(Print &out) override { p_output_audio = &out; }
  void setOutput(Print &out) override { setOutputAudio(out); }
  void setOutputVideo(Print &out) override { p_output_video = &out; }
  void setOutputVideo(VideoOutput &out) override { p_output_video_video = &out; }

  /// Not applicable: both AAC (ADTS) and MPEG audio are compressed and
  /// self-framed, never PCM - provided only to satisfy the Demuxer
  /// interface.
  void setSendWavHeader(bool flag) override { (void)flag; }

  /// Common video info - format is the PMT-reported codec once the PMT
  /// has been parsed (VideoFormat::UNKNOWN before then, or if the
  /// stream_type isn't one this container recognizes); width/height/fps
  /// are not parsed (see class comment) and stay 0.
  VideoInfo getVideoInfo() override {
    VideoInfo result;
    result.format = mtsVideoFormat(video_stream_type);
    result.total_file_size = total_bytes;
    return result;
  }

  /// Common audio info, parsed from the audio ES's own ADTS/MPEG-audio
  /// frame header once seen - falls back to 44100/stereo if not yet
  /// available.
  AudioInfoFormat getAudioInfo() override {
    AudioInfoFormat result;
    result.sample_rate = audio_sample_rate > 0 ? audio_sample_rate : 44100;
    result.channels = audio_channels > 0 ? audio_channels : 2;
    result.bits_per_sample = 16;
    result.format = mtsAudioFormat(audio_stream_type);
    if (result.format == AudioFormat::UNKNOWN) result.format = AudioFormat::AAC;
    return result;
  }

  size_t write(const uint8_t *data, size_t len) override {
    size_t result = parse_buffer.writeArray(data, len);
    if (is_parsing_active) {
      while (parse()) {
      }
    }
    return result;
  }

 protected:
  bool is_parsing_active = false;
  MTSParseBuffer parse_buffer;
  Print *p_output_audio = nullptr;
  Print *p_output_video = nullptr;
  VideoOutput *p_output_video_video = nullptr;
  uint32_t total_bytes = 0;

  uint16_t pmt_pid = 0xFFFF;
  uint16_t video_pid = 0xFFFF;
  uint16_t audio_pid = 0xFFFF;
  uint8_t video_stream_type = 0;
  uint8_t audio_stream_type = 0;

  bool video_frame_open = false;
  bool audio_frame_open = false;
  Vector<uint8_t> video_unit_buffer;
  size_t video_unit_len = 0;
  Vector<uint8_t> audio_unit_buffer;
  size_t audio_unit_len = 0;

  bool audio_header_parsed = false;
  uint32_t audio_sample_rate = 0;
  uint8_t audio_channels = 0;
  Vector<uint8_t> audio_probe;

  /// Consumes one 188-byte packet (resyncing on the sync byte first, if
  /// needed) and parses it. Returns true if progress was made (call again
  /// - more packets may already be buffered), false if it needs more data.
  bool parse() {
    size_t avail = parse_buffer.available();
    if (avail < MTS_PACKET_SIZE) return false;
    uint8_t *buf = parse_buffer.data();
    if (buf[0] != MTS_SYNC_BYTE) {
      // resync: find a 0x47 that is also followed by a 0x47 exactly one
      // packet later (when enough data is buffered to check) so a stray
      // 0x47 inside a payload isn't mistaken for realignment.
      size_t idx = 1;
      for (; idx < avail; idx++) {
        if (buf[idx] != MTS_SYNC_BYTE) continue;
        if (idx + MTS_PACKET_SIZE < avail) {
          if (buf[idx + MTS_PACKET_SIZE] == MTS_SYNC_BYTE) break;
        } else {
          break;  // can't confirm yet, but it's the best guess available
        }
      }
      if (idx < avail) {
        parse_buffer.consume(idx);
        return true;
      }
      parse_buffer.consume(avail);
      return false;
    }
    parsePacket(buf);
    parse_buffer.consume(MTS_PACKET_SIZE);
    total_bytes += MTS_PACKET_SIZE;
    return true;
  }

  void parsePacket(const uint8_t *pkt) {
    if (pkt[1] & 0x80) return;  // transport_error_indicator
    uint16_t pid = (uint16_t)((pkt[1] & 0x1F) << 8) | pkt[2];
    bool pusi = (pkt[1] & 0x40) != 0;
    uint8_t adaptation_control = (pkt[3] >> 4) & 0x03;
    if (adaptation_control == 0x00) return;  // reserved
    size_t pos = 4;
    if (adaptation_control == 0x02 || adaptation_control == 0x03) {
      uint8_t af_len = pkt[4];
      pos = 5 + af_len;
      if (pos > MTS_PACKET_SIZE) return;  // malformed
    }
    if (adaptation_control == 0x02) return;  // adaptation field only, no payload
    size_t payload_len = MTS_PACKET_SIZE - pos;
    if (payload_len == 0) return;
    const uint8_t *payload = pkt + pos;

    if (pid == MTS_PID_PAT) {
      parsePat(payload, payload_len, pusi);
    } else if (pmt_pid != 0xFFFF && pid == pmt_pid) {
      parsePmt(payload, payload_len, pusi);
    } else if (pid == video_pid) {
      handlePesPayload(true, payload, payload_len, pusi);
    } else if (pid == audio_pid) {
      handlePesPayload(false, payload, payload_len, pusi);
    }
  }

  /// Parses the Program Association Section, picking the PID of the PMT
  /// for the first program listed (program_number != 0).
  void parsePat(const uint8_t *data, size_t len, bool pusi) {
    if (!pusi || len < 1) return;
    size_t off = 1 + (size_t)data[0];  // skip pointer_field
    if (off + 8 > len) return;
    if (data[off] != 0x00) return;  // table_id: program_association_section
    uint16_t section_length = (uint16_t)((data[off + 1] & 0x0F) << 8) | data[off + 2];
    size_t after_length = off + 3;
    size_t section_end = after_length + section_length - 4;  // exclude CRC32
    if (section_end > len) section_end = len;
    size_t prog_off = after_length + 5;  // skip transport_stream_id/flags/section#/last_section#
    while (prog_off + 4 <= section_end) {
      uint16_t program_number = (uint16_t)(data[prog_off] << 8) | data[prog_off + 1];
      uint16_t pid = (uint16_t)((data[prog_off + 2] & 0x1F) << 8) | data[prog_off + 3];
      if (program_number != 0) {
        pmt_pid = pid;
        break;
      }
      prog_off += 4;
    }
  }

  /// Parses the (single-section) Program Map, picking out the PID/
  /// stream_type of the first video and first audio elementary stream.
  void parsePmt(const uint8_t *data, size_t len, bool pusi) {
    if (!pusi || len < 1) return;
    size_t off = 1 + (size_t)data[0];  // skip pointer_field
    if (off + 12 > len) return;
    if (data[off] != 0x02) return;  // table_id: TS_program_map_section
    uint16_t section_length = (uint16_t)((data[off + 1] & 0x0F) << 8) | data[off + 2];
    size_t after_length = off + 3;
    size_t section_end = after_length + section_length - 4;  // exclude CRC32
    if (section_end > len) section_end = len;
    uint16_t program_info_length =
        (uint16_t)((data[after_length + 7] & 0x0F) << 8) | data[after_length + 8];
    size_t stream_off = after_length + 9 + program_info_length;

    uint16_t new_video_pid = 0xFFFF, new_audio_pid = 0xFFFF;
    uint8_t new_video_type = 0, new_audio_type = 0;
    while (stream_off + 5 <= section_end) {
      uint8_t stream_type = data[stream_off];
      uint16_t es_pid = (uint16_t)((data[stream_off + 1] & 0x1F) << 8) | data[stream_off + 2];
      uint16_t es_info_length = (uint16_t)((data[stream_off + 3] & 0x0F) << 8) | data[stream_off + 4];
      if (isMtsVideoStreamType(stream_type) && new_video_pid == 0xFFFF) {
        new_video_pid = es_pid;
        new_video_type = stream_type;
      } else if (isMtsAudioStreamType(stream_type) && new_audio_pid == 0xFFFF) {
        new_audio_pid = es_pid;
        new_audio_type = stream_type;
      }
      stream_off += 5 + es_info_length;
    }
    video_pid = new_video_pid;
    video_stream_type = new_video_type;
    audio_pid = new_audio_pid;
    audio_stream_type = new_audio_type;
  }

  /// Reassembles one PES packet's payload per PID from the TS packets
  /// between one payload_unit_start_indicator and the next - see class
  /// comment for why a PUSI boundary is used directly as the access-unit
  /// boundary instead of tracking PES_packet_length.
  void handlePesPayload(bool isVideo, const uint8_t *payload, size_t len, bool pusi) {
    bool &frame_open = isVideo ? video_frame_open : audio_frame_open;
    if (pusi) {
      if (frame_open) flushUnit(isVideo);
      if (len < 6 || !(payload[0] == 0 && payload[1] == 0 && payload[2] == 1)) {
        LOGW("DemuxerMTS: missing PES start code on PID %s",
             isVideo ? "video" : "audio");
        return;
      }
      if (len < 7) {
        LOGW("DemuxerMTS: PES header split across TS packet boundary - dropping unit");
        return;
      }
      uint8_t flags2 = payload[7];
      size_t header_data_length = (len >= 9) ? payload[8] : 0;
      size_t header_total = 9 + header_data_length;
      (void)flags2;
      if (len < header_total) {
        LOGW("DemuxerMTS: PES header split across TS packet boundary - dropping unit");
        return;
      }
      frame_open = true;
      const uint8_t *es = payload + header_total;
      size_t es_len = len - header_total;
      if (es_len > 0) {
        if (!isVideo && !audio_header_parsed) probeAudioHeader(es, es_len);
        appendToUnit(isVideo, es, es_len);
      }
    } else {
      if (!frame_open) return;  // no unit in progress - drop stray continuation
      if (!isVideo && !audio_header_parsed) probeAudioHeader(payload, len);
      appendToUnit(isVideo, payload, len);
    }
  }

  void appendToUnit(bool isVideo, const uint8_t *data, size_t len) {
    if (isVideo)
      appendToVideo(data, len);
    else
      appendToAudio(data, len);
  }

  /// Appends to video_unit_buffer with amortized (doubling) growth, same
  /// rationale as ContainerMPG.h's own appendToVideo/appendToAudio.
  void appendToVideo(const uint8_t *data, size_t len) {
    size_t needed = video_unit_len + len;
    if (needed > (size_t)video_unit_buffer.size()) {
      size_t newCap = video_unit_buffer.size() == 0 ? 8192 : (size_t)video_unit_buffer.size() * 2;
      if (newCap < needed) newCap = needed;
      video_unit_buffer.resize(newCap);
    }
    memcpy(video_unit_buffer.data() + video_unit_len, data, len);
    video_unit_len += len;
  }
  void appendToAudio(const uint8_t *data, size_t len) {
    size_t needed = audio_unit_len + len;
    if (needed > (size_t)audio_unit_buffer.size()) {
      size_t newCap = audio_unit_buffer.size() == 0 ? 4096 : (size_t)audio_unit_buffer.size() * 2;
      if (newCap < needed) newCap = needed;
      audio_unit_buffer.resize(newCap);
    }
    memcpy(audio_unit_buffer.data() + audio_unit_len, data, len);
    audio_unit_len += len;
  }

  void flushUnit(bool isVideo) {
    if (isVideo)
      flushVideoUnit();
    else
      flushAudioUnit();
  }

  void flushVideoUnit() {
    if (video_unit_len > 0) {
      if (p_output_video != nullptr) p_output_video->write(video_unit_buffer.data(), video_unit_len);
      if (p_output_video_video != nullptr)
        p_output_video_video->write(video_unit_buffer.data(), video_unit_len);
      video_unit_len = 0;
    }
    if (p_output_video != nullptr) p_output_video->flush();
    if (p_output_video_video != nullptr) p_output_video_video->flush();
    video_frame_open = false;
  }

  void flushAudioUnit() {
    if (audio_unit_len > 0) {
      if (p_output_audio != nullptr) p_output_audio->write(audio_unit_buffer.data(), audio_unit_len);
      audio_unit_len = 0;
    }
    audio_frame_open = false;
  }

  /// Inspects the first bytes of the audio ES for an ADTS (AAC) or MPEG
  /// audio frame header - whichever matches audio_stream_type - to
  /// determine sample_rate/channels. Same approach as ContainerMPG.h's
  /// probeAudioHeader(), adapted for ADTS's layout.
  void probeAudioHeader(const uint8_t *data, size_t len) {
    size_t old_size = (size_t)audio_probe.size();
    size_t take = old_size < 4 ? 4 - old_size : 0;
    if (take > len) take = len;
    for (size_t i = 0; i < take; i++) {
      uint8_t b = data[i];
      audio_probe.push_back(b);
    }
    if ((size_t)audio_probe.size() < 4) return;

    uint8_t b0 = audio_probe[0], b1 = audio_probe[1], b2 = audio_probe[2], b3 = audio_probe[3];
    if (audio_stream_type == MTS_STREAM_TYPE_AAC_ADTS) {
      if (b0 == 0xFF && (b1 & 0xF0) == 0xF0) {
        uint8_t sr_index = (b2 >> 2) & 0x0F;
        uint8_t chan_cfg = (uint8_t)(((b2 & 0x01) << 2) | ((b3 >> 6) & 0x03));
        static const uint32_t rates[16] = {96000, 88200, 64000, 48000, 44100, 32000,
                                            24000, 22050, 16000, 12000, 11025, 8000,
                                            7350,  0,     0,     0};
        uint32_t rate = rates[sr_index];
        if (rate > 0) {
          audio_sample_rate = rate;
          audio_channels = chan_cfg > 0 ? chan_cfg : 2;
        }
      }
    } else if (audio_stream_type == MTS_STREAM_TYPE_MPEG1_AUDIO ||
               audio_stream_type == MTS_STREAM_TYPE_MPEG2_AUDIO) {
      if (b0 == 0xFF && (b1 & 0xE0) == 0xE0) {
        uint8_t version = (b1 >> 3) & 0x03;
        uint8_t sr_index = (b2 >> 2) & 0x03;
        uint8_t mode = (b3 >> 6) & 0x03;
        static const uint32_t rates_v1[4] = {44100, 48000, 32000, 0};
        static const uint32_t rates_v2[4] = {22050, 24000, 16000, 0};
        static const uint32_t rates_v25[4] = {11025, 12000, 8000, 0};
        uint32_t rate = 0;
        if (version == 0x03) rate = rates_v1[sr_index];
        else if (version == 0x02) rate = rates_v2[sr_index];
        else if (version == 0x00) rate = rates_v25[sr_index];
        if (rate > 0) {
          audio_sample_rate = rate;
          audio_channels = (mode == 0x03) ? 1 : 2;
        }
      }
    }
    audio_header_parsed = true;
    audio_probe.clear();
    audio_probe.shrink_to_fit();
  }
};

/**
 * @brief ISO/IEC 13818-1 MPEG Transport Stream Encoder: packetizes an
 * already-encoded video elementary stream (H.264 by default - see
 * MuxerVideoConfig::format) and an optional audio elementary stream (AAC
 * ADTS by default - see setAudioInfo()) into 188-byte TS packets, with a
 * PAT and PMT describing the one program/two streams, written to a Print
 * (a local File to record a .ts, or e.g. a network Client to publish a
 * live/HLS-segment stream).
 *
 * Like MuxerMPG/MuxerAVI, this is a *streaming* writer: PAT+PMT are
 * (re-)written before every video keyframe (so a player/segmenter joining
 * mid-stream, or starting a new HLS segment, always sees them up front),
 * and each addVideoFrame()/addAudioFrame() call becomes exactly one PES
 * packet, split across as many TS packets as its size needs (no PES
 * length limit is enforced: PES_packet_length is written literally when
 * it fits 16 bits, 0 ("unbounded", the standard convention for a video
 * elementary stream whose access units routinely exceed 64KB) otherwise -
 * DemuxerMTS itself never relies on this field, only on
 * payload_unit_start_indicator, for framing). A PCR (derived from the
 * same PTS clock as the video track, not a wall clock) is attached to
 * every video access unit's first TS packet, matching the PMT's PCR_PID
 * (always the video PID).
 *
 * MJPEG is also supported (VideoFormat::MJPEG + addJpegFrame(), one
 * complete JPEG image per call) - see MTS_STREAM_TYPE_MJPEG's own comment
 * for how it's signalled, since MPEG-TS has no ISO-registered
 * stream_type for it.
 *
 * Usage:
 * @code
 * MuxerMTS mts(client);  // any Print: File, WiFiClient, ...
 * MuxerVideoConfig cfg;
 * cfg.width = 1280;
 * cfg.height = 720;
 * cfg.fps = 30;
 * cfg.format = VideoFormat::H264;
 * mts.setVideoInfo(cfg);
 * mts.setAudioInfo(AudioInfoFormat(48000, 2, 16, AudioFormat::AAC));
 * mts.begin();
 * // for each encoded H.264 access unit (Annex-B, starting with its own
 * // start codes):
 * mts.addVideoFrame(h264_access_unit, len, isKeyFrame);
 * // for each raw ADTS-framed AAC frame:
 * mts.addAudioFrame(adts_frame, len);
 * @endcode
 * (VideoMuxer/VideoMuxerWithTasks drive the video side, and
 * setStreamType()-toggled write(), automatically - MuxerMTS implements
 * the same Muxer interface as MuxerAVI/MuxerMP4/MuxerMPG, so it plugs
 * into them unmodified.)
 *
 * @ingroup codecs
 * @ingroup encoder
 * @ingroup video
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class MuxerMTS : public Muxer {
 public:
  MuxerMTS() {
    video_cfg.format = VideoFormat::H264;
    audio_info.format = AudioFormat::AAC;
  }
  MuxerMTS(Print &out) : MuxerMTS() { setOutput(out); }

  const char *mimeVideo() override { return "video/mp2t"; }
  void setOutput(Print &out) override { p_out = &out; }

  void setVideoInfo(MuxerVideoConfig config) override { video_cfg = config; }
  MuxerVideoConfig getVideoInfo() override { return video_cfg; }

  /// Adds an (optional) audio track. info.format selects the PMT
  /// stream_type (AudioFormat::AAC -> ADTS/0x0F, AudioFormat::MP3 ->
  /// MPEG-1 audio/0x03 - the only two with a standard MPEG-TS mapping).
  /// Call before begin().
  void setAudioInfo(AudioInfoFormat info) override {
    audio_info = info;
    if (audio_info.format == AudioFormat::UNKNOWN) audio_info.format = AudioFormat::AAC;
    audio_samples_per_frame = (audio_info.format == AudioFormat::MP3) ? 1152 : 1024;
    has_audio = true;
  }
  AudioInfoFormat &audioInfo() override { return audio_info; }

  /// Writes the initial PAT + PMT. Call after configuring video (and
  /// audio, if any) and before writing any frames.
  bool begin() override {
    if (p_out == nullptr) {
      LOGE("output not defined");
      return false;
    }
    video_frame_count = 0;
    audio_frame_count = 0;
    pat_cc = 0;
    pmt_cc = 0;
    video_cc = 0;
    audio_cc = 0;
    writePat();
    writePmt();
    is_open = true;
    return true;
  }

  void setStreamType(StreamContentType type) override { write_stream_type = type; }
  StreamContentType streamType() override { return write_stream_type; }

  /// MPEG-TS has no defined trailer - simply stops accepting frames.
  void end() override { is_open = false; }

  operator bool() override { return is_open; }

  size_t write(const uint8_t *data, size_t len) override {
    if (write_stream_type == StreamContentType::Audio) return addAudioFrame(data, len);
    return addVideoFrame(data, len);
  }

  /// Writes one complete, already-encoded video access unit as a single
  /// PES packet (see class comment). Re-sends PAT+PMT first when
  /// isKeyFrame is true.
  size_t addVideoFrame(const uint8_t *data, size_t len, bool isKeyFrame = true) override {
    if (!is_open || data == nullptr || len == 0) return 0;
    if (isKeyFrame) {
      writePat();
      writePmt();
    }
    uint64_t pts = videoPts();
    writeAccessUnit(MTS_PID_VIDEO, MTS_STREAM_ID_VIDEO, video_cc, data, len, pts,
                     /*withPcr=*/true);
    video_frame_count++;
    return len;
  }

  /// Writes one complete, already-encoded JPEG image as a single PES
  /// packet (see MTS_STREAM_TYPE_MJPEG's own comment for how this
  /// container signals MJPEG - there is no ISO-registered stream_type for
  /// it). isKeyFrame is always true: every JPEG image is independently
  /// decodable, so - unlike addVideoFrame()'s H.264-oriented default -
  /// this always re-sends PAT+PMT first too (same convention
  /// MuxerVideoSink, ContainerCommon.h, already uses for MJPEG).
  size_t addJpegFrame(const uint8_t *data, size_t len) override {
    if (video_cfg.format != VideoFormat::MJPEG)
      LOGW("MuxerMTS: getVideoInfo().format does not match addJpegFrame()");
    return addVideoFrame(data, len, true);
  }
  size_t addYUV422Frame(const uint8_t *data, size_t len) override {
    LOGW("MuxerMTS: raw YUV422 is not supported by MPEG-TS");
    return addVideoFrame(data, len);
  }
  size_t addRGB565Frame(const uint8_t *data, size_t len) override {
    LOGW("MuxerMTS: raw RGB565 is not supported by MPEG-TS");
    return addVideoFrame(data, len);
  }
  size_t addI420Frame(const uint8_t *data, size_t len) override {
    LOGW("MuxerMTS: raw I420 is not supported by MPEG-TS");
    return addVideoFrame(data, len);
  }

  /// Writes one complete audio frame (e.g. one ADTS-framed AAC frame, or
  /// one MP3 frame) as a single PES packet. Ignored (returns 0) if no
  /// audio track was configured via setAudioInfo().
  size_t addAudioFrame(const uint8_t *data, size_t len) override {
    if (!is_open || !has_audio || data == nullptr || len == 0) return 0;
    uint64_t pts = audioPts();
    writeAccessUnit(MTS_PID_AUDIO, MTS_STREAM_ID_AUDIO, audio_cc, data, len, pts,
                     /*withPcr=*/false);
    audio_frame_count++;
    return len;
  }

  uint32_t videoFrameCount() { return video_frame_count; }
  uint32_t audioFrameCount() { return audio_frame_count; }

 protected:
  Print *p_out = nullptr;
  MuxerVideoConfig video_cfg;
  AudioInfoFormat audio_info;
  bool has_audio = false;
  bool is_open = false;
  StreamContentType write_stream_type = StreamContentType::Video;
  uint32_t video_frame_count = 0;
  uint32_t audio_frame_count = 0;
  uint32_t audio_samples_per_frame = 1024;  // AAC: 1024, MP3: 1152

  uint8_t pat_cc = 0, pmt_cc = 0, video_cc = 0, audio_cc = 0;

  uint64_t videoPts() {
    float fps = video_cfg.fps > 0 ? video_cfg.fps : 25.0f;
    return (uint64_t)((double)video_frame_count * MTS_CLOCK_HZ / fps);
  }
  uint64_t audioPts() {
    uint32_t sr = audio_info.sample_rate > 0 ? audio_info.sample_rate : 44100;
    return (uint64_t)((double)audio_frame_count * audio_samples_per_frame * MTS_CLOCK_HZ / sr);
  }

  /// Writes a 5-byte PTS/DTS-only timestamp field (ISO/IEC 13818-1
  /// 2.4.3.7): 4-bit marker (0x02 for a PTS-only field), 3x15-bit chunks
  /// of the 33-bit timestamp each terminated by a marker_bit.
  static void writeTsTimestamp(uint8_t *b, uint64_t ts) {
    b[0] = (uint8_t)((0x02 << 4) | (((ts >> 30) & 0x07) << 1) | 0x01);
    uint16_t mid = (uint16_t)((((ts >> 15) & 0x7FFF) << 1) | 0x01);
    uint16_t low = (uint16_t)(((ts & 0x7FFF) << 1) | 0x01);
    b[1] = (uint8_t)(mid >> 8);
    b[2] = (uint8_t)mid;
    b[3] = (uint8_t)(low >> 8);
    b[4] = (uint8_t)low;
  }

  /// Encodes a PCR (program_clock_reference) field: a 33-bit base (this
  /// muxer's 90kHz PTS-style clock, matching the video track's own
  /// timestamps rather than a wall clock - same simplification MuxerMPG
  /// makes for its SCR) plus a 9-bit extension, always 0 here (adequate
  /// precision for a streaming muxer, not frame-accurate 27MHz jitter
  /// control).
  static void writePcr(uint8_t *buf, uint64_t pcr_base) {
    pcr_base &= 0x1FFFFFFFFULL;
    buf[0] = (uint8_t)(pcr_base >> 25);
    buf[1] = (uint8_t)(pcr_base >> 17);
    buf[2] = (uint8_t)(pcr_base >> 9);
    buf[3] = (uint8_t)(pcr_base >> 1);
    buf[4] = (uint8_t)(((pcr_base & 0x1) << 7) | 0x7E);
    buf[5] = 0x00;
  }

  /// Copies 'n' bytes starting at virtual offset 'voffset' out of the
  /// logical concatenation of [a, a+alen) followed by [b, b+blen) - lets
  /// packBytesToTs() packetize a small PES header and a (possibly large)
  /// ES payload as one continuous byte stream without ever copying the
  /// payload into a combined buffer first.
  static void copyVirtual(uint8_t *dst, size_t voffset, size_t n, const uint8_t *a, size_t alen,
                           const uint8_t *b, size_t blen) {
    size_t written = 0;
    if (voffset < alen) {
      size_t take = min(n, alen - voffset);
      memcpy(dst, a + voffset, take);
      written += take;
    }
    if (written < n) {
      size_t bOffset = (voffset + written) - alen;
      memcpy(dst + written, b + bOffset, n - written);
    }
  }

  /// Splits the logical concatenation of a PES header (header/header_len)
  /// and its ES payload (payload/payload_len) into as many 188-byte TS
  /// packets as needed. payload_unit_start_indicator is set on the first
  /// packet only; the continuity_counter 'cc' is threaded through and
  /// incremented per packet. When 'withPcr' is set, the first packet
  /// additionally carries a PCR in its adaptation field (stealing from
  /// its own payload capacity for that packet only) - see writePcr().
  /// Every packet that doesn't fill its 184-byte payload capacity exactly
  /// is padded to it via adaptation-field stuffing (0xFF bytes) - this
  /// only ever applies to the last packet of the run.
  void packBytesToTs(uint16_t pid, uint8_t &cc, const uint8_t *header, size_t header_len,
                      const uint8_t *payload, size_t payload_len, bool withPcr, uint64_t pcr) {
    size_t total_len = header_len + payload_len;
    size_t offset = 0;
    bool first = true;
    do {
      size_t remaining = total_len - offset;
      bool addPcr = first && withPcr;
      bool use_af = addPcr || remaining < 184;
      uint8_t pkt[MTS_PACKET_SIZE];
      pkt[0] = MTS_SYNC_BYTE;
      pkt[1] = (uint8_t)((first ? 0x40 : 0x00) | ((pid >> 8) & 0x1F));
      pkt[2] = (uint8_t)(pid & 0xFF);
      size_t pos, chunk;
      if (!use_af) {
        pkt[3] = (uint8_t)(0x10 | (cc & 0x0F));  // adaptation_field_control=01 (payload only)
        pos = 4;
        chunk = 184;
      } else {
        size_t overhead = 2 + (addPcr ? 6 : 0);  // adaptation_field_length byte + flags byte + PCR
        chunk = min(remaining, 184 - overhead);
        size_t stuffing = 184 - overhead - chunk;
        pkt[3] = (uint8_t)(0x30 | (cc & 0x0F));  // adaptation_field_control=11 (adaptation + payload)
        pkt[4] = (uint8_t)(1 + (addPcr ? 6 : 0) + stuffing);
        pkt[5] = addPcr ? 0x10 : 0x00;  // PCR_flag
        pos = 6;
        if (addPcr) {
          writePcr(pkt + pos, pcr);
          pos += 6;
        }
        for (size_t i = 0; i < stuffing; i++) pkt[pos++] = 0xFF;
      }
      copyVirtual(pkt + pos, offset, chunk, header, header_len, payload, payload_len);
      pos += chunk;
      cc = (uint8_t)((cc + 1) & 0x0F);
      p_out->write(pkt, MTS_PACKET_SIZE);
      offset += chunk;
      first = false;
    } while (offset < total_len);
  }

  /// Builds the (ISO/IEC 13818-1) PES header for one PTS-tagged access
  /// unit and hands the whole PES packet (header + payload) to
  /// packBytesToTs().
  void writeAccessUnit(uint16_t pid, uint8_t stream_id, uint8_t &cc, const uint8_t *data, size_t len,
                        uint64_t pts, bool withPcr) {
    static const size_t kOptionalLen = 8;  // 3 flag/length bytes + 5-byte PTS field
    size_t pes_payload = kOptionalLen + len;
    uint16_t pes_len = (pes_payload <= 0xFFFF) ? (uint16_t)pes_payload : 0;

    uint8_t hdr[6 + kOptionalLen];
    hdr[0] = 0x00;
    hdr[1] = 0x00;
    hdr[2] = 0x01;
    hdr[3] = stream_id;
    hdr[4] = (uint8_t)(pes_len >> 8);
    hdr[5] = (uint8_t)pes_len;
    hdr[6] = 0x80;  // '10' marker + scrambling/priority/alignment/copyright flags = 0
    hdr[7] = 0x80;  // PTS_DTS_flags='10' (PTS only), other flags 0
    hdr[8] = 0x05;  // PES_header_data_length: 5 bytes (the PTS field) follow
    writeTsTimestamp(hdr + 9, pts);

    packBytesToTs(pid, cc, hdr, sizeof(hdr), data, len, withPcr, pts);
  }

  void writeStreamEntry(uint8_t *pkt, size_t &pos, uint8_t stream_type, uint16_t pid) {
    pkt[pos++] = stream_type;
    pkt[pos++] = (uint8_t)(0xE0 | ((pid >> 8) & 0x1F));
    pkt[pos++] = (uint8_t)(pid & 0xFF);
    pkt[pos++] = 0xF0;  // reserved(4) + ES_info_length high(4)=0
    pkt[pos++] = 0x00;  // ES_info_length low = 0 (no descriptors)
  }

  /// Writes the Program Association Section: one program (program_number
  /// 1) pointing at the PMT PID.
  void writePat() {
    uint8_t pkt[MTS_PACKET_SIZE];
    memset(pkt, 0xFF, sizeof(pkt));
    pkt[0] = MTS_SYNC_BYTE;
    pkt[1] = (uint8_t)(0x40 | ((MTS_PID_PAT >> 8) & 0x1F));
    pkt[2] = (uint8_t)(MTS_PID_PAT & 0xFF);
    pkt[3] = (uint8_t)(0x10 | (pat_cc & 0x0F));
    pat_cc = (uint8_t)((pat_cc + 1) & 0x0F);

    size_t pos = 4;
    pkt[pos++] = 0x00;  // pointer_field
    size_t section_start = pos;
    pkt[pos++] = 0x00;  // table_id: program_association_section
    size_t len_pos = pos;
    pos += 2;  // section_length placeholder
    size_t after_len = pos;
    pkt[pos++] = 0x00;
    pkt[pos++] = 0x01;  // transport_stream_id = 1
    pkt[pos++] = 0xC1;  // reserved(2)=11 + version(5)=0 + current_next_indicator(1)=1
    pkt[pos++] = 0x00;  // section_number
    pkt[pos++] = 0x00;  // last_section_number
    pkt[pos++] = 0x00;
    pkt[pos++] = 0x01;  // program_number = 1
    pkt[pos++] = (uint8_t)(0xE0 | ((MTS_PID_PMT >> 8) & 0x1F));
    pkt[pos++] = (uint8_t)(MTS_PID_PMT & 0xFF);

    size_t section_length = (pos - after_len) + 4;  // + CRC32
    pkt[len_pos] = (uint8_t)(0xB0 | ((section_length >> 8) & 0x0F));
    pkt[len_pos + 1] = (uint8_t)(section_length & 0xFF);
    uint32_t crc = mtsCrc32(pkt + section_start, pos - section_start);
    pkt[pos++] = (uint8_t)(crc >> 24);
    pkt[pos++] = (uint8_t)(crc >> 16);
    pkt[pos++] = (uint8_t)(crc >> 8);
    pkt[pos++] = (uint8_t)crc;

    p_out->write(pkt, MTS_PACKET_SIZE);
  }

  /// Writes the Program Map Section: the video track (PCR_PID is always
  /// the video PID) plus, if configured, the audio track.
  void writePmt() {
    uint8_t pkt[MTS_PACKET_SIZE];
    memset(pkt, 0xFF, sizeof(pkt));
    pkt[0] = MTS_SYNC_BYTE;
    pkt[1] = (uint8_t)(0x40 | ((MTS_PID_PMT >> 8) & 0x1F));
    pkt[2] = (uint8_t)(MTS_PID_PMT & 0xFF);
    pkt[3] = (uint8_t)(0x10 | (pmt_cc & 0x0F));
    pmt_cc = (uint8_t)((pmt_cc + 1) & 0x0F);

    size_t pos = 4;
    pkt[pos++] = 0x00;  // pointer_field
    size_t section_start = pos;
    pkt[pos++] = 0x02;  // table_id: TS_program_map_section
    size_t len_pos = pos;
    pos += 2;  // section_length placeholder
    size_t after_len = pos;
    pkt[pos++] = 0x00;
    pkt[pos++] = 0x01;  // program_number = 1
    pkt[pos++] = 0xC1;  // reserved(2)=11 + version(5)=0 + current_next_indicator(1)=1
    pkt[pos++] = 0x00;  // section_number
    pkt[pos++] = 0x00;  // last_section_number
    pkt[pos++] = (uint8_t)(0xE0 | ((MTS_PID_VIDEO >> 8) & 0x1F));  // PCR_PID = video PID
    pkt[pos++] = (uint8_t)(MTS_PID_VIDEO & 0xFF);
    pkt[pos++] = 0xF0;  // reserved(4) + program_info_length high(4)=0
    pkt[pos++] = 0x00;  // program_info_length low = 0

    writeStreamEntry(pkt, pos, mtsVideoStreamType(video_cfg.format), MTS_PID_VIDEO);
    if (has_audio) writeStreamEntry(pkt, pos, mtsAudioStreamType(audio_info.format), MTS_PID_AUDIO);

    size_t section_length = (pos - after_len) + 4;  // + CRC32
    pkt[len_pos] = (uint8_t)(0xB0 | ((section_length >> 8) & 0x0F));
    pkt[len_pos + 1] = (uint8_t)(section_length & 0xFF);
    uint32_t crc = mtsCrc32(pkt + section_start, pos - section_start);
    pkt[pos++] = (uint8_t)(crc >> 24);
    pkt[pos++] = (uint8_t)(crc >> 16);
    pkt[pos++] = (uint8_t)(crc >> 8);
    pkt[pos++] = (uint8_t)crc;

    p_out->write(pkt, MTS_PACKET_SIZE);
  }
};

}  // namespace audio_tools
