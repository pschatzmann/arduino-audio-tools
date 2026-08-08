/*
 * Author: Phil Schatzmann
 *
 * H.264 RTP Encoder for RTSP Video Streaming
 * Implements RFC 6184 (packetization-mode=1: Single NAL Unit + FU-A)
 */

#pragma once

#include "AudioTools/AudioCodecs/AudioCodecsBase.h"
#include "AudioTools/AudioCodecs/CodecBase64.h"
#include "AudioTools/CoreAudio/AudioBasic/Collections/Vector.h"
#include "RTSPFormat.h"
#include "RTSPFragmentQueue.h"
#include "RTSPVideoEncoder.h"

namespace audio_tools {

/**
 * @brief H.264 RTP Encoder - Packetizes Annex-B H.264 access units per RFC
 * 6184 for RTSP video streaming.
 *
 * Like JPEGRtpEncoder, this class plays both roles needed to get a video
 * frame onto the wire:
 * - As an AudioEncoder, write() accepts one complete Annex-B access unit
 *   (all the NAL units for one encoded frame, e.g. the output of a single
 *   H264Encoder::encode()/captureH264() call from the codec-h264-ESP32S3
 *   library) per call - there is no cross-call accumulation, since Annex-B
 *   has no reliable in-stream "end of frame" marker to detect the way JPEG
 *   has 0xFFD9.
 * - As an IMediaSource, packetSize()/readBytes() hand out the resulting RTP
 *   payloads one fragment at a time: NAL units that fit in one packet are
 *   sent as Single NAL Unit packets (payload = NAL bytes verbatim); larger
 *   ones are split into FU-A fragments, each with its own 2-byte FU header.
 *
 * SPS (type 7) and PPS (type 8) NAL units are cached as soon as they are
 * seen and re-sent (as their own Single NAL Unit packets) immediately
 * before every IDR frame, so clients that join mid-stream or lose a packet
 * can still resync - the same approach used by RFC 6184-based servers in
 * general. The cached SPS is also used to derive profile-level-id and,
 * together with the cached PPS, sprop-parameter-sets for the SDP - pushed
 * into the associated RTSPFormatH264 as soon as both are available (see
 * RTSPFormatH264 for why this can only happen after the first IDR frame).
 *
 * Usage with RTSPOutput:
 * ```cpp
 * RTSPFormatH264 h264Format(1280, 720, 15.0f);
 * H264RtpEncoder h264Encoder;
 * RTSPOutput<RTSPPlatformWiFi> rtspOutput(h264Format, h264Encoder);
 * ```
 *
 * @ingroup rtsp
 * @ingroup codecs
 * @author Phil Schatzmann
 */
class H264RtpEncoder : public RTSPVideoEncoder, protected RTSPFragmentQueue {
 public:
  H264RtpEncoder() = default;

  /**
   * @brief Constructor with maximum fragment size
   * @param maxFragmentSize Maximum RTP payload size per packet (default:
   * 1400 bytes)
   */
  H264RtpEncoder(size_t maxFragmentSize) : m_maxFragmentSize(maxFragmentSize) {}

  /// Set the maximum RTP payload size per packet
  void setMaxFragmentSize(size_t size) { m_maxFragmentSize = size; }

  /// Maximum number of complete frames buffered when the consumer falls behind
  void setMaxQueuedFrames(size_t frames) { m_maxQueuedFrames = frames; }

  /// Associates the RTSPFormat (an RTSPFormatH264) that receives
  /// sprop-parameter-sets/profile-level-id once SPS/PPS are known (via the
  /// virtual RTSPFormat::setSpropParameterSets()/setProfileLevelId() hooks)
  /// and supplies width/height/framerate.
  void setFormat(RTSPFormat &format) override { p_format = &format; }

  // -- IMediaSource --------------------------------------------------

  RTSPFormat &getFormat() override {
    return p_format != nullptr ? *p_format : default_format;
  }

  void start() override { begin(); }
  void stop() override { end(); }

  int packetSize() override { return queuePacketSize(); }

  /// Retrieves the next queued fragment. Call packetSize() first and pass
  /// its result as maxBytes so exactly one fragment is read.
  int readBytes(void *dest, int maxBytes) override {
    return queueReadBytes(dest, maxBytes);
  }

  // -- AudioEncoder ----------------------------------------------------

