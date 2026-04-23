/*
 * Author: Phil Schatzmann
 *
 * JPEG RTP Encoder for RTSP Video Streaming
 * Implements RFC 2435 compliant JPEG over RTP fragmentation
 */

#pragma once

#include "AudioTools/AudioCodecs/AudioCodecsBase.h"
#include "RTSPFormat.h"

namespace audio_tools {

/**
 * @brief JPEG RTP Encoder - Handles JPEG Frame Processing for RTP Streaming
 *
 * This encoder implements the complete JPEG frame processing pipeline for
 * RTP streaming as per RFC 2435:
 *
 * 1. Takes JPEG frames (complete or raw data to encode)
 * 2. Removes standard JPEG headers (optional optimization)
 * 3. Splits large frames into RTP-sized chunks
 * 4. Outputs RTP-ready packets with:
 *    - Proper JPEG RTP headers with fragment offsets
 *    - Same timestamp for all fragments of a frame
 *    - Marker bit set only on the last packet of each frame
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
class JPEGRtpEncoder : public AudioEncoder {
 public:
  JPEGRtpEncoder() = default;
  
  /**
   * @brief Constructor with maximum fragment size
   * @param maxFragmentSize Maximum payload size per RTP packet (default: 1400 bytes)
   */
  JPEGRtpEncoder(size_t maxFragmentSize) : m_maxFragmentSize(maxFragmentSize) {}

  /// Set the maximum fragment size for RTP packets
  void setMaxFragmentSize(size_t size) { m_maxFragmentSize = size; }
  
  /// Enable/disable JPEG header stripping optimization
  void setStripJpegHeaders(bool strip) { m_stripHeaders = strip; }

  bool begin() override {
    LOGI("Starting JPEG RTP Encoder");
    m_isStarted = true;
    m_currentFrame.clear();
    m_fragmentOffset = 0;
    m_frameComplete = true;
    return true;
  }

  void end() override {
    LOGI("Stopping JPEG RTP Encoder");
    m_isStarted = false;
    m_currentFrame.clear();
  }

  void setOutput(Stream &outputStream) {
    p_stream = &outputStream;
  }

  void setAudioInfo(AudioInfo info) override {
    // For video encoder, we can ignore audio info
    // Video parameters come from RTSPFormat
  }

  const char* mime() override {
    return "video/jpeg";
  }

  /**
   * @brief Process JPEG frame data and output RTP fragments
   * 
   * @param data Pointer to JPEG frame data
   * @param len Length of JPEG frame data
   * @return Number of bytes processed
   */
  size_t write(const uint8_t *data, size_t len) override {
    if (!m_isStarted || p_stream == nullptr || len == 0) {
      return 0;
    }

    LOGD("JPEGRtpEncoder: Processing %d bytes of JPEG data", len);

    // Accumulate complete JPEG frame
    if (m_frameComplete) {
      m_currentFrame.clear();
      m_fragmentOffset = 0;
      m_frameComplete = false;
    }

    // Add data to current frame buffer
    for (size_t i = 0; i < len; i++) {
      uint8_t byte = data[i];
      m_currentFrame.push_back(byte);
    }

    // Check if we have a complete JPEG frame
    if (isCompleteJpegFrame()) {
      processCompleteFrame();
      m_frameComplete = true;
    }

    return len;
  }

  operator bool() override {
    return m_isStarted;
  }

 protected:
  Stream *p_stream = nullptr;
  bool m_isStarted = false;
  size_t m_maxFragmentSize = 1400;  // Typical MTU minus IP/UDP/RTP headers
  bool m_stripHeaders = false;      // JPEG header stripping optimization
  
  // Frame processing state
  Vector<uint8_t> m_currentFrame;
  size_t m_fragmentOffset = 0;
  bool m_frameComplete = true;

  /**
   * @brief Check if current buffer contains a complete JPEG frame
   * @return true if frame is complete (ends with 0xFF 0xD9)
   */
  bool isCompleteJpegFrame() {
    if (m_currentFrame.size() < 2) return false;
    
    // Look for JPEG end marker (0xFF 0xD9)
    size_t size = m_currentFrame.size();
    return (m_currentFrame[size-2] == 0xFF && m_currentFrame[size-1] == 0xD9);
  }

