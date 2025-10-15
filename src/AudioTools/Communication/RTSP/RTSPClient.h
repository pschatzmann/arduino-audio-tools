/*
 * Author: Phil Schatzmann
 *
 * Based on Micro-RTSP library:
 * https://github.com/geeksville/Micro-RTSP
 * https://github.com/Tomp0801/Micro-RTSP-Audio
 *
 */
#pragma once
#include <Arduino.h>
#include <IPAddress.h>

#include "AudioTools/AudioCodecs/CodecNetworkFormat.h"
#include "AudioTools/AudioCodecs/MultiDecoder.h"
#include "AudioTools/CoreAudio//BaseStream.h"
#include "AudioTools/CoreAudio/AudioBasic/Collections/Vector.h"
#include "AudioTools/CoreAudio/Buffers.h"
#include "AudioTools/CoreAudio/ResampleStream.h"

namespace audio_tools {

/**
 * @brief Efficient RTSP client for UDP/RTP audio with decoder pipeline.
 *
 * Establishes an RTSP session (OPTIONS, DESCRIBE, SETUP/UDP, PLAY), binds a
 * local UDP RTP port and receives RTP audio packets. The payload of each RTP
 * packet is forwarded to an internal MultiDecoder. For raw PCM over RTP
 * (e.g. L16) a DecoderNetworkFormat is used to convert network byte order
 * into host format before writing to the configured output. For compressed
 * formats, register decoders with addDecoder().
 *
 * Usage:
 * - Construct and set an output via setOutput()
 * - Call begin(address, port)
 * - In your loop, call  copy() to push RTP payloads to decoders
 * - Optionally control streaming via setActive(true/false)
 * - Optionally define resampling factor to prevent buffer over/underflows
 *
 * Template parameters:
 * - TcpClient: TCP client type (e.g: WiFiClient)
 * - UdpSocket: UDP socket type (e.g: WiFiUDP)
 *
 * @ingroup rtsp
 * @author Phil Schatzmann
 */
template <typename TcpClient, typename UdpSocket>
class RTSPClient : public AudioInfoSource, public AudioInfoSupport {
 public:
  RTSPClient() {
    // convert network format to little endian
    m_multi_decoder.addDecoder(m_decoder_net, "audio/L16");
    // convert to 16 bit
    m_multi_decoder.addDecoder(m_decoder_l8, "audio/L8");
    // Start resampler; it will receive AudioInfo later via setAudioInfo
    m_resampler.begin();
    m_multi_decoder.setOutput(m_resampler);
  }

  /**
   * @brief Construct with an AudioOutput as decoding sink.
   */
  RTSPClient(AudioOutput& out) : RTSPClient() { setOutput(out); }
  /**
   * @brief Construct with an AudioStream as decoding sink.
   */
  RTSPClient(AudioStream& out) : RTSPClient() { setOutput(out); }
  /**
   * @brief Construct with a generic Print sink.
   */
  RTSPClient(Print& out) : RTSPClient() { setOutput(out); }
  /**
   * @brief Define decoding sink as AudioOutput.
   */
  void setOutput(AudioOutput& out) { m_resampler.setOutput(out); }
  /**
   * @brief Define decoding sink as AudioStream.
   */
  void setOutput(AudioStream& out) { m_resampler.setStream(out); }
  /**
   * @brief Define decoding sink as Print.
   */
  void setOutput(Print& out) { m_resampler.setOutput(out); }

  /**
   * @brief Set resampling factor to stabilize buffers and playback.
   * 1.0 means no resampling. factor > 1.0 speeds up (upsamples),
   * factor < 1.0 slows down (downsamples). Useful to compensate clock drift
   * between sender and receiver to prevent buffer overflows/underflows.
   * Internally mapped to step size as step = 1.0 / factor.
   * @note This can be used to prevent buffer overflows/underflows
   */
  void setResampleFactor(float factor) {
    if (factor <= 0.0f) factor = 1.0f;
    float step = 1.0f / factor;
    m_resampleStep = step;
    m_resampler.setStepSize(step);
    // Always route via resampler; factor 1.0 is pass-through
  }
  /**
   * @brief Set idle backoff delay (ms) for zero-return cases.
   * Used in available() and copy() to avoid busy loops.
   */
  void setIdleDelay(uint32_t ms) { m_idleDelayMs = ms; }

  /**
   * @brief Set number of TCP connect retries (default 2).
   */
  void setConnectRetries(uint8_t retries) { m_connectRetries = retries; }

