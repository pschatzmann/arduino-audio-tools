/*
 * Author: Phil Schatzmann
 *
 * Based on Micro-RTSP library:
 * https://github.com/Tomp0801/Micro-RTSP-Audio
 * https://github.com/geeksville/Micro-RTSP
 *
 */

#pragma once

#include <stdio.h>

#include <ctime>

#include "AudioTools/CoreAudio/AudioBasic/Collections/Vector.h"
#include "RTSPAudioStreamer.h"
#include "RTSPPlatform.h"

/// Buffer size for incoming requests, and outgoing responses
#define RTSP_BUFFER_SIZE 10000
/// Size of RTSP parameter buffers
#define RTSP_PARAM_STRING_MAX 100
/// Buffer size for RTSP host name
#define MAX_HOSTNAME_LEN 256
/// Size of response buffer (previously hardcoded 2251)
#define RTSP_RESPONSE_BUFFER_SIZE 2251
/// Size of SDP buffer (previously hardcoded 1024*2)
#define RTSP_SDP_BUFFER_SIZE 1024
/// Size of URL buffer (previously hardcoded 1024)
#define RTSP_URL_BUFFER_SIZE 1024
/// Generic small temp buffer size (previously hardcoded 256)
#define RTSP_SMALL_BUFFER_SIZE 256

namespace audio_tools {

/// Supported RTSP command types
enum RTSP_CMD_TYPES {
  RTSP_OPTIONS,
  RTSP_DESCRIBE,
  RTSP_SETUP,
  RTSP_PLAY,
  RTSP_PAUSE,
  RTSP_TEARDOWN,
  RTSP_UNKNOWN
};

/**
 * @brief RTSP Session Handler - Individual Client Protocol Management
 *
 * The RtspSession class manages RTSP protocol communication with a single
 * client. It handles the complete RTSP session lifecycle from initial
 * connection through streaming termination. Key responsibilities include:
 *
 * - RTSP message parsing and protocol state management
 * - SDP (Session Description Protocol) generation for audio format negotiation
 * - RTP transport setup and coordination with RTSPAudioStreamer
 * - Session state tracking (INIT -> READY -> PLAYING)
 * - Client timeout and connection management
 *
 * The memory for buffers is allocated in PSRAM if available and active.
 *
 * @section protocol RTSP Message Flow
 * 1. **OPTIONS** - Client queries supported methods
 * 2. **DESCRIBE** - Server returns SDP with audio format details
 * 3. **SETUP** - Client requests RTP transport, server allocates ports
 * 4. **PLAY** - Client starts playback, server begins RTP streaming
 * 5. **TEARDOWN** - Client ends session, server cleans up resources
 *
 * @note This class is typically instantiated by RTSPServer, not directly by
 * users
 * @note Requires a configured RTSPAudioStreamer for media delivery
 * @author Phil Schatzmann
 * @ingroup rtsp
 */
template <typename Platform>
class RtspSession {
 public:
  /**
   * @brief Construct RTSP session for a connected client
   *
   * Creates a new RTSP session instance to handle protocol communication
   * with a specific client. Initializes session state and prepares for
   * RTSP message processing. Buffer initialization is handled by the init()
   * method.
   *
   * @param aClient WiFiClient object representing the connected RTSP client
   * @param aStreamer Pointer to RTSPAudioStreamer that will provide audio data
   * for this session
   *
   * @note Session automatically generates unique session ID and initializes
   * internal state
   * @note Vector buffers are initialized lazily by the init() method when
   * needed
   * @see handleRequests(), init()
   */
  RtspSession(typename Platform::TcpClientType& aClient,
              RTSPAudioStreamerBase<Platform>& aStreamer)
      : m_Client(aClient), m_Streamer(&aStreamer) {
    m_RtspClient = &m_Client;
    m_RtspSessionID = random(65536);  // create a session ID
    LOGI("RTSP session created");
  }

  /**
   * @brief Destructor - cleanup session resources
   *
   * Closes the RTSP client socket and ensures streaming is properly stopped.
   * Vector buffers are automatically managed.
   *
   * @note Ensures UDP transport cleanup for proper resource management
   */
  ~RtspSession() {
    LOGI("RTSP session destructor");

    // Ensure streaming is stopped and resources are released
    if (m_streaming && m_Streamer) {
      LOGI("Final cleanup: stopping streamer in destructor");
      m_Streamer->stop();
      m_Streamer->releaseUdpTransport();
      m_streaming = false;
    }

    // Close the client socket (check connection using client method directly)
    if (m_RtspClient && m_RtspClient->connected()) {
      Platform::closeSocket(m_RtspClient);
    }

    LOGI("RTSP session cleanup completed");
  }

