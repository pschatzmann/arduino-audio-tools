/*
 * Author: Thomas Pfitzinger
 * github: https://github.com/Tomp0801/Micro-RTSP-Audio
 *
 * Based on Micro-RTSP library for video streaming by Kevin Hester:
 *
 * https://github.com/geeksville/Micro-RTSP
 *
 * Copyright 2018 S. Kevin Hester-Chow, kevinh@geeksville.com (MIT License)
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

namespace audio_tools {

/// Supported RTSP command types
enum RTSP_CMD_TYPES {
  RTSP_OPTIONS,
  RTSP_DESCRIBE,
  RTSP_SETUP,
  RTSP_PLAY,
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
 * @section protocol RTSP Message Flow
 * 1. **OPTIONS** - Client queries supported methods
 * 2. **DESCRIBE** - Server returns SDP with audio format details
 * 3. **SETUP** - Client requests RTP transport, server allocates ports
 * 4. **PLAY** - Client starts playback, server begins RTP streaming
 * 5. **TEARDOWN** - Client ends session, server cleans up resources
 *
 * @section usage Usage Pattern
 * @code
 * // Created automatically by RTSPServer for each client
 * RtspSession session(streamer, clientSocket);
 *
 * // Process requests until client disconnects
 * while (session.handleRequests(timeout)) {
 *   // Session active, continue processing
 * }
 * // Session ended, cleanup handled automatically
 * @endcode
 *
 * @note This class is typically instantiated by RTSPServer, not directly by
 * users
 * @note Requires a configured RTSPAudioStreamer for media delivery
 * @author Thomas Pfitzinger
 * @ingroup rtsp
 * @version 0.2.0
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
  RtspSession(Platform::TcpClientType& aClient,
              RTSPAudioStreamer<Platform>& aStreamer)
      : m_Client(aClient), m_Streamer(&aStreamer) {
    m_RtspClient = &m_Client;
    m_RtspSessionID = random(65536);  // create a session ID
    log_i("RTSP session created");
  }

  /**
   * @brief Destructor - cleanup session resources
   *
   * Closes the RTSP client socket.
   * Vector buffers are automatically managed.
   *
   * @note UDP transport cleanup is handled separately by the streamer
   */
  ~RtspSession() {
    // m_Streamer->releaseUdpTransport();
    Platform::closeSocket(m_RtspClient);
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
    log_v("handleRequests");
    // initlaize buffers and state if not already done
    init();

    if (m_stopped) return false;  // Already closed down

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
        // TODO this should go in the handling functions
        if (C == RTSP_PLAY)
          m_streaming = true;
        else if (C == RTSP_TEARDOWN) {
          m_stopped = true;
        }
      }
      return true;
    } else if (res == 0) {
      log_w("client closed socket, exiting");
      m_stopped = true;
      return true;
    } else {
      // Timeout on read
      // printf("RTSP read timeout\n");
      return false;
    }
  }

  bool isSessionOpen() { return m_sessionOpen; }

 protected:
  const char* STD_URL_PRE_SUFFIX = "trackID";

  // global session state parameters
  int m_RtspSessionID;
  Platform::TcpClientType m_Client;
  typename Platform::TcpClientType*
      m_RtspClient;           // RTSP socket of that session
  int m_StreamID = -1;        // number of simulated stream of that session
  uint16_t m_ClientRTPPort;   // client port for UDP based RTP transport
  uint16_t m_ClientRTCPPort;  // client port for UDP based RTCP transport
  RTSPAudioStreamer<Platform>* m_Streamer =
      nullptr;  // the UDP streamer of that session

  // parameters of the last received RTSP request
  RTSP_CMD_TYPES m_RtspCmdType;  // command type (if any) of the current request
  audio_tools::Vector<char> m_URLPreSuffix;  // stream name pre suffix
  audio_tools::Vector<char> m_URLSuffix;     // stream name suffix
  audio_tools::Vector<char> m_CSeq;          // RTSP command sequence number
  audio_tools::Vector<char> m_URLHostPort;   // host:port part of the URL
  unsigned m_ContentLength;                  // SDP string size
  uint16_t m_RtpClientPort =
      0;  // RTP receiver port on client (in host byte order!)
  uint16_t m_RtcpClientPort =
      0;  // RTCP receiver port on client (in host byte order!)
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
  bool m_stopped = false;
  volatile bool m_sessionOpen = true;

  /**
   * Initializes memory and buffers
   *
   * Lazily initializes Vector buffers if they haven't been allocated yet,
   * and resets all RTSP parsing state variables to their default values.
   * This method can be called multiple times safely.
   */
  void init() {
    if (m_is_init) return;
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
    if (m_Response.size() == 0) {
      m_Response.resize(2251);
    }
    if (m_SDPBuf.size() == 0) {
      m_SDPBuf.resize(1024);
    }
    if (m_URLBuf.size() == 0) {
      m_URLBuf.resize(1024);
    }
    if (m_Buf1.size() == 0) {
      m_Buf1.resize(256);
    }
    if (m_Buf2.size() == 0) {
      m_Buf2.resize(256);
    }
    if (m_CmdName.size() == 0) {
      m_CmdName.resize(RTSP_PARAM_STRING_MAX);
    }

    m_RtspCmdType = RTSP_UNKNOWN;
    memset(m_URLPreSuffix.data(), 0x00, m_URLPreSuffix.size());
    memset(m_URLSuffix.data(), 0x00, m_URLSuffix.size());
    memset(m_CSeq.data(), 0x00, m_CSeq.size());
    memset(m_URLHostPort.data(), 0x00, m_URLHostPort.size());
    m_ContentLength = 0;
    m_is_init = true;
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
    unsigned CurRequestSize;

    log_v("aRequest: ------------------------\n%s\n-------------------------",
          aRequest);

    CurRequestSize = aRequestSize;
    memcpy(mCurRequest.data(), aRequest, aRequestSize);

    // check whether the request contains information about the RTP/RTCP UDP
    // client ports (SETUP command)
    char* ClientPortPtr;
    char* TmpPtr;
    // static char CP[1024];
    char* CP = m_Response.data();  // reuse unused buffer
    memset(CP, 0, m_Response.size());
    char* pCP;

    ClientPortPtr = strstr(mCurRequest.data(), "client_port");
    if (ClientPortPtr != nullptr) {
      TmpPtr = strstr(ClientPortPtr, "\r\n");
      if (TmpPtr != nullptr) {
        TmpPtr[0] = 0x00;
        strcpy(CP, ClientPortPtr);
        pCP = strstr(CP, "=");
        if (pCP != nullptr) {
          pCP++;
          strcpy(CP, pCP);
          pCP = strstr(CP, "-");
          if (pCP != nullptr) {
            pCP[0] = 0x00;
            m_ClientRTPPort = atoi(CP);
            m_ClientRTCPPort = m_ClientRTPPort + 1;
          }
        }
      }
    }

    // Read everything up to the first space as the command name
    bool parseSucceeded = false;
    unsigned i;
    for (i = 0; i < m_CmdName.size() - 1 && i < CurRequestSize; ++i) {
      char c = mCurRequest[i];
      if (c == ' ' || c == '\t') {
        parseSucceeded = true;
        break;
      }
      m_CmdName[i] = c;
    }
    m_CmdName[i] = '\0';
    if (!parseSucceeded) {
      log_e("failed to parse RTSP");
      return false;
    }

    log_i("RTSP received %s", m_CmdName.data());

    // find out the command type
    if (strstr(m_CmdName.data(), "OPTIONS") != nullptr)
      m_RtspCmdType = RTSP_OPTIONS;
    else if (strstr(m_CmdName.data(), "DESCRIBE") != nullptr)
      m_RtspCmdType = RTSP_DESCRIBE;
    else if (strstr(m_CmdName.data(), "SETUP") != nullptr)
      m_RtspCmdType = RTSP_SETUP;
    else if (strstr(m_CmdName.data(), "PLAY") != nullptr)
      m_RtspCmdType = RTSP_PLAY;
    else if (strstr(m_CmdName.data(), "TEARDOWN") != nullptr)
      m_RtspCmdType = RTSP_TEARDOWN;
    else
      log_e("Error: Unsupported Command received (%s)!", m_CmdName.data());

    // Skip over the prefix of any "rtsp://" or "rtsp:/" URL that follows:
    unsigned j = i + 1;
    while (j < CurRequestSize &&
           (mCurRequest[j] == ' ' || mCurRequest[j] == '\t'))
      ++j;  // skip over any additional white space
    for (; (int)j < (int)(CurRequestSize - 8); ++j) {
      if ((mCurRequest[j] == 'r' || mCurRequest[j] == 'R') &&
          (mCurRequest[j + 1] == 't' || mCurRequest[j + 1] == 'T') &&
          (mCurRequest[j + 2] == 's' || mCurRequest[j + 2] == 'S') &&
          (mCurRequest[j + 3] == 'p' || mCurRequest[j + 3] == 'P') &&
          mCurRequest[j + 4] == ':' && mCurRequest[j + 5] == '/') {
        j += 6;
        if (mCurRequest[j] == '/') {  // This is a "rtsp://" URL; skip over the
                                      // host:port part that follows:
          ++j;
          unsigned uidx = 0;
          while (j < CurRequestSize && mCurRequest[j] != '/' &&
                 mCurRequest[j] != ' ' &&
                 uidx < m_URLHostPort.size() -
                            1) {  // extract the host:port part of the URL here
            m_URLHostPort[uidx] = mCurRequest[j];
            uidx++;
            ++j;
          }
        } else
          --j;
        i = j;
        break;
      }
    }

    log_v("m_URLHostPort: %s", m_URLHostPort.data());

    // Look for the URL suffix (before the following "RTSP/"):
    parseSucceeded = false;
    for (unsigned k = i + 1; (int)k < (int)(CurRequestSize - 5); ++k) {
      if (mCurRequest[k] == 'R' && mCurRequest[k + 1] == 'T' &&
          mCurRequest[k + 2] == 'S' && mCurRequest[k + 3] == 'P' &&
          mCurRequest[k + 4] == '/') {
        while (--k >= i && mCurRequest[k] == ' ') {
        }
        unsigned k1 = k;
        while (k1 > i && mCurRequest[k1] != '=') --k1;
        if (k - k1 + 1 > m_URLSuffix.size()) {
          parseSucceeded = false;
        } else {
          unsigned n = 0, k2 = k1 + 1;

          while (k2 <= k) m_URLSuffix[n++] = mCurRequest[k2++];
          m_URLSuffix[n] = '\0';

          if (k1 - i > m_URLPreSuffix.size()) {
            parseSucceeded = false;
          } else {
            parseSucceeded = true;
          }

          n = 0;
          k2 = i + 1;
          while (k2 <= k1 - 1) m_URLPreSuffix[n++] = mCurRequest[k2++];
          m_URLPreSuffix[n] = '\0';
          i = k + 7;
        }

        break;
      }
    }
    log_v("m_URLSuffix: %s", m_URLSuffix.data());
    log_v("m_URLPreSuffix: %s", m_URLPreSuffix.data());
    log_v("URL Suffix parse succeeded: %i", parseSucceeded);

    // Look for "CSeq:", skip whitespace, then read everything up to the next \r
    // or \n as 'CSeq':
    parseSucceeded = false;
    for (j = i; (int)j < (int)(CurRequestSize - 5); ++j) {
      if (mCurRequest[j] == 'C' && mCurRequest[j + 1] == 'S' &&
          mCurRequest[j + 2] == 'e' && mCurRequest[j + 3] == 'q' &&
          mCurRequest[j + 4] == ':') {
        j += 5;
        while (j < CurRequestSize &&
               (mCurRequest[j] == ' ' || mCurRequest[j] == '\t'))
          ++j;
        unsigned n;
        for (n = 0; n < m_CSeq.size() - 1 && j < CurRequestSize; ++n, ++j) {
          char c = mCurRequest[j];
          if (c == '\r' || c == '\n') {
            parseSucceeded = true;
            break;
          }
          m_CSeq[n] = c;
        }
        m_CSeq[n] = '\0';
        break;
      }
    }
    log_v("Look for CSeq success: %i", parseSucceeded);
    if (!parseSucceeded) return false;

    // Also: Look for "Content-Length:" (optional)
    for (j = i; (int)j < (int)(CurRequestSize - 15); ++j) {
      if (mCurRequest[j] == 'C' && mCurRequest[j + 1] == 'o' &&
          mCurRequest[j + 2] == 'n' && mCurRequest[j + 3] == 't' &&
          mCurRequest[j + 4] == 'e' && mCurRequest[j + 5] == 'n' &&
          mCurRequest[j + 6] == 't' && mCurRequest[j + 7] == '-' &&
          (mCurRequest[j + 8] == 'L' || mCurRequest[j + 8] == 'l') &&
          mCurRequest[j + 9] == 'e' && mCurRequest[j + 10] == 'n' &&
          mCurRequest[j + 11] == 'g' && mCurRequest[j + 12] == 't' &&
          mCurRequest[j + 13] == 'h' && mCurRequest[j + 14] == ':') {
        j += 15;
        while (j < CurRequestSize &&
               (mCurRequest[j] == ' ' || mCurRequest[j] == '\t'))
          ++j;
        unsigned num;
        if (sscanf(&mCurRequest[j], "%u", &num) == 1) m_ContentLength = num;
      }
    }

    return true;
  }

  /**
   * Sends Response to OPTIONS command
   */
  void handleRtspOption() {
    snprintf(m_Response.data(), m_Response.size(),
             "RTSP/1.0 200 OK\r\nCSeq: %s\r\n"
             "Public: DESCRIBE, SETUP, TEARDOWN, PLAY\r\n\r\n",
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
        m_Streamer->getAudioSource()->getFormat()->format(m_Buf2.data(), 256),
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

    log_v("handleRtspDescribe: %s", (const char*)m_Response.data());
    sendSocket(m_RtspClient, m_Response.data(), strlen(m_Response.data()));
  }

  /**
   * Sends Response to SETUP command and prepares RTP stream
   */
  void handleRtspSetup() {
    // init RTSP Session transport type (UDP or TCP) and ports for UDP transport
    initTransport(m_ClientRTPPort, m_ClientRTCPPort);

    // simulate SETUP server response
    snprintf(m_Buf1.data(), m_Buf1.size(),
             "RTP/"
             "AVP;unicast;destination=127.0.0.1;source=127.0.0.1;client_port=%"
             "i-%i;server_port=%i-%i",
             //"RTP/AVP;unicast;client_port=%i-%i;server_port=%i-%i",
             m_ClientRTPPort, m_ClientRTCPPort, m_Streamer->getRtpServerPort(),
             m_Streamer->getRtcpServerPort());
    snprintf(m_Response.data(), m_Response.size(),
             "RTSP/1.0 200 OK\r\n"
             "CSeq: %s\r\n"
             "%s\r\n"
             "Session: %i\r\n"
             "Transport: %s\r\n"
             "\r\n",
             m_CSeq.data(), dateHeader(), m_RtspSessionID, m_Buf1.data());

    sendSocket(m_RtspClient, m_Response.data(), strlen(m_Response.data()));
  }

  /**
   * Sends Response to PLAY command and starts the RTP stream
   */
  void handleRtspPlay() {
    // simulate SETUP server response
    snprintf(m_Response.data(), m_Response.size(),
             "RTSP/1.0 200 OK\r\n"
             "CSeq: %s\r\n"
             //"%s\r\n"
             "Range: npt=0.000-\r\n"  // this is necessary
             "Session: %i\r\n\r\n"
             //"RTP-Info: url=rtsp://127.0.0.1:8554/%s=0\r\n\r\n",          //
             // TODO whats this
             ,
             m_CSeq.data(),
             // DateHeader(),
             m_RtspSessionID
             // STD_URL_PRE_SUFFIX
    );

    sendSocket(m_RtspClient, m_Response.data(), strlen(m_Response.data()));

    m_Streamer->start();
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