  /**
   * @brief Set delay between connect retries in ms (default 500ms).
   */
  void setConnectRetryDelayMs(uint32_t ms) { m_connectRetryDelayMs = ms; }

  /**
   * @brief Set timeout (ms) for reading RTSP response headers.
   * Increase if your server responds slowly. Default 3000ms.
   */
  void setHeaderTimeoutMs(uint32_t ms) { m_headerTimeoutMs = ms; }

  /**
   * @brief Set additional RTP payload offset in bytes.
   * Some payloads embed a small header before the actual audio data
   * (e.g., RFC2250 4-byte header for MP3). This offset is added after
   * the RTP header and any CSRC entries.
   */
  void setPayloadOffset(uint8_t bytes) { m_payloadOffset = bytes; }
  /**
   * @brief Start RTSP session and UDP RTP reception.
   * @param addr RTSP server IP address
   * @param port RTSP server port (typically 554)
   * @param path Optional path appended to the RTSP URL (e.g. "stream1").
   *             If provided, the base URL becomes
   *             rtsp://<ip>:<port>/<path>/
   * @return true on success
   */
  bool begin(IPAddress addr, uint16_t port, const char* path = nullptr) {
    resetState();
    m_addr = addr;
    m_port = port;

    if (m_tcp.connected()) m_tcp.stop();
    LOGI("RTSPClient: connecting to %u.%u.%u.%u:%u", m_addr[0], m_addr[1],
         m_addr[2], m_addr[3], (unsigned)m_port);
    // m_tcp.setTimeout(m_headerTimeoutMs / 1000);
    bool connected = false;
    for (uint8_t attempt = 0; attempt <= m_connectRetries; ++attempt) {
      if (m_tcp.connect(m_addr, m_port)) {
        connected = true;
        break;
      }
      LOGW("RTSPClient: connect attempt %u failed", (unsigned)(attempt + 1));
      if (attempt < m_connectRetries) delay(m_connectRetryDelayMs);
    }
    if (!connected) {
      LOGE("RTSPClient: TCP connect failed");
      return false;
    }
    m_tcp.setNoDelay(true);

    // Build base URL and track URL
    buildUrls(path);

    // CSeq starts at 1
    m_cseq = 1;

    // OPTIONS
    LOGI("OPTIONS");
    int retry = m_connectRetries;
    while (!sendSimpleRequest("OPTIONS", m_baseUrl, nullptr, 0, m_hdrBuf,
                              sizeof(m_hdrBuf), nullptr, 0)) {
      if (--retry == 0) {
        return fail("OPTIONS failed");
      } else {
        LOGW("RTSPClient: retrying OPTIONS");
        delay(800);
      }
    }

    // DESCRIBE
    LOGI("DESCRIBE");
    const char* describeExtra = "Accept: application/sdp\r\n";
    if (!sendSimpleRequest("DESCRIBE", m_baseUrl, describeExtra,
                           strlen(describeExtra), m_hdrBuf, sizeof(m_hdrBuf),
                           m_bodyBuf, sizeof(m_bodyBuf)))
      return fail("DESCRIBE failed");

    // Parse SDP (rtpmap) to capture payload and encoding
    parseSdp(m_bodyBuf);
    // Parse Content-Base for absolute/relative control resolution
    parseContentBaseFromHeaders(m_hdrBuf);
    // Parse a=control and build the correct track URL for SETUP
    parseControlFromSdp(m_bodyBuf);
    buildTrackUrlFromBaseAndControl();
    LOGI("RTSPClient: SDP control='%s' content-base='%s'", m_sdpControl,
         m_contentBase);
    LOGI("RTSPClient: SETUP url: %s", m_trackUrl);

    // Prepare UDP (client_port)
    if (!openUdpPorts()) return fail("UDP bind failed");

    // SETUP with client_port pair
    char transportHdr[128];
    snprintf(transportHdr, sizeof(transportHdr),
             "Transport: RTP/AVP;unicast;client_port=%u-%u\r\n",
             (unsigned)m_clientRtpPort, (unsigned)(m_clientRtpPort + 1));
    if (!sendSimpleRequest("SETUP", m_trackUrl, transportHdr,
                           strlen(transportHdr), m_hdrBuf, sizeof(m_hdrBuf),
                           nullptr, 0)) {
      // Fallback: some servers require explicit UDP in transport profile
      snprintf(transportHdr, sizeof(transportHdr),
               "Transport: RTP/AVP/UDP;unicast;client_port=%u-%u\r\n",
               (unsigned)m_clientRtpPort, (unsigned)(m_clientRtpPort + 1));
      if (!sendSimpleRequest("SETUP", m_trackUrl, transportHdr,
                             strlen(transportHdr), m_hdrBuf, sizeof(m_hdrBuf),
                             nullptr, 0)) {
        return fail("SETUP failed");
      }
    }

    // Parse Session and server_port from last headers
    parseSessionFromHeaders(m_hdrBuf);
    parseServerPortsFromHeaders(m_hdrBuf);
    if (m_sessionId[0] == '\0') return fail("Missing Session ID");

    // Prime UDP path to server RTP port (helps some networks/servers)
    primeUdpPath();

    // PLAY
    LOGI("PLAY");
    char sessionHdr[128];
    snprintf(sessionHdr, sizeof(sessionHdr), "Session: %s\r\n", m_sessionId);
    if (!sendSimpleRequest("PLAY", m_baseUrl, sessionHdr, strlen(sessionHdr),
                           m_hdrBuf, sizeof(m_hdrBuf), nullptr, 0)) {
      // Some servers start streaming RTP immediately but delay/omit PLAY
      // response Treat PLAY as successful if RTP arrives shortly after sending
      // PLAY
      if (sniffUdpFor(1500)) {
        LOGW("RTSPClient: proceeding without PLAY response (RTP detected)");
      } else {
        return fail("PLAY failed");
      }
    }

    m_started = true;
    m_isPlaying = true;
    m_lastKeepaliveMs = millis();
    return true;
  }

