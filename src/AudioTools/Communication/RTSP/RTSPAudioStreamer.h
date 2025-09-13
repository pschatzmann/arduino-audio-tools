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

#include "AudioTools/CoreAudio/AudioBasic/Collections/Vector.h"
#include "AudioTools/CoreAudio/AudioTimer.h"
#include "IAudioSource.h"
#include "RTSPPlatform.h"

namespace audio_tools {

/**
 * @brief RTSPAudioStreamer - RTP Audio Streaming Engine
 *
 * The RTSPAudioStreamer class is responsible for converting audio data from an
 * IAudioSource into RTP (Real-time Transport Protocol) packets and streaming
 * them over UDP to RTSP clients. This class handles the complete RTP streaming
 * pipeline including:
 *
 * - Audio source initialization and management
 * - UDP transport setup for RTP/RTCP communications
 * - RTP packet formation with proper headers and sequencing
 * - Periodic streaming using AudioTools TimerAlarmRepeating
 * - Network format conversion (endianness handling)
 *
 * @section usage Usage Example
 * @code
 * // Create an audio source (implement IAudioSource interface)
 * MyAudioSource audioSource;
 *
 * // Create and configure the streamer
 * DefaultRTSPAudioStreamer streamer(&audioSource);
 *
 * // Initialize UDP transport for a client
 * IPAddress clientIP(192, 168, 1, 100);
 * streamer.initUdpTransport(clientIP, 12345);
 *
 * // Start streaming
 * streamer.start();
 *
 * // Stop when done
 * streamer.stop();
 * streamer.releaseUdpTransport();
 * @endcode
 *
 * @section technical Technical Details
 * - Uses RTP protocol with L16 payload format (16-bit linear PCM)
 * - Default streaming buffer size: 2048 bytes
 * - Timer period configurable via audio source format
 * - Supports multiple concurrent clients via reference counting
 * - Automatic port allocation starting from 6970
 *
 * @note This class is now platform-independent thanks to AudioTools
 * TimerAlarmRepeating and template-based platform abstraction
 * @author Thomas Pfitzinger
 * @version 0.1.1
 */
template <typename Platform = DefaultRTSPPlatform>
class RTSPAudioStreamer {
 protected:
  const int STREAMING_BUFFER_SIZE = 1024 * 2;
  audio_tools::Vector<uint8_t> mRtpBuf;

  IAudioSource *m_audioSource = nullptr;
  int m_fragmentSize = 0;  // changed from samples to bytes !
  int m_timer_period = 20000;
  const int HEADER_SIZE = 12;  // size of the RTP header

  typename Platform::UdpSocketType* m_RtpSocket;   // RTP socket for streaming RTP packets to client
  typename Platform::UdpSocketType* m_RtcpSocket;  // RTCP socket for sending/receiving RTCP packages

  uint16_t m_RtpServerPort;   // RTP sender port on server
  uint16_t m_RtcpServerPort;  // RTCP sender port on server

  u_short m_SequenceNumber;
  uint32_t m_Timestamp;
  int m_SendIdx;

  IPAddress m_ClientIP;
  uint16_t m_ClientPort;
  uint32_t m_prevMsec;

  int m_udpRefCount;

  audio_tools::TimerAlarmRepeating rtpTimer;

 public:
  /**
   * @brief Default constructor for RTSPAudioStreamer
   *
   * Creates a new RTSPAudioStreamer instance with default configuration.
   * Initializes internal state including ports, sequence numbers, and timer
   * configuration. No audio source is assigned - use setAudioSource() to
   * configure streaming source.
   *
   * @note Audio source must be set before streaming can begin
   * @see setAudioSource()
   */
  RTSPAudioStreamer() {
    log_v("Creating RTSP Audio streamer");
    m_RtpServerPort = 0;
    m_RtcpServerPort = 0;

    m_SequenceNumber = 0;
    m_Timestamp = 0;
    m_SendIdx = 0;

    m_RtpSocket = Platform::NULL_UDP_SOCKET;
    m_RtcpSocket = Platform::NULL_UDP_SOCKET;

    m_prevMsec = 0;

    m_udpRefCount = 0;

    // Setup timer callback for RTP streaming
    rtpTimer.setCallbackParameter(this);
  }

