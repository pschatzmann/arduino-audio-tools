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

#include "AudioTools/Concurrency/RTOS.h"
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
 * @section usage Usage Example
 * @code
 * // Create an audio source (implement IAudioSource interface)
 * MyAudioSource audioSource;
 *
 * // Create and configure the base streamer
 * RTSPAudioStreamerBase<RTSPPlatformWiFi> streamer(&audioSource);
 *
 * // Initialize UDP transport for a client
 * IPAddress clientIP(192, 168, 1, 100);
 * streamer.initUdpTransport(clientIP, 12345);
 *
 * // Start the audio source
 * streamer.start();
 *
 * // Manual streaming loop
 * while (streaming) {
 *   streamer.sendRtpPacketDirect();
 *   delay(streamer.getTimerPeriodMs());
 * }
 *
 * // Stop when done
 * streamer.stop();
 * streamer.releaseUdpTransport();
 * @endcode
 *
 * @section technical Technical Details
 * - Uses RTP protocol with L16 payload format (16-bit linear PCM)
 * - Default streaming buffer size: 2048 bytes
 * - Supports multiple concurrent clients via reference counting
 * - Automatic port allocation starting from 6970
 *
 * @note This base class does not include timer functionality
 * @note Use RTSPAudioStreamer for automatic timer-driven streaming
 * @author Thomas Pfitzinger
 * @version 0.2.0
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
    log_v("Creating RTSP Audio streamer base");
    m_RtpServerPort = 0;
    m_RtcpServerPort = 0;

    m_SequenceNumber = 0;
    m_Timestamp = 0;
    m_SendIdx = 0;

    m_RtpSocket = Platform::NULL_UDP_SOCKET;
    m_RtcpSocket = Platform::NULL_UDP_SOCKET;

    m_prevMsec = 0;

    m_udpRefCount = 0;
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
    log_i("RTSP Audio streamer created.  Fragment size: %i bytes",
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
    m_timer_period_us = getAudioSource()->getFormat()->timerPeriodUs();
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
   * @see start(), timerCallback()
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

    Platform::sendUdpSocket(m_RtpSocket, mRtpBuf.data(),
                            HEADER_SIZE + bytesRead1, m_ClientIP, m_ClientPort);

    return bytesRead1;
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
    log_i("Starting audio source (base)");

    if (mRtpBuf.size() == 0) {
      mRtpBuf.resize(STREAMING_BUFFER_SIZE + 1);
    }

    if (m_audioSource != nullptr) {
      initAudioSource();
      m_audioSource->start();
      log_i("Audio source started - ready for manual streaming");
    } else {
      log_e("No streaming source");
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
    log_i("Stopping audio source (base)");

    if (m_audioSource != nullptr) {
      m_audioSource->stop();
    }

    log_i("Audio source stopped");
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
    log_v("timerCallback");
    if (audioStreamerObj == nullptr) {
      log_e("audioStreamerObj is null");
      return;
    };
    RTSPAudioStreamerBase<Platform> *streamer =
        (RTSPAudioStreamerBase<Platform> *)audioStreamerObj;
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
    if (stop - start > streamer->m_timer_period_us) {
      log_w("RTP Stream can't keep up (took %lu us, %lu is max)!", stop - start,
            streamer->m_timer_period_us);
    }
    log_v("done");
  }

 protected:
  const int STREAMING_BUFFER_SIZE = 1024 * 2;
  audio_tools::Vector<uint8_t> mRtpBuf;

  IAudioSource *m_audioSource = nullptr;
  int m_fragmentSize = 0;  // changed from samples to bytes !
  int m_timer_period_us = 20000;
  const int HEADER_SIZE = 12;  // size of the RTP header

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
 * @section usage Usage Example
 * @code
 * // Create an audio source (implement IAudioSource interface)
 * MyAudioSource audioSource;
 *
 * // Create and configure the timer-driven streamer
 * RTSPAudioStreamer<RTSPPlatformWiFi> streamer(&audioSource);
 *
 * // Initialize UDP transport for a client
 * IPAddress clientIP(192, 168, 1, 100);
 * streamer.initUdpTransport(clientIP, 12345);
 *
 * // Start automatic streaming
 * streamer.start();
 *
 * // Streaming happens automatically in background
 * // ...
 *
 * // Stop when done
 * streamer.stop();
 * streamer.releaseUdpTransport();
 * @endcode
 *
 * @section technical Technical Details
 * - Inherits all base class functionality
 * - Uses TimerAlarmRepeating for periodic packet transmission
 * - Timer runs in safe task context (ESP_TIMER_TASK) to prevent crashes
 * - Performance monitoring and timing validation
 *
 * @note This is the recommended class for most use cases
 * @note Use RTSPAudioStreamerBase for custom streaming control
 * @author Thomas Pfitzinger
 * @version 0.2.0
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
    log_v("Creating RTSP Audio streamer with timer");

    // Setup timer callback for RTP streaming
    rtpTimer.setCallbackParameter(this);

    // CRITICAL FIX: Force timer to run in task context, not ISR context
    // ISR context (ESP_TIMER_ISR) has severe limitations that cause
    // LoadProhibited crashes Task context (ESP_TIMER_TASK) allows full object
    // access and FreeRTOS calls
    rtpTimer.setIsSave(
        true);  // true = use TimerCallbackInThread (ESP_TIMER_TASK)
    log_i("RTSPAudioStreamer: Timer set to safe task mode (ESP_TIMER_TASK)");
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
    log_i("Starting RTP Stream with timer");

    // Call base class start to initialize audio source and buffer
    RTSPAudioStreamerBase<Platform>::start();

    if (this->m_audioSource != nullptr) {
      // Start timer with period in microseconds
      if (!rtpTimer.begin(RTSPAudioStreamerBase<Platform>::timerCallback,
                          this->m_timer_period_us, audio_tools::US)) {
        log_e("Could not start timer");
      }
      log_i("timer: %u us", (unsigned)this->m_timer_period_us);
#ifdef ESP32
      log_i("Free heap size: %i KB", esp_get_free_heap_size() / 1000);
#endif
    }
  }

  /**
   * @brief Stop timer-driven RTP streaming
   *
   * Stops the RTP streaming process by:
   * - Stopping the TimerAlarmRepeating to cease packet transmission
   * - Calling base class stop() to stop the audio source
   *
   * @note Does not release UDP transport or buffers - use releaseUdpTransport()
   * separately
   * @note Can be restarted by calling start() again
   * @see start(), releaseUdpTransport()
   */
  void stop() override {
    log_i("Stopping RTP Stream with timer");

    // Stop timer first to prevent callbacks during cleanup
    rtpTimer.end();

    // Add small delay to ensure any running callbacks complete
    delay(50);

    // Call base class stop to stop audio source
    RTSPAudioStreamerBase<Platform>::stop();

    log_i("RTP Stream stopped - ready for restart");
  }

 protected:
  audio_tools::TimerAlarmRepeating rtpTimer;
};