  /**
   * @brief Process incoming RTSP requests from the client
   *
   * Reads RTSP messages from the client socket and dispatches them to
   * appropriate handler methods. This is the main processing loop for RTSP
   * protocol communication. Supports all standard RTSP commands (OPTIONS,
   * DESCRIBE, SETUP, PLAY, TEARDOWN).
   *
   * @param readTimeoutMs Maximum time in milliseconds to wait for incoming data
   * @return true if request was processed successfully, false on timeout or
   * session end
   *
   * @note Returns false when client disconnects or times out
   * @note Filters messages to ensure they are valid RTSP commands
   * @note Should be called repeatedly in a loop until it returns false
   */
  bool handleRequests(uint32_t readTimeoutMs) {
    LOGD("handleRequests");
    // initlaize buffers and state if not already done
    init();

    if (!m_sessionOpen) {
      delay(100);    // give some time to close down
      return false;  // Already closed down
    }

    memset(mRecvBuf.data(), 0x00, RTSP_BUFFER_SIZE);
    int res = readSocket(m_RtspClient, mRecvBuf.data(), RTSP_BUFFER_SIZE,
                         readTimeoutMs);
    if (res > 0) {
      // we filter away everything which seems not to be an RTSP command:
      // O-ption, D-escribe, S-etup, P-lay, T-eardown
      if ((mRecvBuf[0] == 'O') || (mRecvBuf[0] == 'D') ||
          (mRecvBuf[0] == 'S') || (mRecvBuf[0] == 'P') ||
          (mRecvBuf[0] == 'T')) {
        RTSP_CMD_TYPES C = handleRtspRequest(mRecvBuf.data(), res);
        if (!m_sessionOpen) {
          // Session was aborted (e.g., rejected by callback); end quickly
          return false;
        }
        // TODO this should go in the handling functions
        if (C == RTSP_PLAY) {
          m_streaming = true;
        } else if (C == RTSP_PAUSE) {
          m_streaming = false;
        } else if (C == RTSP_TEARDOWN) {
          m_sessionOpen = false;  // Session ended by TEARDOWN

          // Properly cleanup streaming on TEARDOWN command
          if (m_streaming && m_Streamer) {
            LOGI("Stopping streamer due to TEARDOWN");
            m_Streamer->stop();
            m_Streamer->releaseUdpTransport();
            m_streaming = false;
          }
        }
      }
      return true;
    } else if (res == 0) {
      LOGW("client closed socket, exiting");
      m_sessionOpen = false;  // Session ended by client disconnect

      // CRITICAL: Properly cleanup streaming when client disconnects
      if (m_streaming && m_Streamer) {
        LOGI("Stopping streamer due to client disconnect");
        m_Streamer->stop();
        m_Streamer->releaseUdpTransport();
        m_streaming = false;
      }

      return false;  // Return false to indicate session should end
    } else {
      // Timeout on read
      // printf("RTSP read timeout\n");
      return false;
    }
  }

  bool isSessionOpen() { return m_sessionOpen; }

  bool isStreaming() { return m_streaming; }

  /**
   * @brief Set a callback to receive the RTSP URL path that opened the session.
   * The callback is invoked once, after the first request is parsed, with the
   * path portion of the RTSP URL (starting with '/'). A user reference is
   * provided back on invocation.
   *
  * Return semantics:
  * - true: accept session and continue normal RTSP handling
  * - false: reject session; the session will be marked closed and no
  *   responses will be sent for the pending request
   */
  void setOnSessionPath(bool (*cb)(const char* path, void* ref), void* ref = nullptr) {
    m_onSessionPath = cb;
    m_onSessionPathRef = ref;
  }

 protected:
  const char* STD_URL_PRE_SUFFIX = "trackID";

  // global session state parameters
  int m_RtspSessionID;
  typename Platform::TcpClientType m_Client;
  typename Platform::TcpClientType* m_RtspClient =
      nullptr;                // RTSP socket of session
  int m_StreamID = -1;        // number of simulated stream of that session
  uint16_t m_ClientRTPPort;   // client port for UDP based RTP transport
  uint16_t m_ClientRTCPPort;  // client port for UDP based RTCP transport
  RTSPAudioStreamerBase<Platform>* m_Streamer =
      nullptr;  // the UDP streamer of that session