  /**
   * @brief Constructor with audio source
   *
   * Creates a new RTSPAudioStreamer instance and immediately configures it with the
   * specified audio source. This is equivalent to calling the default
   * constructor followed by setAudioSource().
   *
   * @param source Pointer to an object implementing the IAudioSource interface.
   *               The source provides audio data and format information for
   * streaming.
   * @note The audio source object must remain valid for the lifetime of the
   * RTSPAudioStreamer
   * @see IAudioSource
   */
  RTSPAudioStreamer(IAudioSource &source) : RTSPAudioStreamer() {
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
  virtual ~RTSPAudioStreamer() {
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
    log_i("RTSP Audio streamer created.  Fragment size: %i bytes", m_fragmentSize);
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
    log_i("initAudioSource");
    if (getAudioSource() == nullptr) {
      log_e("audio_source is null");
      return false;
    }
    if (getAudioSource()->getFormat() == nullptr) {
      log_e("fromat is null");
      return false;
    }
    m_fragmentSize = getAudioSource()->getFormat()->fragmentSize();
    m_timer_period = getAudioSource()->getFormat()->timerPeriod();
    log_i("m_fragmentSize (bytes): %d", m_fragmentSize);
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

    log_d("RTP Streamer set up with client IP %s and client Port %i",
          m_ClientIP.toString().c_str(), m_ClientPort);

    return true;
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
   * @see start(), doRtpStream()
   */
  int sendRtpPacketDirect() {
    // check buffer
    if (mRtpBuf.size() == 0) {
      log_e("mRtpBuf is empty");
      return -1;
    }

    // append data to header
    if (m_audioSource == nullptr) {
      log_e("No audio source provided");
      return -1;
    }

    // unsigned char * dataBuf = &mRtpBuf[m_fragmentSize];
    if (m_fragmentSize + HEADER_SIZE >= STREAMING_BUFFER_SIZE) {
      log_e(
          "STREAMIN_BUFFER_SIZE too small for the sampling rate: increase to "
          "%d",
          m_fragmentSize + HEADER_SIZE);
      return -1;
    }

    // Clear the buffer
    memset(mRtpBuf.data(), 0, STREAMING_BUFFER_SIZE);

    // Prepare the 12 byte RTP header TODO this can be optimized, some is static
    mRtpBuf[0] = 0x80;         // RTP version
    mRtpBuf[1] = 0x0b | 0x80;  // L16 payload (11) and no marker bit
    mRtpBuf[3] = m_SequenceNumber &
                 0x0FF;  // each packet is counted with a sequence counter
    mRtpBuf[2] = m_SequenceNumber >> 8;
    mRtpBuf[4] =
        (m_Timestamp & 0xFF000000) >> 24;  // each image gets a timestamp
    mRtpBuf[5] = (m_Timestamp & 0x00FF0000) >> 16;
    mRtpBuf[6] = (m_Timestamp & 0x0000FF00) >> 8;
    mRtpBuf[7] = (m_Timestamp & 0x000000FF);
    mRtpBuf[8] = 0x13;  // 4 byte SSRC (sychronization source identifier)
    mRtpBuf[9] = 0xf9;  // we just an arbitrary number here to keep it simple
    mRtpBuf[10] = 0x7e;
    mRtpBuf[11] = 0x67;

    // determine start of data in buffer
    // unsigned char * dataBuf = ((unsigned char*)mRtpBuf + HEADER_SIZE);
    unsigned char *dataBuf = &mRtpBuf[HEADER_SIZE];

    int bytesRead = m_audioSource->readBytes((void *)dataBuf, m_fragmentSize);

    // convert to network format (big endian)
    int bytesRead1 = m_audioSource->getFormat()->convert(dataBuf, bytesRead);

    // prepare the packet counter for the next packet
    m_SequenceNumber++;

    Platform::sendUdpSocket(m_RtpSocket, mRtpBuf.data(), HEADER_SIZE + bytesRead1,
                  m_ClientIP, m_ClientPort);

    return bytesRead1;
  }

  /**
   * @brief Start RTP streaming
   *
   * Begins the RTP streaming process by:
   * - Initializing the RTP buffer if not already done
   * - Creating and configuring the TimerAlarmRepeating for periodic streaming
   * - Starting the audio source
   * - Beginning periodic timer-driven packet transmission
   *
   * @note UDP transport must be initialized before calling this method
   * @note Audio source must be configured before calling this method
   * @warning Will log errors if audio source is not properly configured
   * @see stop(), initUdpTransport(), setAudioSource()
   */
  void start() {
    log_i("Starting RTP Stream");

    if (mRtpBuf.size() == 0) {
      mRtpBuf.resize(STREAMING_BUFFER_SIZE + 1);
    }

    if (m_audioSource != nullptr) {
      initAudioSource();
      m_audioSource->start();

      // Start timer with period in microseconds
      if (!rtpTimer.begin(RTSPAudioStreamer::doRtpStream, m_timer_period,
                          audio_tools::US)) {
        log_e("Could not start timer");
      }
      log_i("timer: %u us", (unsigned)m_timer_period);
#ifdef ESP32
      log_i("Free heap size: %i KB", esp_get_free_heap_size() / 1000);
#endif

    } else {
      log_e("No streaming source");
    }
  }

  /**
   * @brief Stop RTP streaming
   *
   * Stops the RTP streaming process by:
   * - Stopping the audio source
   * - Stopping the TimerAlarmRepeating to cease packet transmission
   *
   * @note Does not release UDP transport or buffers - use releaseUdpTransport()
   * separately
   * @note Can be restarted by calling start() again
   * @see start(), releaseUdpTransport()
   */
  void stop() {
    log_i("Stopping RTP Stream");
    if (m_audioSource != nullptr) {
      m_audioSource->stop();
    }
    rtpTimer.end();
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

 protected:
  /**
   * @brief Static timer callback function for periodic RTP streaming
   *
   * This static method is called periodically by the TimerAlarmRepeating to
   * send RTP packets. It handles timing measurements and calls
   * sendRtpPacketDirect() to transmit audio data. Performance monitoring
   * ensures the streaming keeps up with real-time requirements.
   *
   * @param audioStreamerObj Void pointer to the RTSPAudioStreamer instance (passed
   * as callback parameter)
   * @note This is a static method suitable for use as a TimerAlarmRepeating
   * callback
   * @note Logs warnings if packet transmission takes too long (>20ms)
   * @see start(), sendRtpPacketDirect()
   */
  static void doRtpStream(void *audioStreamerObj) {
    RTSPAudioStreamer<Platform> *streamer = (RTSPAudioStreamer<Platform> *)audioStreamerObj;
    unsigned long start, stop;

    start = micros();

    int bytes = streamer->sendRtpPacketDirect();
    int samples = bytes / 2;
    if (samples < 0) {
      log_w("Direct sending of RTP stream failed");
    } else if (samples > 0) {            // samples have been sent
      streamer->m_Timestamp += samples;  // no of samples sent
      log_v("%i samples sent (%ims); timestamp: %i", samples, samples / 16,
            streamer->m_Timestamp);
    }

    stop = micros();
    if (stop - start > 20000) {
      log_w("RTP Stream can't keep up (took %lu us, 20000 is max)!",
            stop - start);
    }
  }
};

// Default platform specialization for Arduino WiFi
using DefaultRTSPAudioStreamer = RTSPAudioStreamer<DefaultRTSPPlatform>;


} // namespace audio_tools