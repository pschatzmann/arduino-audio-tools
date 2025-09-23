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

#ifdef __linux__
#include "AudioTools/Concurrency/Desktop.h"
#else
#include "AudioTools/Concurrency/RTOS.h"
#endif
#include "AudioTools/CoreAudio/AudioBasic/Collections/Vector.h"
#include "AudioTools/CoreAudio/AudioTimer.h"
#include "IAudioSource.h"
#include "RTSPPlatform.h"

namespace audio_tools {

/**
 * @brief RTSPAudioStreamerBase - Core RTP Audio Streaming Engine
 *
 * The RTSPAudioStreamerBase class provides the fundamental RTP streaming
 * functionality without timer management. This base class handles:
 *
 * - Audio source initialization and management
 * - UDP transport setup for RTP/RTCP communications
 * - RTP packet formation with proper headers and sequencing
 * - Network format conversion (endianness handling)
 * - Manual packet transmission via sendRtpPacketDirect()
 *
 * @section technical Technical Details
 * - Uses RTP protocol with L16 payload format (16-bit linear PCM)
 * - Default streaming buffer size: 2048 bytes
 * - Supports multiple concurrent clients via reference counting
 * - Automatic port allocation starting from 6970
 *
 * @note This base class does not include timer functionality
 * @note Use RTSPAudioStreamer for automatic timer-driven streaming
 * @ingroup rtsp
 * @author Phil Schatzmann
 */
template <typename Platform>
class RTSPAudioStreamerBase {
 public:
  /**
   * @brief Default constructor for RTSPAudioStreamerBase
   *
   * Creates a new RTSPAudioStreamerBase instance with default configuration.
   * Initializes internal state including ports, sequence numbers, and
   * streaming parameters. No audio source is assigned - use setAudioSource()
   * to configure streaming source.
   *
   * @note Audio source must be set before streaming can begin
   * @see setAudioSource()
   */
  RTSPAudioStreamerBase() {
    LOGD("Creating RTSP Audio streamer base");
    m_RtpServerPort = 0;
    m_RtcpServerPort = 0;

    m_SequenceNumber = 0;
    m_Timestamp = 0;
    m_SendIdx = 0;

    m_RtpSocket = Platform::NULL_UDP_SOCKET;
    m_RtcpSocket = Platform::NULL_UDP_SOCKET;

    m_prevMsec = 0;

    m_udpRefCount = 0;

    m_useTcpInterleaved = false;
    m_RtspTcpSocket = Platform::NULL_TCP_SOCKET;
    m_TcpRtpChannel = 0;
    m_TcpRtcpChannel = 1;

    m_lastSamplesSent = 0;
  }

  /**
   * @brief Constructor with audio source
   *
   * Creates a new RTSPAudioStreamerBase instance and immediately configures it
   * with the specified audio source. This is equivalent to calling the default
   * constructor followed by setAudioSource().
   *
   * @param source Pointer to an object implementing the IAudioSource interface.
   *               The source provides audio data and format information for
   * streaming.
   * @note The audio source object must remain valid for the lifetime of the
   * RTSPAudioStreamerBase
   * @see IAudioSource
   */
  RTSPAudioStreamerBase(IAudioSource &source) : RTSPAudioStreamerBase() {
    setAudioSource(&source);
  }

  /**
   * @brief Destructor
   *
   * Cleans up allocated resources.
   * Automatically stops any active streaming.
   *
   * @note UDP sockets are managed separately via releaseUdpTransport()
   */
  virtual ~RTSPAudioStreamerBase() {
    // mRtpBuf is automatically managed by Vector
  }

  /**
   * @brief Configure the audio source for streaming
   *
   * Sets or changes the audio source that provides data for the RTP stream.
   * This method automatically initializes the audio source configuration
   * including fragment size and timer period based on the source's format
   * specifications.
   *
   * @param source Pointer to an IAudioSource implementation. Must not be
   * nullptr.
   * @note Calling this method will reinitialize audio source parameters
   * @see IAudioSource, initAudioSource()
   */
  virtual void setAudioSource(IAudioSource *source) {
    m_audioSource = source;
    initAudioSource();
    LOGI("RTSP Audio streamer created.  Fragment size: %i bytes",
         m_fragmentSize);
  }

  /**
   * @brief Initialize audio source configuration
   *
   * Extracts and caches configuration parameters from the audio source
   * including fragment size (bytes per packet) and timer period (microseconds
   * between packets). This method must be called before streaming can begin.
   *
   * @return true if initialization succeeded, false if audio source or format
   * is invalid
   * @note Fragment size determines how much audio data is sent per RTP packet
   * @note Timer period controls the streaming rate and must match the audio
   * sample rate
   */
  bool initAudioSource() {
    LOGI("initAudioSource");
    if (getAudioSource() == nullptr) {
      LOGE("audio_source is null");
      return false;
    }
    RTSPFormat &fmt = getAudioSource()->getFormat();
    m_payloadType = fmt.rtpPayloadType();
    m_fragmentSize = fmt.fragmentSize();
    m_timer_period_us = fmt.timerPeriodUs();
    LOGI("m_fragmentSize (bytes): %d", m_fragmentSize);
    return true;
  }