  // parameters of the last received RTSP request
  RTSP_CMD_TYPES m_RtspCmdType;  // command type (if any) of the current request
  audio_tools::Vector<char> m_URLPreSuffix;  // stream name pre suffix
  audio_tools::Vector<char> m_URLSuffix;     // stream name suffix
  audio_tools::Vector<char> m_CSeq;          // RTSP command sequence number
  audio_tools::Vector<char> m_URLHostPort;   // host:port part of the URL
  audio_tools::Vector<char> m_URLPath;       // full RTSP path (starting with '/')
  unsigned m_ContentLength;                  // SDP string size
  uint16_t m_RtpClientPort =
      0;  // RTP receiver port on client (in host byte order!)
  uint16_t m_RtcpClientPort =
      0;  // RTCP receiver port on client (in host byte order!)
  // Transport parsing (TCP interleaved)
  bool m_TransportIsTcp = false;
  int m_InterleavedRtp = -1;
  int m_InterleavedRtcp = -1;
  audio_tools::Vector<char>
      m_Response;  // Note: we assume single threaded, this large buf we
                   // keep off of the tiny stack
  audio_tools::Vector<char> m_SDPBuf;
  audio_tools::Vector<char> m_URLBuf;
  audio_tools::Vector<char> m_Buf1;
  audio_tools::Vector<char> m_Buf2;
  audio_tools::Vector<char> mRecvBuf;
  audio_tools::Vector<char> mCurRequest;
  audio_tools::Vector<char> m_CmdName;
  bool m_is_init = false;
  bool m_streaming = false;
  volatile bool m_sessionOpen = true;
  bool m_pathNotified = false;
  bool (*m_onSessionPath)(const char* path, void* ref) = nullptr;
  void* m_onSessionPathRef = nullptr;

  /**
   * Initializes memory and buffers
   *
   * Lazily initializes Vector buffers if they haven't been allocated yet,
   * and resets all RTSP parsing state variables to their default values.
   * This method can be called multiple times safely.
   */
  void init() {
    if (m_is_init) return;
    LOGD("init");

    // Reset session state for clean initialization
    m_streaming = false;
    m_sessionOpen = true;

    // initialize buffers if not already done
    if (mRecvBuf.size() == 0) {
      mRecvBuf.resize(RTSP_BUFFER_SIZE);
    }
    if (mCurRequest.size() == 0) {
      mCurRequest.resize(RTSP_BUFFER_SIZE);
    }
    if (m_URLPreSuffix.size() == 0) {
      m_URLPreSuffix.resize(RTSP_PARAM_STRING_MAX);
    }
    if (m_URLSuffix.size() == 0) {
      m_URLSuffix.resize(RTSP_PARAM_STRING_MAX);
    }
    if (m_CSeq.size() == 0) {
      m_CSeq.resize(RTSP_PARAM_STRING_MAX);
    }
    if (m_URLHostPort.size() == 0) {
      m_URLHostPort.resize(MAX_HOSTNAME_LEN);
    }
    if (m_URLPath.size() == 0) {
      m_URLPath.resize(RTSP_URL_BUFFER_SIZE);
    }
    if (m_Response.size() == 0) {
      m_Response.resize(RTSP_RESPONSE_BUFFER_SIZE);
    }
    if (m_SDPBuf.size() == 0) {
      m_SDPBuf.resize(RTSP_SDP_BUFFER_SIZE);
    }
    if (m_URLBuf.size() == 0) {
      m_URLBuf.resize(RTSP_URL_BUFFER_SIZE);
    }
    if (m_Buf1.size() == 0) {
      m_Buf1.resize(RTSP_SMALL_BUFFER_SIZE);
    }
    if (m_Buf2.size() == 0) {
      m_Buf2.resize(RTSP_SMALL_BUFFER_SIZE);
    }
    if (m_CmdName.size() == 0) {
      m_CmdName.resize(RTSP_PARAM_STRING_MAX);
    }

    m_RtspCmdType = RTSP_UNKNOWN;
    memset(m_URLPreSuffix.data(), 0x00, m_URLPreSuffix.size());
    memset(m_URLSuffix.data(), 0x00, m_URLSuffix.size());
    memset(m_CSeq.data(), 0x00, m_CSeq.size());
    memset(m_URLHostPort.data(), 0x00, m_URLHostPort.size());
    if (m_URLPath.size() > 0) memset(m_URLPath.data(), 0x00, m_URLPath.size());
    m_ContentLength = 0;
    m_TransportIsTcp = false;
    m_InterleavedRtp = -1;
    m_InterleavedRtcp = -1;
    m_is_init = true;
    m_pathNotified = false;
  }