  /// returns true when streaming is active and a decoder is configured and we
  /// have data
  operator bool() { return m_started && mime() != nullptr && available() > 0; }

  /**
   * @brief Stop streaming and close RTSP/UDP sockets.
   */
  void end() {
    if (m_started) {
      // best-effort TEARDOWN
      if (m_tcp.connected()) {
        char sessionHdr[128];
        if (m_sessionId[0]) {
          snprintf(sessionHdr, sizeof(sessionHdr), "Session: %s\r\n",
                   m_sessionId);
          sendSimpleRequest("TEARDOWN", m_baseUrl, sessionHdr,
                            strlen(sessionHdr), m_hdrBuf, sizeof(m_hdrBuf),
                            nullptr, 0, /*quiet*/ true);
        }
      }
    }

    if (m_udp_active) {
      m_udp.stop();
    }
    if (m_tcp.connected()) m_tcp.stop();
    m_started = false;
    m_isPlaying = false;
  }

  /**
   * @brief Returns buffered RTP payload bytes available for copy().
   */
  int available() {
    if (!m_started) {
      delay(m_idleDelayMs);
      return 0;
    }
    // keepalive regardless of play state
    maybeKeepalive();
    if (!m_isPlaying) {
      delay(m_idleDelayMs);
      return 0;
    }
    serviceUdp();
    int avail = m_pktBuf.available();
    if (avail == 0) delay(m_idleDelayMs);
    return avail;
  }

  /**
   * @brief Best-effort MIME derived from SDP (e.g. audio/L16, audio/aac).
   * @return MIME string or nullptr if unknown.
   */
  const char* mime() const {
    // Prefer static RTP payload type mapping when available
    switch (m_payloadType) {
      case 0:  // PCMU
        return "audio/PCMU";
      case 3:  // GSM
        return "audio/gsm";
      case 4:  // G723
        return "audio/g723";
      case 5:   // DVI4/8000 (IMA ADPCM)
      case 6:   // DVI4/16000
      case 16:  // DVI4/11025
      case 17:  // DVI4/22050
        return "audio/adpcm";
      case 8:  // PCMA
        return "audio/PCMA";
      case 9:  // G722
        return "audio/g722";
      case 10:  // L16 stereo
      case 11:  // L16 mono
        return "audio/L16";
      case 14:  // MPA (MPEG audio / MP3)
        return "audio/mpeg";
      default:
        break;  // dynamic or unknown; fall back to SDP encoding string
    }
    // Fallback: infer from SDP encoding token
    if (strcasecmp(m_encoding, "L16") == 0) return "audio/L16";
    if (strcasecmp(m_encoding, "L8") == 0) return "audio/L8";
    if (strcasecmp(m_encoding, "PCMU") == 0) return "audio/PCMU";
    if (strcasecmp(m_encoding, "PCMA") == 0) return "audio/PCMA";
    if (strcasecmp(m_encoding, "GSM") == 0) return "audio/gsm";
    if (strcasecmp(m_encoding, "MPA") == 0) return "audio/mpeg";  // MP3
    if (strcasecmp(m_encoding, "MPEG4-GENERIC") == 0) return "audio/aac";
    if (strcasecmp(m_encoding, "OPUS") == 0) return "audio/opus";
    if (strcasecmp(m_encoding, "DVI4") == 0) return "audio/adpcm";  // IMA ADPCM
    return nullptr;
  }