  /**
   * @brief Initialize UDP transport for RTP streaming
   *
   * Sets up UDP sockets for RTP and RTCP communication with a specific client.
   * Automatically allocates consecutive port pairs starting from 6970.
   * Supports multiple clients through reference counting - subsequent calls
   * with different clients will reuse existing sockets.
   *
   * @param aClientIP IP address of the RTP client that will receive the stream
   * @param aClientPort Port number on the client for receiving RTP packets
   * @return true if transport initialization succeeded, false on socket
   * creation failure
   *
   * @note Port allocation: RTP uses even ports (6970, 6972, ...), RTCP uses odd
   * ports (6971, 6973, ...)
   * @note Call releaseUdpTransport() when streaming to this client is complete
   * @see releaseUdpTransport(), getRtpServerPort(), getRtcpServerPort()
   */
  bool initUdpTransport(IPAddress aClientIP, uint16_t aClientPort) {
    m_ClientIP = aClientIP;
    m_ClientPort = aClientPort;

    m_SequenceNumber = random(65536);

    if (m_udpRefCount != 0) {
      ++m_udpRefCount;
      return true;
    }

    for (u_short P = 6970; P < 0xFFFE; P += 2) {
      m_RtpSocket = Platform::createUdpSocket(P);
      if (m_RtpSocket) {  // Rtp socket was bound successfully. Lets try to bind
                          // the consecutive Rtsp socket
        m_RtcpSocket = Platform::createUdpSocket(P + 1);
        if (m_RtcpSocket) {
          m_RtpServerPort = P;
          m_RtcpServerPort = P + 1;
          break;
        } else {
          Platform::closeUdpSocket(m_RtpSocket);
          Platform::closeUdpSocket(m_RtcpSocket);
        };
      }
    };
    ++m_udpRefCount;

    LOGI("RTP Streamer set up with client IP %s and client Port %i",
         m_ClientIP.toString().c_str(), m_ClientPort);

    // If client IP is unknown (0.0.0.0), try to learn it from an inbound UDP packet
    tryLearnClientFromUdp(true);

    return true;
  }

  /**
   * @brief Initialize TCP interleaved transport for RTP over RTSP
   *
   * Configures the streamer to send RTP packets using RTSP TCP interleaving.
   * RTP and RTCP packets are framed with '$', channel byte, and 2-byte length
   * per RFC 2326 section 10.12 and sent over the RTSP control socket.
   *
   * @param tcpSock Pointer to the RTSP TCP client socket
   * @param rtpChannel Interleaved channel number for RTP packets (e.g., 0)
   * @param rtcpChannel Interleaved channel number for RTCP packets (e.g., 1)
   */
  void initTcpInterleavedTransport(typename Platform::TcpClientType *tcpSock,
                                   int rtpChannel, int rtcpChannel) {
    m_RtspTcpSocket = tcpSock;
    m_TcpRtpChannel = rtpChannel;
    m_TcpRtcpChannel = rtcpChannel;
    m_useTcpInterleaved = true;
    m_SequenceNumber = random(65536);
    LOGI("Using RTP over RTSP TCP interleaved: ch=%d/%d", rtpChannel,
         rtcpChannel);
  }

  /**
   * @brief Release UDP transport resources
   *
   * Decrements the reference count for UDP socket usage and closes sockets
   * when no more clients are using them. This implements proper resource
   * management for multiple concurrent streaming sessions.
   *
   * @note Only closes sockets when reference count reaches zero
   * @note Always call this method when streaming to a client is complete
   * @see initUdpTransport()
   */
  void releaseUdpTransport(void) {
    --m_udpRefCount;
    if (m_udpRefCount == 0) {
      m_RtpServerPort = 0;
      m_RtcpServerPort = 0;
      Platform::closeUdpSocket(m_RtpSocket);
      Platform::closeUdpSocket(m_RtcpSocket);

      m_RtpSocket = Platform::NULL_UDP_SOCKET;
      m_RtcpSocket = Platform::NULL_UDP_SOCKET;
    }
  }

  /**
   * @brief Send a single RTP packet with audio data
   *
   * Constructs and sends one RTP packet containing audio data from the
   * configured source. This method handles the complete RTP packet creation
   * process including:
   * - RTP header construction with sequence numbers and timestamps
   * - Audio data retrieval from the source
   * - Network byte order conversion
   * - UDP transmission to the configured client
   *
   * @return Number of bytes sent in the packet payload, or negative value on
   * error
   * @retval -1 RTP buffer not allocated or audio source not configured
   *
   * @note This method is typically called periodically by the timer-driven
   * streaming
   * @note Automatically increments sequence numbers and updates timestamps
   * @see start(), timerCallback()
   */
  int sendRtpPacketDirect() {
    // check buffer
    if (mRtpBuf.size() == 0) {
      LOGE("mRtpBuf is empty");
      return -1;
    }

    // append data to header
    if (m_audioSource == nullptr) {
      LOGE("No audio source provided");
      return -1;
    }

    // unsigned char * dataBuf = &mRtpBuf[m_fragmentSize];
    if (m_fragmentSize + HEADER_SIZE >= STREAMING_BUFFER_SIZE) {
      LOGE(
          "STREAMIN_BUFFER_SIZE too small for the sampling rate: increase to "
          "%d",
          m_fragmentSize + HEADER_SIZE);
      return -1;
    }

    // Generic path (PCM, G711, etc.)
    memset(mRtpBuf.data(), 0, STREAMING_BUFFER_SIZE);
    buildRtpHeader();

    unsigned char *dataBuf = &mRtpBuf[HEADER_SIZE];
    int header_len = m_audioSource->getFormat().readHeader(dataBuf);
    int maxPayload = STREAMING_BUFFER_SIZE - HEADER_SIZE - header_len;
    dataBuf += header_len;

    int toRead = m_fragmentSize;
    if (toRead > maxPayload) {
      LOGW("Fragment exceeds payload capacity (%d > %d); clamping", toRead, maxPayload);
      toRead = maxPayload;
    }
    int bytesRead = m_audioSource->readBytes((void *)dataBuf, toRead);
    LOGI("Read %d bytes from audio source", bytesRead);
    int bytesNet = m_audioSource->getFormat().convert(dataBuf, bytesRead);

    // Compute samples sent for timestamp increment
    m_lastSamplesSent = m_audioSource->getFormat().timestampIncrement();
    m_SequenceNumber++;
    sendOut(HEADER_SIZE + bytesNet + header_len);
    return bytesNet;
  }