  /**
   * Parses the an RTSP request and calls the response function depending on the
   * type of command
   * @param aRequest c string containing the request
   * @param aRequestSize length of the request
   * @return the command type of the RTSP request
   */
  RTSP_CMD_TYPES handleRtspRequest(char const* aRequest,
                                   unsigned aRequestSize) {
    if (parseRtspRequest(aRequest, aRequestSize)) {
      switch (m_RtspCmdType) {
        case RTSP_OPTIONS: {
          handleRtspOption();
          break;
        }
        case RTSP_DESCRIBE: {
          handleRtspDescribe();
          break;
        }
        case RTSP_SETUP: {
          handleRtspSetup();
          break;
        }
        case RTSP_PLAY: {
          handleRtspPlay();
          break;
        }
        case RTSP_PAUSE: {
          handleRtspPause();
          break;
        }
        case RTSP_TEARDOWN: {
          handleRtspTeardown();
          break;
        }
        default: {
        }
      }
    }
    return m_RtspCmdType;
  }

  /**
   * Parses an RTSP request, storing the extracted information in the
   * RTSPSession object
   * @param aRequest c string containing the request
   * @param aRequestSize length of the request
   * @return true if parsing was successful
   */
  bool parseRtspRequest(char const* aRequest, unsigned aRequestSize) {
    LOGI("aRequest: ------------------------\n%s\n-------------------------",
         aRequest);

    const unsigned CurRequestSize = aRequestSize;
    memcpy(mCurRequest.data(), aRequest, aRequestSize);

    // 1) Ports and transport
    parseClientPorts(mCurRequest.data());
    parseTransportHeader(mCurRequest.data());

    // 2) Command + URL host/parts
    unsigned idxAfterCmd = 0;
    if (!parseCommandName(mCurRequest.data(), CurRequestSize, idxAfterCmd))
      return false;
    determineCommandType();
    parseUrlHostPortAndSuffix(mCurRequest.data(), CurRequestSize, idxAfterCmd);
    if (!m_sessionOpen) {
      // Aborted by callback during URL parse; don't proceed further
      return false;
    }

    // 3) CSeq and Content-Length
    if (!parseCSeq(mCurRequest.data(), CurRequestSize, idxAfterCmd))
      return false;
    parseContentLength(mCurRequest.data(), CurRequestSize, idxAfterCmd);

    // 4) Client preference toggle (User-Agent / URL)
    detectClientHeaderPreference(mCurRequest.data());

    return true;
  }

  // ---- Parsing helpers ----
  void parseClientPorts(char* req) {
    char* ClientPortPtr = strstr(req, "client_port");
    if (!ClientPortPtr) return;
    char* lineEnd = strstr(ClientPortPtr, "\r\n");
    if (!lineEnd) return;
    char* CP = m_Response.data();
    memset(CP, 0, m_Response.size());
    char saved = lineEnd[0];
    lineEnd[0] = '\0';
    strcpy(CP, ClientPortPtr);
    char* eq = strstr(CP, "=");
    if (eq) {
      ++eq;
      strcpy(CP, eq);
      char* dash = strstr(CP, "-");
      if (dash) {
        dash[0] = '\0';
        m_ClientRTPPort = atoi(CP);
        m_ClientRTCPPort = m_ClientRTPPort + 1;
      }
    }
    lineEnd[0] = saved;
  }