  /**
   * @brief Process a complete JPEG frame and fragment it for RTP transmission
   */
  void processCompleteFrame() {
    LOGI("Processing complete JPEG frame: %d bytes", m_currentFrame.size());
    
    uint8_t *frameData = m_currentFrame.data();
    size_t frameSize = m_currentFrame.size();
    size_t processedOffset = 0;

    // Optional: Strip standard JPEG headers for bandwidth optimization
    if (m_stripHeaders) {
      size_t headerSize = findJpegDataStart();
      if (headerSize > 0) {
        frameData += headerSize;
        frameSize -= headerSize;
        LOGD("Stripped %d bytes of JPEG headers", headerSize);
      }
    }

    // Fragment the frame into RTP packets
    size_t totalFragments = (frameSize + m_maxFragmentSize - 1) / m_maxFragmentSize;
    LOGD("Fragmenting into %d RTP packets", totalFragments);

    for (size_t fragmentIndex = 0; fragmentIndex < totalFragments; fragmentIndex++) {
      size_t fragmentSize = min(m_maxFragmentSize, frameSize - processedOffset);
      bool isLastFragment = (fragmentIndex == totalFragments - 1);
      
      // Create RTP packet with JPEG fragment
      createRtpFragment(frameData + processedOffset, fragmentSize, 
                       processedOffset, isLastFragment);
      
      processedOffset += fragmentSize;
    }

    LOGI("JPEG frame fragmentation complete: %d fragments sent", totalFragments);
  }

  /**
   * @brief Find the start of actual JPEG image data (after headers)
   * @return Offset to image data, or 0 if headers should not be stripped
   */
  size_t findJpegDataStart() {
    // Look for Start of Scan (0xFF 0xDA) marker
    for (size_t i = 0; i < m_currentFrame.size() - 1; i++) {
      if (m_currentFrame[i] == 0xFF && m_currentFrame[i+1] == 0xDA) {
        // Skip the SOS marker and length field to get to actual image data
        if (i + 4 < m_currentFrame.size()) {
          size_t sosLength = (m_currentFrame[i+2] << 8) | m_currentFrame[i+3];
          return i + 2 + sosLength;
        }
      }
    }
    return 0; // Don't strip if we can't find proper markers
  }

  /**
   * @brief Create and output an RTP packet fragment
   * 
   * @param fragmentData Pointer to fragment payload data
   * @param fragmentSize Size of fragment payload
   * @param fragmentOffset Byte offset of this fragment within the frame
   * @param isLastFragment True if this is the last fragment of the frame
   */
  void createRtpFragment(const uint8_t *fragmentData, size_t fragmentSize,
                        size_t fragmentOffset, bool isLastFragment) {
    
    // Calculate total packet size: JPEG RTP header + payload
    const size_t JPEG_RTP_HEADER_SIZE = 8;
    size_t totalPacketSize = JPEG_RTP_HEADER_SIZE + fragmentSize;
    
    // Create packet buffer
    Vector<uint8_t> packet;
    packet.resize(totalPacketSize);
    
    // Build JPEG RTP header (RFC 2435)
    buildJpegRtpHeader(packet.data(), fragmentOffset);
    
    // Copy fragment payload
    memcpy(packet.data() + JPEG_RTP_HEADER_SIZE, fragmentData, fragmentSize);
    
    // Special marker for RTP layer to handle timestamp and marker bit
    if (isLastFragment) {
      // Add special marker to indicate this is the last fragment
      // The RTP layer will set the marker bit appropriately
      packet.push_back(0xFF); // Special end-of-frame marker
      packet.push_back(0xEF); // "End Frame" marker
    }
    
    // Output the packet to the stream
    size_t written = p_stream->write(packet.data(), packet.size());
    LOGD("RTP fragment sent: offset=%d, size=%d, last=%s, written=%d",
         fragmentOffset, fragmentSize, isLastFragment ? "yes" : "no", written);
  }

  /**
   * @brief Build JPEG RTP header per RFC 2435
   * 
   * @param buffer Buffer to write header to (must be at least 8 bytes)
   * @param fragmentOffset Byte offset of this fragment within the frame
   */
  void buildJpegRtpHeader(uint8_t *buffer, size_t fragmentOffset) {
    memset(buffer, 0, 8);
    
    // Type-specific field (0 for baseline JPEG)
    buffer[0] = 0;
    
    // Fragment offset (24-bit, network byte order)
    buffer[1] = (fragmentOffset >> 16) & 0xFF;
    buffer[2] = (fragmentOffset >> 8) & 0xFF;
    buffer[3] = fragmentOffset & 0xFF;
    
    // Type (JPEG type - 0 for baseline)
    buffer[4] = 0;
    
    // Q (quantization table ID - 128+ means standard tables)
    buffer[5] = 128;
    
    // Width/Height (in 8-pixel blocks) - these should come from format
    // For now, use default values - this should be configured externally
    buffer[6] = 80;  // 640/8
    buffer[7] = 60;  // 480/8
  }
};

} // namespace audio_tools