  /**
   * @brief Start audio source (base implementation)
   *
   * Begins the audio source and initializes the RTP buffer if not already done.
   * This base implementation does not start any timer - derived classes should
   * override this method to add timer functionality.
   *
   * @note UDP transport must be initialized before calling this method
   * @note Audio source must be configured before calling this method
   * @warning Will log errors if audio source is not properly configured
   * @see stop(), initUdpTransport(), setAudioSource()
   */
  virtual void start() {
    LOGI("Starting audio source (base)");

    if (mRtpBuf.size() == 0) {
      mRtpBuf.resize(STREAMING_BUFFER_SIZE + 1);
    }

    if (m_audioSource != nullptr) {
      initAudioSource();
      m_audioSource->start();
      LOGI("Audio source started - ready for manual streaming");
    } else {
      LOGE("No streaming source");
    }
  }

  /**
   * @brief Stop audio source (base implementation)
   *
   * Stops the audio source. This base implementation does not manage any
   * timer - derived classes should override this method to add timer
   * management.
   *
   * @note Does not release UDP transport or buffers - use releaseUdpTransport()
   * separately
   * @note Can be restarted by calling start() again
   * @see start(), releaseUdpTransport()
   */
  virtual void stop() {
    LOGI("Stopping audio source (base)");

    if (m_audioSource != nullptr) {
      m_audioSource->stop();
    }

    LOGI("Audio source stopped");
  }

  /**
   * @brief Get the RTP server port number
   * @return Port number used for RTP packet transmission, 0 if not initialized
   * @see initUdpTransport()
   */
  u_short getRtpServerPort() { return m_RtpServerPort; }

  /**
   * @brief Get the RTCP server port number
   * @return Port number used for RTCP communication, 0 if not initialized
   * @see initUdpTransport()
   */
  u_short getRtcpServerPort() { return m_RtcpServerPort; }

  /**
   * @brief Get the configured audio source
   * @return Pointer to the current audio source, nullptr if not set
   * @see setAudioSource()
   */
  IAudioSource *getAudioSource() { return m_audioSource; }

  /**
   * @brief Get the timer period in microseconds
   * @return Timer period configured from audio source format
   * @see initAudioSource()
   */
  uint32_t getTimerPeriodUs() const { return m_timer_period_us; }

  /**
   * @brief Get the timer period in milliseconds
   * @return Timer period in milliseconds for convenience
   * @see getTimerPeriodUs()
   */
  uint32_t getTimerPeriodMs() const { return m_timer_period_us / 1000; }

  /**
   * @brief Get current RTP sequence number that will be used in the next packet
   * @return Current RTP sequence number
   */
  uint16_t currentSeq() const { return m_SequenceNumber; }

  /**
   * @brief Get current RTP timestamp value that will be used in the next packet
   * @return Current RTP timestamp
   */
  uint32_t currentRtpTimestamp() const { return m_Timestamp; }

  /**
   * @brief Get current SSRC used in RTP header
   */
  uint32_t currentSsrc() const { return m_Ssrc; }

  /**
   * @brief Check if timer period has changed and update if necessary
   *
   * Checks the audio source format for timer period changes and updates
   * the cached value if different. This enables dynamic timer period
   * adjustments during playback when the audio format changes.
   *
   * @return true if timer period changed, false if unchanged or no audio source
   * @note Call this periodically to detect format changes during streaming
   * @see updateTimerPeriod()
   */
  bool checkTimerPeriodChange() {
    if (m_audioSource == nullptr) {
      return false;
    }

    uint32_t newPeriod = m_audioSource->getFormat().timerPeriodUs();
    if (newPeriod != m_timer_period_us && newPeriod > 0) {
      LOGI("Timer period changed from %u us to %u us",
           (unsigned)m_timer_period_us, (unsigned)newPeriod);
      m_timer_period_us = newPeriod;
      return true;
    }
    return false;
  }