  void parseTransportHeader(char* req) {
    char* TransportPtr = strstr(req, "Transport:");
    if (!TransportPtr) return;
    char* lineEnd = strstr(TransportPtr, "\r\n");
    if (!lineEnd) return;
    char* CP = m_Response.data();
    memset(CP, 0, m_Response.size());
    char saved = lineEnd[0];
    lineEnd[0] = '\0';
    strncpy(CP, TransportPtr, m_Response.size() - 1);
    CP[m_Response.size() - 1] = '\0';
    if (strstr(CP, "RTP/AVP/TCP") || strstr(CP, "/TCP")) m_TransportIsTcp = true;
    char* inter = strstr(CP, "interleaved=");
    if (inter) {
      inter += strlen("interleaved=");
      int a = -1, b = -1;
      if (sscanf(inter, "%d-%d", &a, &b) == 2) {
        m_InterleavedRtp = a;
        m_InterleavedRtcp = b;
      } else if (sscanf(inter, "%d,%d", &a, &b) == 2) {
        m_InterleavedRtp = a;
        m_InterleavedRtcp = b;
      } else if (sscanf(inter, "%d", &a) == 1) {
        m_InterleavedRtp = a;
        m_InterleavedRtcp = a + 1;
      }
    }
    lineEnd[0] = saved;
  }

  bool parseCommandName(char* req, unsigned reqSize, unsigned& outIdx) {
    bool ok = false;
    unsigned i;
    for (i = 0; i < m_CmdName.size() - 1 && i < reqSize; ++i) {
      char c = req[i];
      if (c == ' ' || c == '\t') {
        ok = true;
        break;
      }
      m_CmdName[i] = c;
    }
    m_CmdName[i] = '\0';
    if (!ok) {
      LOGE("failed to parse RTSP");
      return false;
    }
    LOGI("RTSP received %s", m_CmdName.data());
    outIdx = i;
    return true;
  }

  void determineCommandType() {
    if (strstr(m_CmdName.data(), "OPTIONS"))
      m_RtspCmdType = RTSP_OPTIONS;
    else if (strstr(m_CmdName.data(), "DESCRIBE"))
      m_RtspCmdType = RTSP_DESCRIBE;
    else if (strstr(m_CmdName.data(), "SETUP"))
      m_RtspCmdType = RTSP_SETUP;
    else if (strstr(m_CmdName.data(), "PLAY"))
      m_RtspCmdType = RTSP_PLAY;
    else if (strstr(m_CmdName.data(), "PAUSE"))
      m_RtspCmdType = RTSP_PAUSE;
    else if (strstr(m_CmdName.data(), "TEARDOWN"))
      m_RtspCmdType = RTSP_TEARDOWN;
    else
      LOGE("Error: Unsupported Command received (%s)!", m_CmdName.data());
  }

  void parseUrlHostPortAndSuffix(char* req, unsigned reqSize, unsigned& i) {
    unsigned j = i + 1;
    while (j < reqSize && (req[j] == ' ' || req[j] == '\t')) ++j;
    for (; (int)j < (int)(reqSize - 8); ++j) {
      if ((req[j] == 'r' || req[j] == 'R') && (req[j + 1] == 't' || req[j + 1] == 'T') &&
          (req[j + 2] == 's' || req[j + 2] == 'S') && (req[j + 3] == 'p' || req[j + 3] == 'P') &&
          req[j + 4] == ':' && req[j + 5] == '/') {
        j += 6;
        if (req[j] == '/') {
          ++j;
          unsigned uidx = 0;
          while (j < reqSize && req[j] != '/' && req[j] != ' ' && uidx < m_URLHostPort.size() - 1) {
            m_URLHostPort[uidx++] = req[j++];
          }
        } else {
          --j;
        }
        i = j;
        break;
      }
    }
    LOGD("m_URLHostPort: %s", m_URLHostPort.data());

    // Extract full RTSP path starting at current index i up to next space
    if (i < reqSize && req[i] == '/') {
      unsigned p = 0;
      unsigned k = i;
      while (k < reqSize && req[k] != ' ' && p < m_URLPath.size() - 1) {
        m_URLPath[p++] = req[k++];
      }
      m_URLPath[p] = '\0';
      LOGD("m_URLPath: %s", m_URLPath.data());
      if (!m_pathNotified && m_onSessionPath) {
        bool ok = m_onSessionPath(m_URLPath.data(), m_onSessionPathRef);
        m_pathNotified = true;
        if (!ok) {
          LOGW("Session rejected by onSessionPath callback");
          m_sessionOpen = false;
          // Early exit: abort further parsing of this request
        }
      }
    }

    bool ok = false;
    for (unsigned k = i + 1; (int)k < (int)(reqSize - 5); ++k) {
      if (req[k] == 'R' && req[k + 1] == 'T' && req[k + 2] == 'S' && req[k + 3] == 'P' && req[k + 4] == '/') {
        while (--k >= i && req[k] == ' ') {}
        unsigned k1 = k;
        while (k1 > i && req[k1] != '=') --k1;
        if (k - k1 + 1 <= m_URLSuffix.size()) {
          unsigned n = 0, k2 = k1 + 1;
          while (k2 <= k) m_URLSuffix[n++] = req[k2++];
          m_URLSuffix[n] = '\0';
          if (k1 - i <= m_URLPreSuffix.size()) ok = true;
          n = 0;
          k2 = i + 1;
          while (k2 <= k1 - 1) m_URLPreSuffix[n++] = req[k2++];
          m_URLPreSuffix[n] = '\0';
          i = k + 7;
        }
        break;
      }
    }
    LOGD("m_URLSuffix: %s", m_URLSuffix.data());
    LOGD("m_URLPreSuffix: %s", m_URLPreSuffix.data());
    LOGD("URL Suffix parse succeeded: %i", ok);
  }