  bool begin() override {
    LOGI("Starting H264 RTP Encoder");
    m_isStarted = true;
    m_sps.clear();
    m_pps.clear();
    clearQueue();
    return true;
  }

  void end() override {
    LOGI("Stopping H264 RTP Encoder");
    m_isStarted = false;
    clearQueue();
  }

  void setAudioInfo(AudioInfo info) override {
    // Video encoder: dimensions/framerate come from the RTSPFormat, not
    // AudioInfo
    (void)info;
  }

  const char *mime() override { return "video/H264"; }

  /**
   * @brief Process one complete Annex-B access unit (all NAL units of one
   * encoded frame) and queue the resulting RTP fragments.
   *
   * @param data Pointer to Annex-B H.264 data (one or more start-code
   * delimited NAL units belonging to a single frame)
   * @param len Length of the data in bytes
   * @return Number of bytes processed (always the full input, matching
   * AudioEncoder::write() semantics; call once per encoded frame)
   */
  size_t write(const uint8_t *data, size_t len) override {
    if (!m_isStarted || len == 0) return 0;

    Vector<size_t> offsets;
    Vector<size_t> lengths;
    extractNALs(data, len, offsets, lengths);
    if (offsets.empty()) {
      LOGW("H264RtpEncoder: no NAL units found in %u byte buffer", (unsigned)len);
      return len;
    }

    // Cache SPS/PPS whenever seen, detect IDR, and count the NAL units
    // that will actually go out on the wire (SPS/PPS handled exclusively
    // through the inject-before-IDR path below, so a frame that already
    // carries its own SPS/PPS - as the first IDR frame from most encoders
    // does - does not end up sending them a second time from this scan).
    bool isIdr = false;
    size_t nonParamNalCount = 0;
    for (size_t i = 0; i < offsets.size(); i++) {
      const uint8_t *nal = data + offsets[i];
      uint8_t type = nal[0] & 0x1F;
      if (type == 5) {
        isIdr = true;
      } else if (type == 7) {
        cacheSps(nal, lengths[i]);
        continue;
      } else if (type == 8) {
        cachePps(nal, lengths[i]);
        continue;
      }
      ++nonParamNalCount;
    }

    bool injectParams = isIdr && !m_sps.empty() && !m_pps.empty();
    size_t totalNals = nonParamNalCount + (injectParams ? 2 : 0);
    size_t nalIndex = 0;

    if (injectParams) {
      emitNal(m_sps.data(), m_sps.size(), ++nalIndex == totalNals);
      emitNal(m_pps.data(), m_pps.size(), ++nalIndex == totalNals);
    }
    for (size_t i = 0; i < offsets.size(); i++) {
      const uint8_t *nal = data + offsets[i];
      uint8_t type = nal[0] & 0x1F;
      if (type == 7 || type == 8) continue;  // already (re-)sent above
      emitNal(nal, lengths[i], ++nalIndex == totalNals);
    }

    return len;
  }

  operator bool() override { return m_isStarted; }

 protected:
  RTSPFormat *p_format = nullptr;
  RTSPFormatH264 default_format;  // fallback so getFormat() is never null

  bool m_isStarted = false;
  size_t m_maxFragmentSize = 1400;  // Typical MTU minus IP/UDP/RTP headers

  Vector<uint8_t> m_sps;
  Vector<uint8_t> m_pps;

  // Reused scratch buffer for building one FU-A fragment at a time
  Vector<uint8_t> m_scratch;

  /**
   * @brief Locate the next Annex-B start code (00 00 01 or 00 00 00 01) at
   * or after `from`.
   * @return Index of the first 0x00 of the start code, or len if none found
   */
  size_t findStartCode(const uint8_t *data, size_t len, size_t from) {
    for (size_t i = from; i + 2 < len; i++) {
      if (data[i] == 0 && data[i + 1] == 0 &&
          (data[i + 2] == 1 ||
           (i + 3 < len && data[i + 2] == 0 && data[i + 3] == 1))) {
        return i;
      }
    }
    return len;
  }