  /**
   * @brief Static timer callback function for periodic RTP streaming
   *
   * This static method is called periodically by timers to send RTP packets.
   * It handles timing measurements and calls sendRtpPacketDirect() to transmit
   * audio data. Performance monitoring ensures the streaming keeps up with
   * real-time requirements.
   *
   * This method can be used by any derived class that implements timer-driven
   * streaming functionality.
   *
   * @param audioStreamerObj Void pointer to the RTSPAudioStreamerBase instance
   * (passed as callback parameter)
   * @note This is a static method suitable for use as a timer callback
   * @note Logs warnings if packet transmission takes too long
   * @see sendRtpPacketDirect()
   */
  static void timerCallback(void *audioStreamerObj) {
    LOGD("timerCallback");
    if (audioStreamerObj == nullptr) {
      LOGE("audioStreamerObj is null");
      return;
    };
    RTSPAudioStreamerBase<Platform> *streamer =
        (RTSPAudioStreamerBase<Platform> *)audioStreamerObj;
    unsigned long start, stop;

    start = micros();

    int bytes = streamer->sendRtpPacketDirect();

    if (bytes < 0) {
      LOGW("Direct sending of RTP stream failed");
    } else if (bytes > 0) {
      uint32_t inc = streamer->computeTimestampIncrement(bytes);
      streamer->m_Timestamp += inc;
      LOGD("%i samples (ts inc) sent; timestamp: %u", inc,
           (unsigned)streamer->m_Timestamp);
    }

    stop = micros();
    if (stop - start > streamer->m_timer_period_us) {
      LOGW("RTP Stream can't keep up (took %lu us, %d is max)!", stop - start,
           streamer->m_timer_period_us);
    }
  }

 protected:
  const int STREAMING_BUFFER_SIZE = 1024 * 3;
  audio_tools::Vector<uint8_t> mRtpBuf;

  IAudioSource *m_audioSource = nullptr;
  int m_fragmentSize = 0;  // changed from samples to bytes !
  int m_timer_period_us = 20000;
  const int HEADER_SIZE = 12;  // size of the RTP header
  volatile bool m_timer_restart_needed =
      false;  // Flag for dynamic timer restart

  typename Platform::UdpSocketType
      *m_RtpSocket;  // RTP socket for streaming RTP packets to client
  typename Platform::UdpSocketType
      *m_RtcpSocket;  // RTCP socket for sending/receiving RTCP packages

  uint16_t m_RtpServerPort;   // RTP sender port on server
  uint16_t m_RtcpServerPort;  // RTCP sender port on server

  u_short m_SequenceNumber;
  uint32_t m_Timestamp;
  int m_SendIdx;

  IPAddress m_ClientIP;
  uint16_t m_ClientPort;
  uint32_t m_prevMsec;

  int m_udpRefCount;

  // TCP interleaved transport
  bool m_useTcpInterleaved;
  typename Platform::TcpClientType *m_RtspTcpSocket;
  int m_TcpRtpChannel;
  int m_TcpRtcpChannel;

  int m_payloadType = 96;
  int m_lastSamplesSent = 0;
  uint32_t m_Ssrc = 0x13F97E67;  // default fixed SSRC
  // MP3 packetization carry buffer to ensure whole-frame packets
  audio_tools::Vector<uint8_t> mMp3Carry;
  int mMp3CarryLen = 0;

  /**
   * @brief Compute RTP timestamp increment based on samples sent
   *
   * Calculates the appropriate RTP timestamp increment based on the number of
   * audio samples sent in the last packet. This ensures proper timing and
   * synchronization in the RTP stream.
   *
   * @param bytesSent Number of bytes sent in the last RTP packet
   * @return Calculated timestamp increment value
   * @note Uses exact samples sent if available, otherwise falls back to audio
   * source format or a crude estimate
   */
  inline uint32_t computeTimestampIncrement(int bytesSent) {
    // Prefer exact samples sent (set by sender) if available
    int samples = 0;
    if (m_lastSamplesSent > 0) {
      samples = m_lastSamplesSent;
    } else if (m_audioSource) {
      samples = m_audioSource->getFormat().timestampIncrement();
    } else {
      samples = bytesSent / 2;  // crude fallback
    }
    return (uint32_t)samples;
  }

  inline void buildRtpHeader() {
    mRtpBuf[0] = 0x80;  // V=2
    mRtpBuf[1] = (uint8_t)(m_payloadType & 0x7F);
    if (m_payloadType == 14) {
      // Set Marker bit on each complete MP3 frame packet
      mRtpBuf[1] |= 0x80;
    }
    mRtpBuf[2] = (uint8_t)((m_SequenceNumber >> 8) & 0xFF);
    mRtpBuf[3] = (uint8_t)(m_SequenceNumber & 0xFF);
    mRtpBuf[4] = (uint8_t)((m_Timestamp >> 24) & 0xFF);
    mRtpBuf[5] = (uint8_t)((m_Timestamp >> 16) & 0xFF);
    mRtpBuf[6] = (uint8_t)((m_Timestamp >> 8) & 0xFF);
    mRtpBuf[7] = (uint8_t)(m_Timestamp & 0xFF);
    // SSRC
    mRtpBuf[8] = (uint8_t)((m_Ssrc >> 24) & 0xFF);
    mRtpBuf[9] = (uint8_t)((m_Ssrc >> 16) & 0xFF);
    mRtpBuf[10] = (uint8_t)((m_Ssrc >> 8) & 0xFF);
    mRtpBuf[11] = (uint8_t)(m_Ssrc & 0xFF);
  }