  bool parseCSeq(char* req, unsigned reqSize, unsigned startIdx) {
    bool ok = false;
    for (unsigned j = startIdx; (int)j < (int)(reqSize - 5); ++j) {
      if (req[j] == 'C' && req[j + 1] == 'S' && req[j + 2] == 'e' && req[j + 3] == 'q' && req[j + 4] == ':') {
        j += 5;
        while (j < reqSize && (req[j] == ' ' || req[j] == '\t')) ++j;
        unsigned n;
        for (n = 0; n < m_CSeq.size() - 1 && j < reqSize; ++n, ++j) {
          char c = req[j];
          if (c == '\r' || c == '\n') {
            ok = true;
            break;
          }
          m_CSeq[n] = c;
        }
        m_CSeq[n] = '\0';
        break;
      }
    }
    LOGD("Look for CSeq success: %i", ok);
    return ok;
  }

  void parseContentLength(char* req, unsigned reqSize, unsigned startIdx) {
    for (unsigned j = startIdx; (int)j < (int)(reqSize - 15); ++j) {
      if (req[j] == 'C' && req[j + 1] == 'o' && req[j + 2] == 'n' && req[j + 3] == 't' &&
          req[j + 4] == 'e' && req[j + 5] == 'n' && req[j + 6] == 't' && req[j + 7] == '-' &&
          (req[j + 8] == 'L' || req[j + 8] == 'l') && req[j + 9] == 'e' && req[j + 10] == 'n' &&
          req[j + 11] == 'g' && req[j + 12] == 't' && req[j + 13] == 'h' && req[j + 14] == ':') {
        j += 15;
        while (j < reqSize && (req[j] == ' ' || req[j] == '\t')) ++j;
        unsigned num;
        if (sscanf(&req[j], "%u", &num) == 1) m_ContentLength = num;
      }
    }
  }

  void detectClientHeaderPreference(char* req) {
    char* ua = strstr(req, "User-Agent:");
    bool want_rfc2250 = false;
    if (ua) {
      if (strcasestr(ua, "ffmpeg") || strcasestr(ua, "ffplay") || strcasestr(ua, "libavformat") ||
          strcasestr(ua, "Lavf")) {
        want_rfc2250 = true;
      }
      if (strcasestr(ua, "vlc")) want_rfc2250 = false;
    }
    char* qm = strchr(req, '?');
    if (qm) {
      if (strstr(qm, "mpa_hdr=1")) want_rfc2250 = true;
      if (strstr(qm, "mpa_hdr=0")) want_rfc2250 = false;
    }
    if (m_Streamer && m_Streamer->getAudioSource()) {
      RTSPFormat& fmt = m_Streamer->getAudioSource()->getFormat();
      fmt.setUseRfc2250Header(want_rfc2250);
    }
  }

  /**
   * Sends Response to OPTIONS command
   */
  void handleRtspOption() {
    snprintf(m_Response.data(), m_Response.size(),
             "RTSP/1.0 200 OK\r\nCSeq: %s\r\n"
             "Public: DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE\r\n\r\n",
             m_CSeq.data());

    sendSocket(m_RtspClient, m_Response.data(), strlen(m_Response.data()));
  }

