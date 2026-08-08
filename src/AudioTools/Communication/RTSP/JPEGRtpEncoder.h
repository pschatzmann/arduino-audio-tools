/*
 * Author: Phil Schatzmann
 *
 * JPEG RTP Encoder for RTSP Video Streaming
 * Implements RFC 2435 compliant JPEG over RTP fragmentation
 */

#pragma once

#include "AudioTools/AudioCodecs/AudioCodecsBase.h"
#include "AudioTools/CoreAudio/AudioBasic/Collections/Vector.h"
#include "RTSPFormat.h"
#include "RTSPFragmentQueue.h"
#include "RTSPVideoEncoder.h"

namespace audio_tools {

/**
 * @brief JPEG RTP Encoder - Fragments JPEG frames per RFC 2435 for RTSP
 * video streaming.
 *
 * This class plays both roles needed to get a JPEG frame onto the wire:
 * - As an AudioEncoder, write() accepts complete JPEG frames (e.g. straight
 *   from a camera).
 * - As an IMediaSource, packetSize()/readBytes() hand out the resulting RTP
 *   payloads one fragment at a time, each one already carrying its RFC 2435
 *   header (and, on the first fragment of a frame, an embedded
 *   quantization-table header).
 *
 * Keeping fragmentation and header construction in one place means the RTP
 * transport never has to re-derive fragment boundaries or rebuild a JPEG
 * header of its own - it only wraps each queued fragment in the generic RTP
 * header (sequence number, shared timestamp, marker bit set once
 * packetSize() reports the queue empty).
 *
 * Per RFC 2435, the payload must be the raw entropy-coded scan data only;
 * SOI/APPn/DQT/SOF/SOS markers are stripped and reconstructed by the
 * receiver from the RTP header fields, so header stripping is not optional.
 *
 * @note Restart intervals (DRI) are not supported.
 *
 * Usage with RTSPOutput:
 * ```cpp
 * RTSPFormatMJPEG mjpegFormat(640, 480, 30.0f);
 * JPEGRtpEncoder jpegEncoder;
 * RTSPOutput<RTSPPlatformWiFi> rtspOutput(mjpegFormat, jpegEncoder);
 * ```
 *
 * @ingroup rtsp
 * @ingroup codecs
 * @author Phil Schatzmann
 */
class JPEGRtpEncoder : public RTSPVideoEncoder, protected RTSPFragmentQueue {
 public:
  JPEGRtpEncoder() = default;

  /**
   * @brief Constructor with maximum fragment size
   * @param maxFragmentSize Maximum RTP payload size per packet, including
   * the RFC 2435 header (default: 1400 bytes)
   */
  JPEGRtpEncoder(size_t maxFragmentSize) : m_maxFragmentSize(maxFragmentSize) {}

  /// Set the maximum RTP payload size per packet (header + data)
  void setMaxFragmentSize(size_t size) { m_maxFragmentSize = size; }

  /// Maximum number of complete frames buffered when the consumer falls behind
  void setMaxQueuedFrames(size_t frames) { m_maxQueuedFrames = frames; }

  /// Associates the RTSPFormat (e.g. RTSPFormatMJPEG) that supplies width/height
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
    LOGI("Starting JPEG RTP Encoder");
    m_isStarted = true;
    m_currentFrame.clear();
    m_frameComplete = true;
    clearQueue();
    return true;
  }

  void end() override {
    LOGI("Stopping JPEG RTP Encoder");
    m_isStarted = false;
    m_currentFrame.clear();
    clearQueue();
  }

  void setAudioInfo(AudioInfo info) override {
    // Video encoder: dimensions/framerate come from the RTSPFormat, not
    // AudioInfo
    (void)info;
  }

  const char *mime() override { return "video/jpeg"; }

  /**
   * @brief Process JPEG frame data and queue the resulting RTP fragments
   *
   * @param data Pointer to JPEG frame data
   * @param len Length of JPEG frame data
   * @return Number of bytes processed
   */
  size_t write(const uint8_t *data, size_t len) override {
    if (!m_isStarted || len == 0) {
      return 0;
    }

    if (m_frameComplete) {
      m_currentFrame.clear();
      m_frameComplete = false;
    }

    size_t oldSize = m_currentFrame.size();
    m_currentFrame.resize(oldSize + len);
    memcpy(m_currentFrame.data() + oldSize, data, len);

    if (isCompleteJpegFrame()) {
      processCompleteFrame();
      m_frameComplete = true;
    }

    return len;
  }

  operator bool() override { return m_isStarted; }

 protected:
  RTSPFormat *p_format = nullptr;
  RTSPFormatMJPEG default_format;  // fallback so getFormat() is never null

  bool m_isStarted = false;
  size_t m_maxFragmentSize = 1400;  // Typical MTU minus IP/UDP/RTP headers