  inline void sendOut(uint16_t totalLen) {
    if (m_useTcpInterleaved && m_RtspTcpSocket != Platform::NULL_TCP_SOCKET) {
      LOGD("Sending TCP: %d", totalLen);
      uint8_t hdr[4];
      hdr[0] = 0x24;  // '$'
      hdr[1] = (uint8_t)m_TcpRtpChannel;
      hdr[2] = (uint8_t)((totalLen >> 8) & 0xFF);
      hdr[3] = (uint8_t)(totalLen & 0xFF);
      Platform::sendSocket(m_RtspTcpSocket, hdr, sizeof(hdr));
      Platform::sendSocket(m_RtspTcpSocket, mRtpBuf.data(), totalLen);
    } else {
      // If client IP is still unknown, attempt to learn it just-in-time
      tryLearnClientFromUdp(false);
      LOGI("Sending UDP: %d bytes (to %s:%d)", totalLen,
           m_ClientIP.toString().c_str(), m_ClientPort);
      Platform::sendUdpSocket(m_RtpSocket, mRtpBuf.data(), totalLen, m_ClientIP,
                              m_ClientPort);
    }
  }

  inline void tryLearnClientFromUdp(bool warnIfNone) {
    if (m_ClientIP == IPAddress(0, 0, 0, 0) && m_RtpSocket) {
      int avail = m_RtpSocket->parsePacket();
      if (avail > 0) {
        IPAddress learnedIp = m_RtpSocket->remoteIP();
        uint16_t learnedPort = m_RtpSocket->remotePort();
        if (learnedIp != IPAddress(0, 0, 0, 0)) {
          m_ClientIP = learnedIp;
          if (m_ClientPort == 0) m_ClientPort = learnedPort;
          LOGI("RTP learned client via UDP: %s:%u",
               m_ClientIP.toString().c_str(), (unsigned)m_ClientPort);
        }
      } else if (warnIfNone) {
        LOGW("Client IP unknown (0.0.0.0) and no inbound UDP yet");
      }
    }
  }
};

/**
 * @brief RTSPAudioStreamer - Timer-driven RTP Audio Streaming Engine
 *
 * The RTSPAudioStreamer class extends RTSPAudioStreamerBase with automatic
 * timer-driven streaming functionality. This class provides:
 *
 * - All base class functionality (audio source, UDP transport, RTP packets)
 * - Automatic periodic streaming using AudioTools TimerAlarmRepeating
 * - Timer safety configuration for ESP32 platforms
 * - Background streaming without manual intervention
 *
 * @note This is the recommended class for most use cases
 * @note Use RTSPAudioStreamerBase for custom streaming control
 * @ingroup rtsp
 * @author Phil Schatzmann
 */
template <typename Platform>
class RTSPAudioStreamer : public RTSPAudioStreamerBase<Platform> {
 public:
  /**
   * @brief Default constructor for RTSPAudioStreamer
   *
   * Creates a new RTSPAudioStreamer instance with timer functionality.
   * Initializes the base class and configures the timer for safe operation.
   *
   * @note Audio source must be set before streaming can begin
   * @see setAudioSource()
   */
  RTSPAudioStreamer() : RTSPAudioStreamerBase<Platform>() {
    LOGD("Creating RTSP Audio streamer with timer");

    // Setup timer callback for RTP streaming
    rtpTimer.setCallbackParameter(this);

    // CRITICAL FIX: Force timer to run in task context, not ISR context
    // ISR context (ESP_TIMER_ISR) has severe limitations that cause
    // LoadProhibited crashes Task context (ESP_TIMER_TASK) allows full object
    // access and FreeRTOS calls
    rtpTimer.setIsSave(
        true);  // true = use TimerCallbackInThread (ESP_TIMER_TASK)
    LOGI("RTSPAudioStreamer: Timer set to safe task mode (ESP_TIMER_TASK)");
  }

  /**
   * @brief Constructor with audio source
   *
   * Creates a new RTSPAudioStreamer instance with timer functionality and
   * immediately configures it with the specified audio source.
   *
   * @param source Reference to an object implementing the IAudioSource
   * interface
   * @see IAudioSource
   */
  RTSPAudioStreamer(IAudioSource &source) : RTSPAudioStreamer() {
    this->setAudioSource(&source);
  }

  /**
   * @brief Start timer-driven RTP streaming
   *
   * Begins the RTP streaming process by:
   * - Calling base class start() to initialize audio source and buffer
   * - Creating and configuring the TimerAlarmRepeating for periodic streaming
   * - Beginning periodic timer-driven packet transmission
   *
   * @note UDP transport must be initialized before calling this method
   * @note Audio source must be configured before calling this method
   * @warning Will log errors if audio source is not properly configured
   * @see stop(), initUdpTransport(), setAudioSource()
   */
  void start() override {
    LOGI("Starting RTP Stream with timer");

    // Call base class start to initialize audio source and buffer
    RTSPAudioStreamerBase<Platform>::start();

    if (this->m_audioSource != nullptr) {
      // Start timer with period in microseconds using specialized callback
      if (!rtpTimer.begin(RTSPAudioStreamerBase<Platform>::timerCallback,
                          this->m_timer_period_us, audio_tools::US)) {
        LOGE("Could not start timer");
      }
      LOGI("timer: %u us", (unsigned)this->m_timer_period_us);
#ifdef ESP32
      LOGI("Free heap size: %i KB", esp_get_free_heap_size() / 1000);
#endif
    }
  }