  /**
   * Sends Response to DESCRIBE command
   */
  void handleRtspDescribe() {
    // check whether we know a stream with the URL which is requested
    m_StreamID = -1;  // invalid URL
    // find Stream ID
    if (strcmp(m_URLPreSuffix.data(), STD_URL_PRE_SUFFIX) == 0) {
      char* end;
      m_StreamID = strtol(m_URLSuffix.data(), &end, 10);
      if (*end != '\0') m_StreamID = -1;
    }

    // simulate DESCRIBE server response
    char* ColonPtr;
    strncpy(m_Buf1.data(), m_URLHostPort.data(), 256);
    ColonPtr = strstr(m_Buf1.data(), ":");
    if (ColonPtr != nullptr) ColonPtr[0] = 0x00;

    snprintf(
        m_SDPBuf.data(), m_SDPBuf.size(),
        "v=0\r\n"  // SDP Version
        "o=- %d 0 IN IP4 %s\r\n"
        "%s"
        "a=control:%s=0",
        rand() & 0xFF, m_Buf1.data(),
        m_Streamer->getAudioSource()->getFormat().format(m_Buf2.data(), 256),
        STD_URL_PRE_SUFFIX);

    snprintf(m_URLBuf.data(), m_URLBuf.size(), "rtsp://%s",
             m_URLHostPort.data());

    snprintf(m_Response.data(), m_Response.size(),
             "RTSP/1.0 200 OK\r\nCSeq: %s\r\n"
             "%s\r\n"
             "Content-Base: %s/\r\n"
             "Content-Type: application/sdp\r\n"
             "Content-Length: %d\r\n\r\n"
             "%s",
             m_CSeq.data(), dateHeader(), m_URLBuf.data(),
             (int)strlen(m_SDPBuf.data()), m_SDPBuf.data());

    // LOGI("handleRtspDescribe: %s", (const char*)m_Response.data());
    Serial.println("------------------------------");
    Serial.println((const char*)m_Response.data());
    Serial.println("------------------------------");
    sendSocket(m_RtspClient, m_Response.data(), strlen(m_Response.data()));
  }

  /**
   * Sends Response to SETUP command and prepares RTP stream
   */
  void handleRtspSetup() {
    // Build response depending on requested transport
    if (m_TransportIsTcp) {
      // Initialize TCP interleaved transport on same RTSP TCP socket
      int ch0 = (m_InterleavedRtp >= 0) ? m_InterleavedRtp : 0;
      int ch1 = (m_InterleavedRtcp >= 0) ? m_InterleavedRtcp : (ch0 + 1);
      m_Streamer->initTcpInterleavedTransport(m_RtspClient, ch0, ch1);

      // Reply with interleaved channels
      snprintf(m_Buf1.data(), m_Buf1.size(),
               "RTP/AVP/TCP;unicast;interleaved=%d-%d", ch0, ch1);
    } else {
      // init RTSP Session transport type (UDP) and ports for UDP transport
      initTransport(m_ClientRTPPort, m_ClientRTCPPort);
      // UDP Transport response with client/server ports and ssrc
      snprintf(m_Buf1.data(), m_Buf1.size(),
               "RTP/AVP;unicast;client_port=%i-%i;server_port=%i-%i;ssrc=%08X",
               m_ClientRTPPort, m_ClientRTCPPort,
               m_Streamer->getRtpServerPort(), m_Streamer->getRtcpServerPort(),
               (unsigned)m_Streamer->currentSsrc());
    }
    snprintf(m_Response.data(), m_Response.size(),
             "RTSP/1.0 200 OK\r\n"
             "CSeq: %s\r\n"
             "%s\r\n"
             "Session: %i\r\n"
             "Transport: %s\r\n"
             "\r\n",
             m_CSeq.data(), dateHeader(), m_RtspSessionID, m_Buf1.data());

    Serial.println("------------------------------");
    Serial.println((char*)m_Response.data());
    Serial.println("------------------------------");

    sendSocket(m_RtspClient, m_Response.data(), strlen(m_Response.data()));
  }