  // Frame accumulation state
  Vector<uint8_t> m_currentFrame;
  bool m_frameComplete = true;

  // Reused scratch buffer for building one fragment at a time
  Vector<uint8_t> m_scratch;

  /**
   * @brief Check if current buffer contains a complete JPEG frame
   * @return true if frame is complete (ends with 0xFF 0xD9)
   */
  bool isCompleteJpegFrame() {
    size_t size = m_currentFrame.size();
    if (size < 4) return false;
    return (m_currentFrame[size - 2] == 0xFF && m_currentFrame[size - 1] == 0xD9);
  }

  /**
   * @brief Find the start of the entropy-coded scan data, i.e. the first
   * byte after the SOS (Start Of Scan) marker segment. RFC 2435 requires
   * everything before this point (SOI/APPn/DQT/SOF/SOS) to be stripped from
   * the RTP payload.
   * @return Offset to the scan data, or 0 if no SOS marker was found
   */
  size_t findJpegDataStart() {
    size_t size = m_currentFrame.size();
    if (size < 4) return 0;
    for (size_t i = 0; i + 4 <= size; i++) {
      if (m_currentFrame[i] == 0xFF && m_currentFrame[i + 1] == 0xDA) {
        size_t sosLength = (m_currentFrame[i + 2] << 8) | m_currentFrame[i + 3];
        size_t dataStart = i + 2 + sosLength;
        return dataStart <= size ? dataStart : 0;
      }
    }
    return 0;
  }

  /**
   * @brief Extract the (8-bit precision) quantization tables 0 and 1 from
   * the JPEG DQT segments found in [data, data+headerLen). RFC 2435 expects
   * table 0 (luma) followed by table 1 (chroma), each 64 bytes in zigzag
   * order exactly as stored in the JPEG - no reordering needed.
   * @return Number of bytes written to out (0, 64 or 128), 0 if no usable
   * 8-bit-precision table was found
   */
  int extractQuantTables(const uint8_t *data, size_t headerLen, uint8_t *out) {
    uint8_t table0[64];
    uint8_t table1[64];
    bool has0 = false, has1 = false;

    size_t i = 2;  // skip SOI (0xFFD8)
    while (i + 4 <= headerLen) {
      if (data[i] != 0xFF) {
        ++i;
        continue;
      }
      uint8_t marker = data[i + 1];
      if (marker == 0xD8 || marker == 0x01 || (marker >= 0xD0 && marker <= 0xD7)) {
        i += 2;
        continue;
      }
      size_t segLen = (data[i + 2] << 8) | data[i + 3];
      if (segLen < 2 || i + 2 + segLen > headerLen) break;

      if (marker == 0xDB) {  // DQT
        size_t p = i + 4;
        size_t segEnd = i + 2 + segLen;
        while (p < segEnd) {
          uint8_t pqtq = data[p++];
          uint8_t precision = pqtq >> 4;
          uint8_t id = pqtq & 0x0F;
          size_t tableBytes = precision == 0 ? 64 : 128;
          if (p + tableBytes > segEnd) break;
          if (precision == 0) {
            if (id == 0 && !has0) {
              memcpy(table0, data + p, 64);
              has0 = true;
            } else if (id == 1 && !has1) {
              memcpy(table1, data + p, 64);
              has1 = true;
            }
          }
          p += tableBytes;
        }
      } else if (marker == 0xDA) {
        break;  // reached scan data, no more tables before it
      }
      i += 2 + segLen;
    }

    int len = 0;
    if (has0) {
      memcpy(out, table0, 64);
      len += 64;
    }
    if (has1) {
      memcpy(out + len, table1, 64);
      len += 64;
    }
    return len;
  }

  /**
   * @brief Determine the RFC 2435 Type (0 = 4:2:2, 1 = 4:2:0) from the
   * JPEG's SOF0/SOF1 marker so the receiver reconstructs the correct chroma
   * subsampling. Defaults to 0 (4:2:2) if it cannot be determined.
   */
  uint8_t detectJpegType(const uint8_t *data, size_t headerLen) {
    size_t i = 2;
    while (i + 4 <= headerLen) {
      if (data[i] != 0xFF) {
        ++i;
        continue;
      }
      uint8_t marker = data[i + 1];
      if (marker == 0xD8 || marker == 0x01 || (marker >= 0xD0 && marker <= 0xD7)) {
        i += 2;
        continue;
      }
      size_t segLen = (data[i + 2] << 8) | data[i + 3];
      if (segLen < 2 || i + 2 + segLen > headerLen) break;

      if (marker == 0xC0 || marker == 0xC1) {  // SOF0 / SOF1 (baseline)
        size_t p = i + 4;
        size_t segEnd = i + 2 + segLen;
        // layout from p: precision(1) Y(2) X(2) Nf(1) [Cid(1) HV(1) Tq(1)]...
        if (p + 8 <= segEnd) {
          uint8_t hv = data[p + 7];
          uint8_t h = hv >> 4;
          uint8_t v = hv & 0x0F;
          if (h == 2 && v == 1) return 0;  // 4:2:2
          if (h == 2 && v == 2) return 1;  // 4:2:0
          LOGW(
              "JPEGRtpEncoder: unsupported chroma subsampling H=%d V=%d; "
              "assuming 4:2:2",
              h, v);
        }
        return 0;
      }
      if (marker == 0xDA) break;
      i += 2 + segLen;
    }
    LOGW("JPEGRtpEncoder: SOF0 marker not found; assuming 4:2:2 subsampling");
    return 0;
  }