  /**
   * @brief Update timer period if audio format has changed
   *
   * Checks for timer period changes and updates the timer if needed.
   * Call this periodically to enable dynamic timer period adjustments.
   *
   * @note Safe to call during streaming - timer restarts with new period
   */
  void updateTimer() {
    if (this->checkTimerPeriodChange()) {
      LOGI("Updating timer period to %u us", (unsigned)this->m_timer_period_us);
      rtpTimer.begin(this->m_timer_period_us, audio_tools::US);
    }
  }

  /**
   * @brief Stop timer-driven RTP streaming
   *
   * Stops the RTP streaming process by:
   * - Stopping the TimerAlarmRepeating to cease packet transmission
   * - Calling base class stop() to stop the audio source
   *
   * @note Does not release UDP transport or buffers - use
   * releaseUdpTransport() separately
   * @note Can be restarted by calling start() again
   * @see start(), releaseUdpTransport()
   */
  void stop() override {
    LOGI("Stopping RTP Stream with timer");

    // Stop timer first to prevent callbacks during cleanup
    rtpTimer.end();

    // Add small delay to ensure any running callbacks complete
    delay(50);

    // Call base class stop to stop audio source
    RTSPAudioStreamerBase<Platform>::stop();

    LOGI("RTP Stream stopped - ready for restart");
  }

 protected:
  audio_tools::TimerAlarmRepeating rtpTimer;
};

/**
 * @brief RTSPAudioStreamerUsingTask - Task-driven RTP Audio Streaming Engine
 *
 * The RTSPAudioStreamerUsingTask class extends RTSPAudioStreamerBase with
 * AudioTools Task-driven streaming functionality. Instead of using hardware
 * timers, this class creates a dedicated task that continuously calls the
 * timer callback at the appropriate intervals. This approach provides:
 *
 * - All base class functionality (audio source, UDP transport, RTP packets)
 * - Task-based periodic streaming using AudioTools Task class
 * - More predictable scheduling compared to hardware timers
 * - Better resource control and priority management
 * - Reduced timer resource usage
 * - Optional throttling for timing control
 *
 * The throttling feature is particularly important when working with audio
 * sources that can provide data faster than the defined sampling rate, such
 * as file readers, buffer sources, or fast generators. Without throttling,
 * these sources would flood the network with packets faster than real-time
 * playback.
 *
 * @section technical Technical Details
 * - Inherits all base class functionality
 * - Uses AudioTools Task class for periodic packet transmission
 * - Configurable task priority, stack size, and CPU core affinity
 * - Optional throttling with microsecond precision timing
 * - Clean task lifecycle management with proper cleanup
 *
 * @note Useful when hardware timers are limited or need different scheduling
 * @note Requires FreeRTOS support (ESP32, etc.)
 * @note Throttled mode provides more accurate timing but uses more CPU
 * @ingroup rtsp
 * @author Phil Schatzmann
 */
template <typename Platform>
class RTSPAudioStreamerUsingTask : public RTSPAudioStreamerBase<Platform> {
 public:
  /**
   * @brief Default constructor for RTSPAudioStreamerUsingTask
   *
   * Creates a new RTSPAudioStreamerUsingTask instance with task
   * functionality. Initializes the base class and sets default task
   * parameters.
   *
   * @param throttled Enable precise timing control with microsecond delays.
   *                  Set to true when your audio source provides data faster
   *                  than the defined sampling rate (e.g., reading from
   * files, buffers, or fast generators). Set to false when the source
   *                  naturally produces data at the correct rate (e.g., ADC,
   *                  microphone input, or rate-limited streams).
   *                  Default: false for better performance.
   * @note Audio source must be set using setAudioSource() before streaming
   * @see setAudioSource(), setTaskParameters()
   */
  RTSPAudioStreamerUsingTask(bool throttled = true)
      : RTSPAudioStreamerBase<Platform>() {
    LOGD("Creating RTSP Audio streamer with task");
    m_taskRunning = false;
    m_throttled = throttled;
  }

  /**
   * @brief Constructor with audio source and throttling control
   *
   * Creates a new RTSPAudioStreamerUsingTask instance with task functionality
   * and immediately configures it with the specified audio source.
   *
   * @param source Reference to an object implementing the IAudioSource
   * interface. The source provides audio data and format information.
   * @param throttled Enable precise timing control with microsecond delays.
   *                  Set to true when your audio source provides data faster
   *                  than the defined sampling rate (e.g., reading from
   * files, buffers, or fast generators). Set to false when the source
   *                  naturally produces data at the correct rate (e.g., ADC,
   *                  microphone input, or rate-limited streams).
   *                  Default: false for better performance.
   * @note The audio source object must remain valid for the lifetime of the
   * streamer
   * @see IAudioSource, setTaskParameters()
   */
  RTSPAudioStreamerUsingTask(IAudioSource &source, bool throttled = true)
      : RTSPAudioStreamerUsingTask(throttled) {
    this->setAudioSource(&source);
  }