/**
 * @brief RTSPAudioStreamerUsingTask - Task-driven RTP Audio Streaming Engine
 *
 * The RTSPAudioStreamerUsingTask class extends RTSPAudioStreamerBase with
 * AudioTools Task-driven streaming functionality. Instead of using hardware
 * timers, this class creates a dedicated task that continuously calls the timer
 * callback at the appropriate intervals. This approach provides:
 *
 * - All base class functionality (audio source, UDP transport, RTP packets)
 * - Task-based periodic streaming using AudioTools Task class
 * - More predictable scheduling compared to hardware timers
 * - Better resource control and priority management
 * - Reduced timer resource usage
 * - Optional throttling for timing control
 * 
 * The throttling feature is particularly important when working with audio
 * sources that can provide data faster than the defined sampling rate, such as
 * file readers, buffer sources, or fast generators. Without throttling, these
 * sources would flood the network with packets faster than real-time playback.
 *
 * @section usage Usage Example
 * @code
 * // Create an audio source (implement IAudioSource interface)
 * MyAudioSource audioSource;
 *
 * // Create and configure the task-driven streamer with throttling
 * RTSPAudioStreamerUsingTask<RTSPPlatformWiFi> streamer(audioSource, true);
 *
 * // Configure task parameters (optional)
 * streamer.setTaskParameters(16384, 10, 1); // 16KB stack, priority 10, core 1
 *
 * // Initialize UDP transport for a client
 * IPAddress clientIP(192, 168, 1, 100);
 * streamer.initUdpTransport(clientIP, 12345);
 *
 * // Start task-based streaming
 * streamer.start();
 *
 * // Streaming happens automatically in background task
 * // ...
 *
 * // Stop when done
 * streamer.stop();
 * streamer.releaseUdpTransport();
 * @endcode
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
 * @author Thomas Pfitzinger
 * @version 0.2.0
 */
