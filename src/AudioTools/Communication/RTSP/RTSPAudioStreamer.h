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
 * @brief RTSPAudioStreamerTaskless - Manual RTP Audio Streaming Engine (no Task)
 *
 * This class provides manual RTP streaming without any background task or timer.
 * Call doLoop() periodically (e.g., from Arduino loop()) to send packets.
 * Optionally supports throttling and fixed delay similar to RTSPAudioStreamerUsingTask.
 */
template <typename Platform>
class RTSPAudioStreamerTaskless : public RTSPAudioStreamerBase<Platform> {
 public:
  RTSPAudioStreamerTaskless(bool throttled = true)
      : RTSPAudioStreamerBase<Platform>(), m_throttled(throttled) {
    m_lastSendUs = 0;
    m_fixed_delay_ms = 1;
    m_throttle_interval = 50;
    m_send_counter = 0;
    m_last_throttle_us = 0;
  }
  RTSPAudioStreamerTaskless(IAudioSource &source, bool throttled = true)
      : RTSPAudioStreamerBase<Platform>(source), m_throttled(throttled) {
    m_lastSendUs = 0;
    m_fixed_delay_ms = 1;
    m_throttle_interval = 50;
    m_send_counter = 0;
    m_last_throttle_us = 0;
  }

  void setThrottled(bool isThrottled) { m_throttled = isThrottled; }
  void setFixedDelayMs(uint32_t delayMs) { m_fixed_delay_ms = delayMs; m_throttled = false; }
  void setThrottleInterval(uint32_t interval) { m_throttle_interval = interval; }

  void start() override {
    RTSPAudioStreamerBase<Platform>::start();
    m_lastSendUs = micros();
    m_send_counter = 0;
    m_last_throttle_us = micros();
  }

  void stop() override {
    RTSPAudioStreamerBase<Platform>::stop();
  }

  /**
   * @brief Call this in your Arduino loop() to send RTP packets at the correct rate
   */
  void doLoop() {
    unsigned long nowUs = micros();
    if (nowUs - m_lastSendUs >= this->getTimerPeriodUs()) {
      RTSPAudioStreamerBase<Platform>::timerCallback(this);
      m_lastSendUs = nowUs;
      applyThrottling(nowUs);
    }
  }

 private:
  void applyThrottling(unsigned long iterationStartUs) {
    ++m_send_counter;
    delay(m_fixed_delay_ms);
    if (m_throttled && m_throttle_interval > 0) {
      if (this->checkTimerPeriodChange()) {
        m_send_counter = 0;
        m_last_throttle_us = iterationStartUs;
        return;
      }
      if (m_send_counter >= m_throttle_interval) {
        uint64_t expectedUs = (uint64_t)m_throttle_interval * (uint64_t)this->getTimerPeriodUs();
        unsigned long nowUs = micros();
        uint64_t actualUs = (uint64_t)(nowUs - m_last_throttle_us);
        if (actualUs < expectedUs) {
          uint32_t remainingUs = (uint32_t)(expectedUs - actualUs);
          if (remainingUs >= 1000) delay(remainingUs / 1000);
          uint32_t remUs = remainingUs % 1000;
          if (remUs > 0) delayMicroseconds(remUs);
        }
        m_send_counter = 0;
        m_last_throttle_us = micros();
      }
    }
  }

  unsigned long m_lastSendUs;
  bool m_throttled;
  uint16_t m_fixed_delay_ms;
  uint32_t m_throttle_interval;
  uint32_t m_send_counter;
  unsigned long m_last_throttle_us;
};


}  // namespace audio_tools