  /**
   * Sends Response to PLAY command and starts the RTP stream
   */
  void handleRtspPlay() {
    // Build RTP-Info to help clients (e.g., VLC) synchronize
    // URL base
    snprintf(m_URLBuf.data(), m_URLBuf.size(), "rtsp://%s/%s=0",
             m_URLHostPort.data(), STD_URL_PRE_SUFFIX);

    // Current seq/timestamp from streamer (values used for next packet)
    uint16_t seq = m_Streamer ? m_Streamer->currentSeq() : 0;
    uint32_t rtptime = m_Streamer ? m_Streamer->currentRtpTimestamp() : 0;

    // PLAY response with Range, Session and RTP-Info
    snprintf(m_Response.data(), m_Response.size(),
             "RTSP/1.0 200 OK\r\n"
             "CSeq: %s\r\n"
             "Range: npt=0.000-\r\n"
             "Session: %i\r\n"
             "RTP-Info: url=%s;seq=%u;rtptime=%u\r\n\r\n",
             m_CSeq.data(), m_RtspSessionID, m_URLBuf.data(), (unsigned)seq,
             (unsigned)rtptime);

    Serial.println("------------------------------");
    Serial.println((char*)m_Response.data());
    Serial.println("------------------------------");

    sendSocket(m_RtspClient, m_Response.data(), strlen(m_Response.data()));

    m_Streamer->start();
  }

  /**
   * Sends Response to PAUSE command and stops RTP stream without closing
   * session
   */
  void handleRtspPause() {
    if (m_streaming && m_Streamer) {
      m_Streamer->stop();
    }
    snprintf(m_Response.data(), m_Response.size(),
             "RTSP/1.0 200 OK\r\n"
             "CSeq: %s\r\n"
             "Session: %i\r\n\r\n",
             m_CSeq.data(), m_RtspSessionID);
    sendSocket(m_RtspClient, m_Response.data(), strlen(m_Response.data()));
  }

  /**
   * Sends Response to TEARDOWN command, stops the RTP stream
   */
  void handleRtspTeardown() {
    m_Streamer->stop();

    // simulate SETUP server response
    snprintf(m_Response.data(), m_Response.size(),
             "RTSP/1.0 200 OK\r\n"
             "CSeq: %s\r\n\r\n",
             m_CSeq.data());

    sendSocket(m_RtspClient, m_Response.data(), strlen(m_Response.data()));

    m_sessionOpen = false;
  }

  /**
   * Gives the current stream ID
   * @return ID of current stream or -1 if invalid
   */
  int getStreamID() { return m_StreamID; }

  /**
   * Prepares sockets for RTP stream
   * @param aRtpPort local port number for RTP connection
   * @param aRtcpPort local port number for RTCP connection
   */
  void initTransport(u_short aRtpPort, u_short aRtcpPort) {
    m_RtpClientPort = aRtpPort;
    m_RtcpClientPort = aRtcpPort;

    IPAddress clientIP;
    uint16_t clientPort;
    Platform::getSocketPeerAddr(m_RtspClient, &clientIP, &clientPort);

    LOGI("SETUP peer resolved: %s:%u (RTP client_port=%u)",
         clientIP.toString().c_str(), (unsigned)clientPort,
         (unsigned)m_RtpClientPort);

    m_Streamer->initUdpTransport(clientIP, m_RtpClientPort);
  }

  typename Platform::TcpClientType*& getClient() { return m_RtspClient; }

  uint16_t getRtpClientPort() { return m_RtpClientPort; }

  /**
   * @brief Inline helper to read from socket
   * @param sock TCP socket to read from
   * @param buf Buffer to read into
   * @param buflen Buffer length
   * @param timeoutmsec Timeout in milliseconds
   * @return Number of bytes read, 0=closed, -1=timeout
   */
  inline int readSocket(typename Platform::TcpClientType* sock, char* buf,
                        size_t buflen, int timeoutmsec) {
    return Platform::readSocket(sock, buf, buflen, timeoutmsec);
  }

  /**
   * @brief Inline helper to send data over socket
   * @param sock TCP socket to send to
   * @param buf Data buffer to send
   * @param len Length of data
   * @return Number of bytes sent
   */
  inline ssize_t sendSocket(typename Platform::TcpClientType* sock,
                            const void* buf, size_t len) {
    return Platform::sendSocket(sock, buf, len);
  }

  /**
   * Create the DateHeader string for RTSP responses
   * @return pointer to Date Header string
   */
  char const* dateHeader() {
    static char buf[200];
    time_t tt = time(NULL);
    strftime(buf, sizeof(buf), "Date: %a, %b %d %Y %H:%M:%S GMT", gmtime(&tt));
    return buf;
  }
};

}  // namespace audio_tools