  /**
   * @brief Destructor
   *
   * Ensures the streaming task is properly stopped and cleaned up.
   */
  virtual ~RTSPAudioStreamerUsingTask() { stop(); }

  /**
   * @brief Set task parameters for streaming task
   *
   * Configure the AudioTools Task parameters before starting streaming.
   * These parameters control the task's resource usage and scheduling
   * behavior. Must be called before start() to take effect.
   *
   * @param stackSize Stack size in bytes for the streaming task (default:
   * 8192) Increase if experiencing stack overflow issues
   * @param priority Task priority (0-configMAX_PRIORITIES-1, default: 5)
   *                 Higher values = higher priority. Use with caution.
   * @param core CPU core to pin task to (default: -1 for any core)
   *             ESP32 only: 0 or 1 for specific core, -1 for automatic
   * @note Higher priority tasks can starve lower priority tasks
   * @note Core pinning can improve performance but reduces scheduling
   * flexibility
   * @warning Cannot change parameters while streaming is active
   */
  void setTaskParameters(uint32_t stackSize, uint8_t priority, int core = -1) {
    if (!m_taskRunning) {
      m_taskStackSize = stackSize;
      m_taskPriority = priority;
      m_taskCore = core;
      LOGI("Task parameters set: stack=%d bytes, priority=%d, core=%d",
           stackSize, priority, core);
    } else {
      LOGW("Cannot change task parameters while streaming is active");
    }
  }

  /**
   * @brief Start task-driven RTP streaming
   *
   * Begins the RTP streaming process by:
   * - Calling base class start() to initialize audio source and buffer
   * - Creating an AudioTools Task for periodic streaming
   * - Beginning task-driven packet transmission
   *
   * @note UDP transport must be initialized before calling this method
   * @note Audio source must be configured before calling this method
   * @warning Will log errors if audio source is not properly configured
   * @see stop(), initUdpTransport(), setAudioSource()
   */
  void start() override {
    LOGI("Starting RTP Stream with task");

    // Call base class start to initialize audio source and buffer
    RTSPAudioStreamerBase<Platform>::start();

    if (this->m_audioSource != nullptr && !m_taskRunning) {
      m_taskRunning = true;

      // Create AudioTools Task for streaming
      if (!m_streamingTask.create("RTSPStreaming", m_taskStackSize,
                                  m_taskPriority, m_taskCore)) {
        LOGE("Failed to create streaming task");
        m_taskRunning = false;
        return;
      }

      // Set reference to this instance for the task
      m_streamingTask.setReference(this);

      // Start the task with our streaming loop
      // Reset throttle window timing
      m_send_counter = 0;
      m_last_throttle_us = micros();

      if (m_streamingTask.begin([this]() { this->streamingTaskLoop(); })) {
        LOGI("Streaming task started successfully");
        LOGI("Task: stack=%d bytes, priority=%d, core=%d, period=%d us",
             m_taskStackSize, m_taskPriority, m_taskCore,
             this->m_timer_period_us);
#ifdef ESP32
        LOGI("Free heap size: %i KB", esp_get_free_heap_size() / 1000);
#endif
      } else {
        LOGE("Failed to start streaming task");
        m_taskRunning = false;
      }
    }
  }

  /**
   * @brief Stop task-driven RTP streaming
   *
   * Stops the RTP streaming process by:
   * - Signaling the streaming task to stop
   * - Ending the AudioTools Task
   * - Calling base class stop() to stop the audio source
   *
   * @note Does not release UDP transport or buffers - use
   * releaseUdpTransport() separately
   * @note Can be restarted by calling start() again
   * @see start(), releaseUdpTransport()
   */
  void stop() override {
    LOGI("Stopping RTP Stream with task");

    if (m_taskRunning) {
      // Signal task to stop
      m_taskRunning = false;

      // Stop the AudioTools Task
      m_streamingTask.end();

      // Small delay to ensure clean shutdown
      delay(50);
    }

    // Call base class stop to stop audio source
    RTSPAudioStreamerBase<Platform>::stop();

    LOGI("RTP Stream with task stopped - ready for restart");
  }

  /**
   * @brief Check if streaming task is currently running
   * @return true if task is active and streaming, false otherwise
   */
  bool isTaskRunning() const { return m_taskRunning; }

  /**
   * @brief Enable or disable throttled timing mode
   *
   * Throttled mode provides more precise timing by using microsecond-level
   * delays, but consumes more CPU resources. This is essential when your
   * audio source can provide data faster than the defined sampling rate.
   *
   * **When to enable throttling (isThrottled = true):**
   * - Audio source reads from files, buffers, or memory
   * - Fast audio generators that can produce data instantly
   * - Sources that don't naturally limit their data rate
   * - When precise timing is critical for synchronization
   *
   * **When to disable throttling (isThrottled = false):**
   * - Real-time sources like ADC or microphone input
   * - Sources that naturally produce data at the correct rate
   * - Battery-powered devices where CPU efficiency is important
   * - Rate-limited streams or network sources
   *
   * @param isThrottled true to enable precise timing with delays, false for
   * efficient timing
   * @note Can be changed while streaming is active
   * @note Throttled mode uses more CPU but prevents audio data overrun
   * @note Non-throttled mode is more efficient but requires rate-limited
   * sources
   */
  void setThrottled(bool isThrottled) { m_throttled = isThrottled; }