  /**
   * @brief RTP payload type from SDP (0xFF if unknown).
   */
  uint8_t payloadType() const { return m_payloadType; }

  /**
   * @brief Pause or resume playback via RTSP PAUSE/PLAY.
   * @param active true to PLAY, false to PAUSE
   * @return true if command succeeded
   */
  bool setActive(bool active) {
    if (!m_started || !m_tcp.connected() || m_sessionId[0] == '\0')
      return false;
    if (active == m_isPlaying) return true;  // no-op

    char sessionHdr[128];
    snprintf(sessionHdr, sizeof(sessionHdr), "Session: %s\r\n", m_sessionId);
    bool ok;
    if (active) {
      ok = sendSimpleRequest("PLAY", m_baseUrl, sessionHdr, strlen(sessionHdr),
                             m_hdrBuf, sizeof(m_hdrBuf), nullptr, 0);
      if (ok) m_isPlaying = true;
    } else {
      ok = sendSimpleRequest("PAUSE", m_baseUrl, sessionHdr, strlen(sessionHdr),
                             m_hdrBuf, sizeof(m_hdrBuf), nullptr, 0);
      if (ok) {
        m_isPlaying = false;
        // drop any buffered payload
        m_pktBuf.clear();
      }
    }
    return ok;
  }

  /**
   * @brief Register a decoder to be auto-selected for the given MIME.
   * @param mimeType MIME to match
   * @param decoder AudioDecoder instance handling that MIME
   */
  void addDecoder(const char* mimeType, AudioDecoder& decoder) {
    m_multi_decoder.addDecoder(decoder, mimeType);
  }

  /**
   * @brief Copy the next buffered RTP payload into the decoder pipeline.
   * Performs initial decoder selection based on SDP MIME.
   * @return Bytes written to decoder, or 0 if none available.
   */
  size_t copy() {
    if (!m_started) {
      delay(m_idleDelayMs);
      LOGD("not started");
      return 0;
    }
    
    maybeKeepalive();
    
    if (!m_isPlaying) {
      delay(m_idleDelayMs);
      LOGD("not playing");
      return 0;
    }
    
    serviceUdp();

    if (m_pktBuf.isEmpty()) {
      LOGD("no data");
      delay(m_idleDelayMs);
      return 0;
    }

    // On first data, make sure decoder selection and audio info are applied
    if (!m_decoderReady) {
      const char* m = mime();
      if (m) {
        LOGI("Selecting decoder: %s", m);
        // Ensure network format decoder has correct PCM info
        m_multi_decoder.selectDecoder(m);
        m_multi_decoder.setAudioInfo(m_info);
        if (m_multi_decoder.getOutput() != nullptr) {
          m_multi_decoder.begin();  // start decoder only when output is defined
        }
        m_decoderReady = true;
      }
    }

    int n = m_pktBuf.available();
    size_t written = m_multi_decoder.write(m_pktBuf.data(), n);
    LOGI("copy: %d -> %d", (int)n, (int)written);
    m_pktBuf.clearArray(written);
    return written;
  }

  /**
   * @brief Audio info parsed from SDP for raw PCM encodings.
   * @return AudioInfo or default-constructed if not PCM.
   */
  AudioInfo audioInfo() override { return m_multi_decoder.audioInfo(); }

  void setAudioInfo(AudioInfo info) override {
    m_multi_decoder.setAudioInfo(info);
  }

  // AudioInfoSource forwarding: delegate notifications to MultiDecoder
  void addNotifyAudioChange(AudioInfoSupport& bi) override {
    m_multi_decoder.addNotifyAudioChange(bi);
  }
  bool removeNotifyAudioChange(AudioInfoSupport& bi) override {
    return m_multi_decoder.removeNotifyAudioChange(bi);
  }
  void clearNotifyAudioChange() override {
    m_multi_decoder.clearNotifyAudioChange();
  }
  void setNotifyActive(bool flag) { m_multi_decoder.setNotifyActive(flag); }
  bool isNotifyActive() { return m_multi_decoder.isNotifyActive(); }