  void buildJpegRtpHeader(uint8_t *buffer, size_t fragmentOffset, int width,
                          int height, uint8_t type, uint8_t qValue) {
    buffer[0] = 0;  // Type-specific field (0 for baseline JPEG)
    buffer[1] = (fragmentOffset >> 16) & 0xFF;
    buffer[2] = (fragmentOffset >> 8) & 0xFF;
    buffer[3] = fragmentOffset & 0xFF;
    buffer[4] = type;
    buffer[5] = qValue;
    buffer[6] = (uint8_t)((width / 8) & 0xFF);
    buffer[7] = (uint8_t)((height / 8) & 0xFF);
  }

  /// RFC 2435 Quantization Table header (only present on the first fragment
  /// of a frame when Q >= 128)
  void writeQuantTableHeader(uint8_t *buffer, const uint8_t *qTable, int qTableLen) {
    buffer[0] = 0;  // MBZ
    buffer[1] = 0;  // Precision: 0 = 8-bit for all tables
    buffer[2] = (qTableLen >> 8) & 0xFF;
    buffer[3] = qTableLen & 0xFF;
    memcpy(buffer + 4, qTable, qTableLen);
  }

  /**
   * @brief Strip the JPEG headers, fragment the scan data and queue one RFC
   * 2435 RTP payload per fragment.
   */
  void processCompleteFrame() {
    if (p_format == nullptr) {
      LOGE("JPEGRtpEncoder: no RTSPFormat configured (call setFormat()); dropping frame");
      return;
    }

    size_t headerSize = findJpegDataStart();
    if (headerSize == 0 || headerSize >= m_currentFrame.size()) {
      LOGW("JPEGRtpEncoder: could not locate JPEG scan data (SOS marker); dropping frame");
      return;
    }

    uint8_t qTable[128];
    int qTableLen = extractQuantTables(m_currentFrame.data(), headerSize, qTable);
    uint8_t qValue = qTableLen > 0 ? 255 : 127;
    if (qTableLen == 0) {
      LOGW(
          "JPEGRtpEncoder: could not extract quantization tables from frame; "
          "using approximate standard tables (Q=%d)",
          qValue);
    }
    uint8_t type = detectJpegType(m_currentFrame.data(), headerSize);

    VideoInfo vi = p_format->videoInfo();
    int width = vi.width;
    int height = vi.height;

    const uint8_t *frameData = m_currentFrame.data() + headerSize;
    size_t frameSize = m_currentFrame.size() - headerSize;
    size_t processedOffset = 0;
    int fragmentCount = 0;

    while (processedOffset < frameSize) {
      bool isFirst = (processedOffset == 0);
      size_t qLen = isFirst ? (size_t)qTableLen : 0;
      size_t qHeaderLen = qLen > 0 ? 4 + qLen : 0;

      if (m_maxFragmentSize <= 8 + qHeaderLen) {
        LOGE("JPEGRtpEncoder: maxFragmentSize too small to fit RTP/JPEG headers");
        return;
      }
      size_t maxData = m_maxFragmentSize - 8 - qHeaderLen;
      size_t chunk = frameSize - processedOffset;
      if (chunk > maxData) chunk = maxData;
      bool isLast = (processedOffset + chunk >= frameSize);

      m_scratch.resize(8 + qHeaderLen + chunk);
      buildJpegRtpHeader(m_scratch.data(), processedOffset, width, height, type, qValue);
      if (qHeaderLen > 0) {
        writeQuantTableHeader(m_scratch.data() + 8, qTable, qTableLen);
      }
      memcpy(m_scratch.data() + 8 + qHeaderLen, frameData + processedOffset, chunk);

      appendFragment(m_scratch.data(), m_scratch.size(), isLast);

      processedOffset += chunk;
      ++fragmentCount;
    }

    LOGI("JPEGRtpEncoder: queued %d RTP fragments for %u byte frame",
         fragmentCount, (unsigned)frameSize);
  }
};

}  // namespace audio_tools