  /// Splits an Annex-B buffer into its individual NAL units (start codes
  /// stripped), recording each one's offset/length into `data`.
  void extractNALs(const uint8_t *data, size_t len, Vector<size_t> &offsets,
                   Vector<size_t> &lengths) {
    size_t i = 0;
    while (i < len) {
      size_t start = findStartCode(data, len, i);
      if (start == len) break;

      size_t scSize = (data[start + 2] == 1) ? 3 : 4;
      size_t nalStart = start + scSize;
      if (nalStart >= len) break;

      size_t next = findStartCode(data, len, nalStart);
      size_t nalEnd = next;  // findStartCode already returns len if none found

      size_t nalLen = nalEnd - nalStart;
      if (nalLen > 0) {
        offsets.push_back(nalStart);
        lengths.push_back(nalLen);
      }
      i = nalEnd;
    }
  }

  void cacheSps(const uint8_t *nal, size_t len) {
    m_sps.resize(len);
    memcpy(m_sps.data(), nal, len);
    if (len >= 4 && p_format != nullptr) {
      char hex[7];
      snprintf(hex, sizeof(hex), "%02X%02X%02X", nal[1], nal[2], nal[3]);
      p_format->setProfileLevelId(hex);
    }
    updateSpropParameterSets();
  }

  void cachePps(const uint8_t *nal, size_t len) {
    m_pps.resize(len);
    memcpy(m_pps.data(), nal, len);
    updateSpropParameterSets();
  }

  void updateSpropParameterSets() {
    if (p_format == nullptr || m_sps.empty() || m_pps.empty()) return;
    String spsB64 = base64Encode(m_sps.data(), m_sps.size());
    String ppsB64 = base64Encode(m_pps.data(), m_pps.size());
    p_format->setSpropParameterSets(spsB64.c_str(), ppsB64.c_str());
  }

  static String base64Encode(const uint8_t *data, size_t len) {
    String out;
    out.reserve(4 * ((len + 2) / 3));
    size_t i = 0;
    while (i < len) {
      uint32_t octetA = i < len ? data[i++] : 0;
      uint32_t octetB = i < len ? data[i++] : 0;
      uint32_t octetC = i < len ? data[i++] : 0;
      uint32_t triple = (octetA << 16) | (octetB << 8) | octetC;
      out += encoding_table[(triple >> 18) & 0x3F];
      out += encoding_table[(triple >> 12) & 0x3F];
      out += encoding_table[(triple >> 6) & 0x3F];
      out += encoding_table[triple & 0x3F];
    }
    int padding = mod_table[len % 3];
    for (int p = 0; p < padding; p++) {
      out.setCharAt(out.length() - 1 - p, '=');
    }
    return out;
  }

  /**
   * @brief Queue one NAL unit as a Single NAL Unit packet, or split it into
   * FU-A fragments (RFC 6184 §5.6/§5.8) if it is larger than
   * m_maxFragmentSize.
   * @param isLastOfFrame True if this is the last NAL unit of the access
   * unit - propagated to its last fragment's isLast flag.
   */
  void emitNal(const uint8_t *nal, size_t nalLen, bool isLastOfFrame) {
    if (nalLen == 0) return;

    if (nalLen <= m_maxFragmentSize) {
      appendFragment(nal, nalLen, isLastOfFrame);
      return;
    }

    // FU-A fragmentation
    uint8_t nalHeader = nal[0];
    uint8_t nalType = nalHeader & 0x1F;
    uint8_t nri = nalHeader & 0x60;

    if (m_maxFragmentSize <= 2) {
      LOGE("H264RtpEncoder: maxFragmentSize too small for FU-A fragmentation");
      return;
    }
    size_t maxPayload = m_maxFragmentSize - 2;

    size_t pos = 1;  // skip the original NAL header; FU-A carries its own
    bool start = true;
    while (pos < nalLen) {
      size_t chunk = nalLen - pos;
      if (chunk > maxPayload) chunk = maxPayload;
      bool end = (pos + chunk >= nalLen);

      m_scratch.resize(2 + chunk);
      m_scratch[0] = nri | 28;  // FU indicator: NRI + Type=28 (FU-A)
      m_scratch[1] = (start ? 0x80 : 0) | (end ? 0x40 : 0) | nalType;  // FU header
      memcpy(m_scratch.data() + 2, nal + pos, chunk);

      appendFragment(m_scratch.data(), m_scratch.size(), end && isLastOfFrame);

      pos += chunk;
      start = false;
    }
  }
};

}  // namespace audio_tools