 protected:
  // Connection
  TcpClient m_tcp;
  UdpSocket m_udp;
  bool m_udp_active = false;
  IPAddress m_addr{};
  uint16_t m_port = 0;

  // RTSP state
  uint32_t m_cseq = 1;
  char m_baseUrl[96] = {0};
  char m_trackUrl[128] = {0};
  char m_contentBase[160] = {0};
  char m_sdpControl[128] = {0};
  char m_sessionId[64] = {0};
  uint16_t m_clientRtpPort = 0;  // even
  uint16_t m_serverRtpPort = 0;  // optional from Transport response
  bool m_started = false;
  bool m_isPlaying = false;
  uint32_t m_lastKeepaliveMs = 0;
  const uint32_t m_keepaliveIntervalMs = 25000;  // 25s

  // Buffers
  SingleBuffer<uint8_t> m_pktBuf{0};
  SingleBuffer<uint8_t> m_tcpCmd{0};
  char m_hdrBuf[1024];
  char m_bodyBuf[1024];

  // Decoder pipeline
  MultiDecoder m_multi_decoder;
  DecoderNetworkFormat m_decoder_net;
  DecoderL8 m_decoder_l8;
  bool m_decoderReady = false;
  uint32_t m_idleDelayMs = 10;
  uint8_t m_payloadOffset = 0;  // extra bytes after RTP header/CSRCs
  uint8_t m_connectRetries = 2;
  uint32_t m_connectRetryDelayMs = 500;
  uint32_t m_headerTimeoutMs = 4000;  // header read timeout

  // Resampling pipeline
  ResampleStream m_resampler;
  float m_resampleStep = 1.0f;
  // Sinks are set directly on the resampler

  // --- RTP/SDP fields ---
  uint8_t m_payloadType = 0xFF;  // unknown by default
  char m_encoding[32] = {0};
  AudioInfo m_info{0, 0, 0};

  void resetState() {
    m_sessionId[0] = '\0';
    m_serverRtpPort = 0;
    m_clientRtpPort = 0;
    m_cseq = 1;
    m_pktBuf.resize(2048);
    m_pktBuf.clear();
    m_decoderReady = false;
    m_udp_active = false;
  }

  void buildUrls(const char* path) {
    snprintf(m_baseUrl, sizeof(m_baseUrl), "rtsp://%u.%u.%u.%u:%u/", m_addr[0],
             m_addr[1], m_addr[2], m_addr[3], (unsigned)m_port);
    if (path && *path) {
      const char* p = path;
      if (*p == '/') ++p;  // skip leading slash
      size_t used = strlen(m_baseUrl);
      size_t avail = sizeof(m_baseUrl) - used - 1;
      if (avail > 0) strncat(m_baseUrl, p, avail);
      // ensure trailing '/'
      used = strlen(m_baseUrl);
      if (used > 0 && m_baseUrl[used - 1] != '/') {
        if (used + 1 < sizeof(m_baseUrl)) {
          m_baseUrl[used] = '/';
          m_baseUrl[used + 1] = '\0';
        }
      }
    }
    snprintf(m_trackUrl, sizeof(m_trackUrl), "%strackID=0", m_baseUrl);
  }

  bool openUdpPorts() {
    // Try a few even RTP ports starting at 5004
    for (uint16_t p = 5004; p < 65000; p += 2) {
      if (m_udp.begin(p)) {
        LOGI("RTSPClient: bound UDP RTP port %u", (unsigned)p);
        m_clientRtpPort = p;
        m_udp_active = true;
        return true;
      }
    }
    return false;
  }

  bool fail(const char* msg) {
    LOGE("RTSPClient: %s", msg);
    end();
    return false;
  }

  void maybeKeepalive() {
    if (!m_started || !m_tcp.connected()) return;
    uint32_t now = millis();
    if (now - m_lastKeepaliveMs < m_keepaliveIntervalMs) return;
    m_lastKeepaliveMs = now;
    char sessionHdr[128];
    if (m_sessionId[0]) {
      snprintf(sessionHdr, sizeof(sessionHdr), "Session: %s\r\n", m_sessionId);
      sendSimpleRequest("OPTIONS", m_baseUrl, sessionHdr, strlen(sessionHdr),
                        m_hdrBuf, sizeof(m_hdrBuf), nullptr, 0, /*quiet*/ true);
    } else {
      sendSimpleRequest("OPTIONS", m_baseUrl, nullptr, 0, m_hdrBuf,
                        sizeof(m_hdrBuf), nullptr, 0, /*quiet*/ true);
    }
  }