  /**
   * @brief Set a fixed inter-packet delay in non-throttled mode
   *
   * Disables microsecond-level throttling and applies a constant delay
   * between iterations of the streaming task using `delay()`. This is useful
   * when the audio source is naturally rate-limited and a small cooperative
   * sleep is sufficient to avoid busy looping.
   *
   * @param delayUs Fixed delay in milliseconds applied via `delay()`.
   * @note Calling this method sets throttled mode to false.
   * @see setThrottled(bool)
   */
  void setFixedDelayMs(uint32_t delayUs) {
    this->m_fixed_delay_ms = delayUs;
    m_throttled = false;
  }

  /**
   * @brief Set the throttle interval (number of sends before precise
   * correction)
   *
   * After each packet send, the streamer delays by a fixed millisecond value.
   * After `interval` sends, it applies a precise microsecond correction based
   * on the expected elapsed time (interval * timerPeriodUs) versus the actual
   * elapsed time, then resets the interval window.
   *
   * @param interval Number of sends per throttle window (e.g., 1000 or 10000)
   */
  void setThrottleInterval(uint32_t interval) {
    m_throttle_interval = interval;
  }

 protected:
  audio_tools::Task m_streamingTask;  ///< AudioTools task for streaming loop
  volatile bool m_taskRunning;        ///< Flag indicating if task is active
  uint32_t m_taskStackSize = 8192;  ///< Task stack size in bytes (8KB default)
  uint8_t m_taskPriority = 5;       ///< Task priority (5 = medium priority)
  int m_taskCore = -1;              ///< CPU core affinity (-1 = any core)
  bool m_throttled = true;          ///< Enable precise microsecond timing
  uint16_t m_fixed_delay_ms = 1;    ///< Fixed delay in milliseconds (if used)
  uint32_t m_throttle_interval =
      1000;  ///< Number of sends before precise correction
  uint32_t m_send_counter =
      0;  ///< Counts sends within the current throttle window
  unsigned long m_last_throttle_us =
      0;  ///< Start timestamp of current throttle window (micros)

  /**
   * @brief Main streaming task loop iteration
   *
   * This method contains a single iteration of the streaming loop that is
   * called continuously by the AudioTools Task. Each iteration:
   * 1. Records start time for performance monitoring
   * 2. Checks for timer period changes and adjusts if needed
   * 3. Calls the timer callback to send one RTP packet
   * 4. Optionally applies throttling delay for precise timing
   *
   * The task runs in its own context using the AudioTools Task class and
   * handles timing based on the throttling mode setting. Dynamic timer
   * period changes are handled seamlessly within the task loop.
   *
   * @note This method is called repeatedly by the task framework
   * @note Performance is monitored when throttling is enabled
   * @see timerCallback(), checkTimerPeriodChange()
   */
  void streamingTaskLoop() {
    LOGD("Streaming task loop iteration");

    auto iterationStartUs = micros();

    // Always call the timer callback to send RTP packet
    RTSPAudioStreamerBase<Platform>::timerCallback(this);

    // Apply throttling (fixed delay per send + periodic correction)
    applyThrottling(iterationStartUs);
  }

  /**
   * @brief Apply streaming throttling policy
   *
   * Implements the two-stage throttling:
   * - Always delay by `m_fixed_delay_ms` after each send
   * - Every `m_throttle_interval` sends, precisely correct drift to
   *   match `m_timer_period_us` pacing
   *
   * Resets the throttle window if the source format changes.
   *
   * @param iterationStartUs Start timestamp of this loop iteration (micros)
   */
  inline void applyThrottling(unsigned long iterationStartUs) {
    // Increment send counter and apply fixed delay after each send
    ++m_send_counter;
    delay(m_fixed_delay_ms);

    // If throttling by interval is enabled (m_throttled true), apply precise
    // correction periodically
    if (m_throttled && m_throttle_interval > 0) {
      // On timer period change, reset the throttle window to avoid drift
      if (this->checkTimerPeriodChange()) {
        LOGI("Timer period updated; resetting throttle window to %u us",
             (unsigned)this->m_timer_period_us);
        m_send_counter = 0;
        m_last_throttle_us = iterationStartUs;
        return;
      }

      if (m_send_counter >= m_throttle_interval) {
        // Expected elapsed time for N sends at configured period
        uint64_t expectedUs =
            (uint64_t)m_throttle_interval * (uint64_t)this->getTimerPeriodUs();
        unsigned long nowUs = micros();
        uint64_t actualUs = (uint64_t)(nowUs - m_last_throttle_us);

        if (actualUs < expectedUs) {
          uint32_t remainingUs = (uint32_t)(expectedUs - actualUs);
          // Sleep in ms then the remainder in us
          if (remainingUs >= 1000) {
            delay(remainingUs / 1000);
          }
          uint32_t remUs = remainingUs % 1000;
          if (remUs > 0) {
            delayMicroseconds(remUs);
          }
        } else if (actualUs > expectedUs + 1000) {
          LOGW("Throttling behind by %llu us over %u sends",
               (unsigned long long)(actualUs - expectedUs),
               (unsigned)m_throttle_interval);
        }

        // Reset window
        m_send_counter = 0;
        m_last_throttle_us = micros();
      }
    }
  }
};

}  // namespace audio_tools