template <typename Platform>
class RTSPAudioStreamerUsingTask : public RTSPAudioStreamerBase<Platform> {
 public:
  /**
   * @brief Default constructor for RTSPAudioStreamerUsingTask
   *
   * Creates a new RTSPAudioStreamerUsingTask instance with task functionality.
   * Initializes the base class and sets default task parameters.
   *
   * @param throttled Enable precise timing control with microsecond delays.
   *                  Set to true when your audio source provides data faster
   *                  than the defined sampling rate (e.g., reading from files,
   *                  buffers, or fast generators). Set to false when the source
   *                  naturally produces data at the correct rate (e.g., ADC,
   *                  microphone input, or rate-limited streams).
   *                  Default: false for better performance.
   * @note Audio source must be set using setAudioSource() before streaming
   * @see setAudioSource(), setTaskParameters()
   */
  RTSPAudioStreamerUsingTask(bool throttled = false)
      : RTSPAudioStreamerBase<Platform>() {
    log_v("Creating RTSP Audio streamer with task");
    m_taskRunning = false;
    m_throttled = throttled;
  }

  /**
   * @brief Constructor with audio source and throttling control
   *
   * Creates a new RTSPAudioStreamerUsingTask instance with task functionality
   * and immediately configures it with the specified audio source.
   *
   * @param source Reference to an object implementing the IAudioSource interface.
   *               The source provides audio data and format information.
   * @param throttled Enable precise timing control with microsecond delays.
   *                  Set to true when your audio source provides data faster
   *                  than the defined sampling rate (e.g., reading from files,
   *                  buffers, or fast generators). Set to false when the source
   *                  naturally produces data at the correct rate (e.g., ADC,
   *                  microphone input, or rate-limited streams).
   *                  Default: false for better performance.
   * @note The audio source object must remain valid for the lifetime of the streamer
   * @see IAudioSource, setTaskParameters()
   */
  RTSPAudioStreamerUsingTask(IAudioSource &source, bool throttled = false)
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
   * These parameters control the task's resource usage and scheduling behavior.
   * Must be called before start() to take effect.
   *
   * @param stackSize Stack size in bytes for the streaming task (default: 8192)
   *                  Increase if experiencing stack overflow issues
   * @param priority Task priority (0-configMAX_PRIORITIES-1, default: 5)
   *                 Higher values = higher priority. Use with caution.
   * @param core CPU core to pin task to (default: -1 for any core)
   *             ESP32 only: 0 or 1 for specific core, -1 for automatic
   * @note Higher priority tasks can starve lower priority tasks
   * @note Core pinning can improve performance but reduces scheduling flexibility
   * @warning Cannot change parameters while streaming is active
   */
  void setTaskParameters(uint32_t stackSize, uint8_t priority, int core = -1) {
    if (!m_taskRunning) {
      m_taskStackSize = stackSize;
      m_taskPriority = priority;
      m_taskCore = core;
      log_i("Task parameters set: stack=%d bytes, priority=%d, core=%d",
            stackSize, priority, core);
    } else {
      log_w("Cannot change task parameters while streaming is active");
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
    log_i("Starting RTP Stream with task");

    // Call base class start to initialize audio source and buffer
    RTSPAudioStreamerBase<Platform>::start();

    if (this->m_audioSource != nullptr && !m_taskRunning) {
      m_taskRunning = true;

      // Create AudioTools Task for streaming
      if (!m_streamingTask.create("RTSPStreaming", m_taskStackSize,
                                  m_taskPriority, m_taskCore)) {
        log_e("Failed to create streaming task");
        m_taskRunning = false;
        return;
      }

      // Set reference to this instance for the task
      m_streamingTask.setReference(this);

      // Start the task with our streaming loop
      if (m_streamingTask.begin([this]() { this->streamingTaskLoop(); })) {
        log_i("Streaming task started successfully");
        log_i("Task: stack=%d bytes, priority=%d, core=%d, period=%d us",
              m_taskStackSize, m_taskPriority, m_taskCore,
              this->m_timer_period_us);
#ifdef ESP32
        log_i("Free heap size: %i KB", esp_get_free_heap_size() / 1000);
#endif
      } else {
        log_e("Failed to start streaming task");
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
   * @note Does not release UDP transport or buffers - use releaseUdpTransport()
   * separately
   * @note Can be restarted by calling start() again
   * @see start(), releaseUdpTransport()
   */
  void stop() override {
    log_i("Stopping RTP Stream with task");

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

    log_i("RTP Stream with task stopped - ready for restart");
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
   * delays, but consumes more CPU resources. This is essential when your audio
   * source can provide data faster than the defined sampling rate.
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
   * @param isThrottled true to enable precise timing with delays, false for efficient timing
   * @note Can be changed while streaming is active
   * @note Throttled mode uses more CPU but prevents audio data overrun
   * @note Non-throttled mode is more efficient but requires rate-limited sources
   */
  void setThrottled(bool isThrottled) { m_throttled = isThrottled; }

 protected:
  audio_tools::Task m_streamingTask;        ///< AudioTools task for streaming loop
  volatile bool m_taskRunning;              ///< Flag indicating if task is active
  uint32_t m_taskStackSize = 8192;          ///< Task stack size in bytes (8KB default)
  uint8_t m_taskPriority = 5;               ///< Task priority (5 = medium priority)
  int m_taskCore = -1;                      ///< CPU core affinity (-1 = any core)
  bool m_throttled = false;                 ///< Enable precise microsecond timing

  /**
   * @brief Main streaming task loop iteration
   *
   * This method contains a single iteration of the streaming loop that is
   * called continuously by the AudioTools Task. Each iteration:
   * 1. Records start time for performance monitoring
   * 2. Calls the timer callback to send one RTP packet
   * 3. Optionally applies throttling delay for precise timing
   *
   * The task runs in its own context using the AudioTools Task class and
   * handles timing based on the throttling mode setting.
   *
   * @note This method is called repeatedly by the task framework
   * @note Performance is monitored when throttling is enabled
   * @see timerCallback(), delayTask()
   */
  void streamingTaskLoop() {
    log_v("Streaming task loop iteration");

    auto startUs = micros();

    // Call the timer callback to send RTP packet
    RTSPAudioStreamerBase<Platform>::timerCallback(this);

    // Apply precise timing delay if throttling is enabled
    if (m_throttled) {
      delayTask(startUs);
    }
  }

  /**
   * @brief Apply precise timing delay for throttled mode
   *
   * Calculates and applies the appropriate delay to maintain precise timing
   * intervals between RTP packets. Uses a combination of millisecond and
   * microsecond delays for maximum accuracy.
   *
   * @param startUs Start time in microseconds for this iteration
   * @note Only called when throttling is enabled
   * @note Logs warnings if the task is running behind schedule
   * @warning High CPU usage due to precise timing requirements
   */
  inline void delayTask(unsigned long startUs) {
    // Calculate required delay based on audio format timing
    auto requiredDelayUs = this->getTimerPeriodUs();
    auto elapsedUs = micros() - startUs;
    
    if (elapsedUs < requiredDelayUs) {
      // Calculate remaining delay needed
      auto remainingDelayUs = requiredDelayUs - elapsedUs;
      auto delayMs = remainingDelayUs / 1000;
      
      // First apply millisecond delay
      if (delayMs > 0) {
        delay(delayMs);
      }
      
      // Recalculate elapsed time after millisecond delay
      auto currentElapsedUs = micros() - startUs;
      if (currentElapsedUs < requiredDelayUs) {
        auto finalDelayUs = requiredDelayUs - currentElapsedUs;
        if (finalDelayUs > 0) {
          delayMicroseconds(finalDelayUs);
        }
      }
    } else {
      // Task is running behind schedule
      log_w("Streaming task is running behind schedule by %lu us",
            elapsedUs - requiredDelayUs);
    }
  }
};

}  // namespace audio_tools