  // Compute the RTP payload offset inside a UDP packet
  // Considers fixed RTP header (12 bytes), CSRC count, and configured extra
  // offset
  size_t computeRtpPayloadOffset(const uint8_t* data, size_t length) {
    if (length <= 12) return length;
    size_t offset = 12;
    uint8_t cc = data[0] & 0x0F;  // CSRC count
    offset += cc * 4;
    // Apply any configured additional payload offset (e.g., RFC2250)
    offset += m_payloadOffset;
    return offset;
  }

  void serviceUdp() {
    // Keep RTSP session alive
    maybeKeepalive();

    if (!m_udp_active) {
      LOGE("no UDP");
      return;
    }
    if (m_pktBuf.available() > 0) {
      LOGI("Still have unprocessed data");
      return;  // still have data buffered
    }

    // parse next UDP packet
    int packetSize = m_udp.parsePacket();
    if (packetSize <= 0) {
      LOGD("packet size: %d", packetSize);
      return;
    }

    // Fill buffer
    if ((size_t)packetSize > m_pktBuf.size()) m_pktBuf.resize(packetSize);
    int n = m_udp.read(m_pktBuf.data(), packetSize);
    m_pktBuf.setAvailable(n);
    if (n <= 12) {
      LOGE("packet too small: %d", n);
      return;  // too small to contain RTP
    }

    // Very basic RTP parsing: compute payload offset
    uint8_t* data = m_pktBuf.data();
    size_t payloadOffset = computeRtpPayloadOffset(data, (size_t)n);
    if (payloadOffset >= (size_t)n) {
      LOGW("no payload: %d", n);
    }

    // move payload to beginning for contiguous read
    m_pktBuf.clearArray(payloadOffset);
  }

  void primeUdpPath() {
    if (!m_udp_active) return;
    if (m_serverRtpPort == 0) return;
    // Send a tiny datagram to server RTP port to open NAT/flows
    // Not required by RTSP, but improves interoperability
    for (int i = 0; i < 2; ++i) {
      m_udp.beginPacket(m_addr, m_serverRtpPort);
      uint8_t b = 0x00;
      m_udp.write(&b, 1);
      m_udp.endPacket();
      delay(2);
    }
  }

  bool sniffUdpFor(uint32_t ms) {
    if (!m_udp_active) return false;
    uint32_t start = millis();
    while ((millis() - start) < ms) {
      int packetSize = m_udp.parsePacket();
      if (packetSize > 0) {
        // restore to be processed by normal path
        return true;
      }
      delay(5);
    }
    return false;
  }

  // Centralized TCP write helper
  size_t tcpWrite(const uint8_t* data, size_t len) {
    if (m_tcpCmd.size() < 400) m_tcpCmd.resize(400);
    return m_tcpCmd.writeArray(data, len);
  }

  bool tcpCommit() {
    bool rc = m_tcp.write(m_tcpCmd.data(), m_tcpCmd.available()) ==
              m_tcpCmd.available();
    m_tcpCmd.clear();
    return rc;
  }

  bool sendSimpleRequest(const char* method, const char* url,
                         const char* extraHeaders, size_t extraLen,
                         char* outHeaders, size_t outHeadersLen, char* outBody,
                         size_t outBodyLen, bool quiet = false) {
    // Build request
    char reqStart[256];
    int reqLen = snprintf(
        reqStart, sizeof(reqStart),
        "%s %s RTSP/1.0\r\nCSeq: %u\r\nUser-Agent: ArduinoAudioTools\r\n",
        method, url, (unsigned)m_cseq++);
    if (reqLen <= 0) return false;

    // Send start line + mandatory headers
    if (tcpWrite((const uint8_t*)reqStart, reqLen) != (size_t)reqLen) {
      return false;
    }
    // Optional extra headers
    if (extraHeaders && extraLen) {
      if (tcpWrite((const uint8_t*)extraHeaders, extraLen) != extraLen) {
        return false;
      }
    }
    // End of headers
    const char* end = "\r\n";
    if (tcpWrite((const uint8_t*)end, 2) != 2) {
      return false;
    }

    if (!tcpCommit()) {
      LOGE("TCP write failed");
      return false;
    }

    // Read response headers until CRLFCRLF
    int hdrUsed = 0;
    memset(outHeaders, 0, outHeadersLen);
    if (!readUntilDoubleCRLF(outHeaders, outHeadersLen, hdrUsed,
                             m_headerTimeoutMs)) {
      if (!quiet) LOGE("RTSPClient: header read timeout");
      return false;
    }

    // Optionally read body based on Content-Length
    int contentLen = parseContentLength(outHeaders);
    if (outBody && outBodyLen && contentLen > 0) {
      int toRead = contentLen;
      if (toRead >= (int)outBodyLen) toRead = (int)outBodyLen - 1;
      int got = readExact((uint8_t*)outBody, toRead, 2000);
      if (got < 0) return false;
      outBody[got] = '\0';
    }
    return true;
  }

  bool readUntilDoubleCRLF(char* buf, size_t buflen, int& used,
                           uint32_t timeoutMs = 3000) {
    uint32_t start = millis();
    used = 0;
    int state = 0;  // match \r\n\r\n
    while ((millis() - start) < timeoutMs && used < (int)buflen - 1) {
      int avail = m_tcp.available();
      if (avail <= 0) {
        delay(5);
        continue;
      }
      int n = m_tcp.read((uint8_t*)buf + used, 1);
      if (n == 1) {
        char c = buf[used++];
        switch (state) {
          case 0:
            state = (c == '\r') ? 1 : 0;
            break;
          case 1:
            state = (c == '\n') ? 2 : 0;
            break;
          case 2:
            state = (c == '\r') ? 3 : 0;
            break;
          case 3:
            state = (c == '\n') ? 4 : 0;
            break;
        }
        if (state == 4) {
          buf[used] = '\0';
          return true;
        }
      }
    }
    buf[used] = '\0';
    return false;
  }

  int readExact(uint8_t* out, int len, uint32_t timeoutMs) {
    uint32_t start = millis();
    int got = 0;
    while (got < len && (millis() - start) < timeoutMs) {
      int a = m_tcp.available();
      if (a <= 0) {
        delay(5);
        continue;
      }
      int n = m_tcp.read(out + got, len - got);
      if (n > 0) got += n;
    }
    return (got == len) ? got : got;  // partial OK for DESCRIBE
  }

  static int parseContentLength(const char* headers) {
    const char* p = strcasestr(headers, "Content-Length:");
    if (!p) return 0;
    int len = 0;
    if (sscanf(p, "Content-Length: %d", &len) == 1) return len;
    return 0;
  }

  void parseSessionFromHeaders(const char* headers) {
    const char* p = strcasestr(headers, "Session:");
    if (!p) return;
    p += 8;  // skip "Session:"
    while (*p == ' ' || *p == '\t') ++p;
    size_t i = 0;
    while (*p && *p != '\r' && *p != '\n' && *p != ';' &&
           i < sizeof(m_sessionId) - 1) {
      m_sessionId[i++] = *p++;
    }
    m_sessionId[i] = '\0';
  }

  void parseServerPortsFromHeaders(const char* headers) {
    const char* t = strcasestr(headers, "Transport:");
    if (!t) return;
    const char* s = strcasestr(t, "server_port=");
    if (!s) return;
    s += strlen("server_port=");
    int a = 0, b = 0;
    if (sscanf(s, "%d-%d", &a, &b) == 2) {
      m_serverRtpPort = (uint16_t)a;
    }
  }

  // --- SDP parsing (rtpmap) ---
  void parseSdp(const char* sdp) {
    if (!sdp) return;
    const char* p = sdp;
    while ((p = strcasestr(p, "a=rtpmap:")) != nullptr) {
      p += 9;  // after a=rtpmap:
      int pt = 0;
      if (sscanf(p, "%d", &pt) != 1) continue;
      const char* space = strchr(p, ' ');
      if (!space) continue;
      ++space;
      // encoding up to '/' or endline
      size_t i = 0;
      while (space[i] && space[i] != '/' && space[i] != '\r' &&
             space[i] != '\n' && i < sizeof(m_encoding) - 1) {
        m_encoding[i] = space[i];
        ++i;
      }
      m_encoding[i] = '\0';
      int rate = 0, ch = 0;
      const char* afterEnc = space + i;
      if (*afterEnc == '/') {
        ++afterEnc;
        if (sscanf(afterEnc, "%d/%d", &rate, &ch) < 1) {
          rate = 0;
          ch = 0;
        }
      }
      m_payloadType = (uint8_t)pt;
      // Fill AudioInfo only for raw PCM encodings
      if (strcasecmp(m_encoding, "L16") == 0) {
        m_info = AudioInfo(rate, (ch > 0 ? ch : (ch == 0 ? 1 : ch)), 16);
      } else if (strcasecmp(m_encoding, "L8") == 0) {
        m_info = AudioInfo(rate, (ch > 0 ? ch : (ch == 0 ? 1 : ch)), 8);
      } else {
        m_info = AudioInfo();
      }
      m_multi_decoder.setAudioInfo(m_info);

      return;  // first match
    }
  }

  // --- Content-Base header parsing ---
  void parseContentBaseFromHeaders(const char* headers) {
    m_contentBase[0] = '\0';
    if (!headers) return;
    const char* p = strcasestr(headers, "Content-Base:");
    if (!p) return;
    p += strlen("Content-Base:");
    while (*p == ' ' || *p == '\t') ++p;
    size_t i = 0;
    while (*p && *p != '\r' && *p != '\n' && i < sizeof(m_contentBase) - 1) {
      m_contentBase[i++] = *p++;
    }
    m_contentBase[i] = '\0';
    // Ensure trailing '/'
    if (i > 0 && m_contentBase[i - 1] != '/') {
      if (i + 1 < sizeof(m_contentBase)) {
        m_contentBase[i++] = '/';
        m_contentBase[i] = '\0';
      }
    }
  }

  // --- SDP control parsing ---
  void parseControlFromSdp(const char* sdp) {
    m_sdpControl[0] = '\0';
    if (!sdp) return;
    const char* audio = strcasestr(sdp, "\nm=audio ");
    const char* searchStart = sdp;
    const char* searchEnd = nullptr;
    if (audio) {
      // find end of this media block (next m= or end)
      searchStart = audio;
      const char* nextm = strcasestr(audio + 1, "\nm=");
      searchEnd = nextm ? nextm : (sdp + strlen(sdp));
    } else {
      // fall back to session-level
      searchStart = sdp;
      searchEnd = sdp + strlen(sdp);
    }
    const char* p = searchStart;
    while (p && p < searchEnd) {
      const char* ctrl = strcasestr(p, "a=control:");
      if (!ctrl || ctrl >= searchEnd) break;
      ctrl += strlen("a=control:");
      // copy value until CR/LF
      size_t i = 0;
      while (ctrl[i] && ctrl[i] != '\r' && ctrl[i] != '\n' &&
             i < sizeof(m_sdpControl) - 1) {
        m_sdpControl[i] = ctrl[i];
        ++i;
      }
      m_sdpControl[i] = '\0';
      break;
    }
  }

  bool isAbsoluteRtspUrl(const char* url) {
    if (!url) return false;
    return (strncasecmp(url, "rtsp://", 7) == 0) ||
           (strncasecmp(url, "rtsps://", 8) == 0);
  }

  void buildTrackUrlFromBaseAndControl() {
    // default fallback if no control provided
    if (m_sdpControl[0] == '\0') {
      snprintf(m_trackUrl, sizeof(m_trackUrl), "%strackID=0", m_baseUrl);
      return;
    }
    if (isAbsoluteRtspUrl(m_sdpControl)) {
      strncpy(m_trackUrl, m_sdpControl, sizeof(m_trackUrl) - 1);
      m_trackUrl[sizeof(m_trackUrl) - 1] = '\0';
      return;
    }
    const char* base = (m_contentBase[0] ? m_contentBase : m_baseUrl);
    size_t blen = strlen(base);
    // Construct base ensuring single '/'
    char tmp[256];
    size_t pos = 0;
    for (; pos < sizeof(tmp) - 1 && pos < blen; ++pos) tmp[pos] = base[pos];
    if (pos > 0 && tmp[pos - 1] != '/' && pos < sizeof(tmp) - 1)
      tmp[pos++] = '/';
    // If control starts with '/', skip one to avoid '//'
    const char* ctrl = m_sdpControl;
    if (*ctrl == '/') ++ctrl;
    while (*ctrl && pos < sizeof(tmp) - 1) tmp[pos++] = *ctrl++;
    tmp[pos] = '\0';
    strncpy(m_trackUrl, tmp, sizeof(m_trackUrl) - 1);
    m_trackUrl[sizeof(m_trackUrl) - 1] = '\0';
  }

  // resampler is started in constructor; audio info will be set dynamically
};

}  // namespace audio_tools