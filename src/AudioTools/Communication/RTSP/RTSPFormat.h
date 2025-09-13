#pragma once
#include "platform.h"
#include "stdint.h"
#include "AudioTools/CoreAudio/AudioTypes.h"

#define DEFAULT_PCM_FRAGMENT_SIZE 640

namespace audio_tools {

/**
 * @brief Audio Format Definition - Base class for RTSP audio formats
 *
 * The RTSPFormat class hierarchy defines audio format characteristics for RTSP
 * streaming, including SDP generation and RTP packaging parameters. This base
 * class provides the interface for:
 *
 * - SDP (Session Description Protocol) generation for RTSP DESCRIBE responses
 * - Audio data conversion (endianness, format transformations)
 * - RTP packaging parameters (fragment size, timing)
 * - Sample rate and channel configuration
 *
 * @section formats Supported Formats
 * - **RTSPFormatPCM**: Linear PCM audio (most common)
 * - Custom formats can be implemented by extending this class
 *
 * @section sdp SDP Format String
 * The format() method generates SDP media descriptions like:
 * ```
 * m=audio 0 RTP/AVP 11
 * a=rtpmap:11 L16/16000/1
 * ```
 *
 * @note Implementations must provide format() method for SDP generation
 * @ingroup rtsp
 * @author Thomas Pfitzinger
 * @version 0.1.1
 */

class RTSPFormat {
 public:
  // Provides the format information: see see
  // https://en.wikipedia.org/wiki/RTP_payload_formats
  virtual const char* format(char* buffer, int len) = 0;

  // Data convertsion e.g. to network format. len is in samples
  virtual int convert(void* data, int sampleCount) { return sampleCount; }

  /// Defines the fragment size in bytes
  void setFragmentSize(int fragmentSize) { fragment_size = fragmentSize; }

  /// Fragment (=write) size in bytes
  int fragmentSize() { return fragment_size; }

  /// Defines the timer period in microseconds
  void setTimerPeriod(int period) { timer_period_us = period; }

  /// Timer period in microseconds
  int timerPeriod() { return timer_period_us; }

 protected:
  const char* STD_URL_PRE_SUFFIX = "trackID";
  // for sample rate 16000
  int fragment_size = DEFAULT_PCM_FRAGMENT_SIZE;
  int timer_period_us = 10000;
};

/**
 * @brief Linear PCM Format for RTSP Streaming
 *
 * RTSPFormatPCM implements the RTSPFormat interface for linear PCM audio,
 * the most common uncompressed audio format. This class:
 *
 * - Generates SDP media descriptions for L16 (Linear 16-bit) RTP payload
 * - Handles byte order conversion from host to network format (big-endian)
 * - Calculates appropriate fragment sizes and timing for smooth streaming
 * - Supports arbitrary sample rates and channel configurations
 *
 * @section rtp RTP Payload Format
 * Uses RTP payload type 11 (L16) as defined in RFC 3551. The generated SDP
 * includes:
 * ```
 * m=audio 0 RTP/AVP 11
 * a=rtpmap:11 L16/44100/2
 * a=fmtp:11 ...
 * ```
 *
 * @section usage Usage Examples
 * @code
 * // Default: 16kHz mono 16-bit
 * RTSPFormatPCM format;
 *
 * // CD quality: 44.1kHz stereo 16-bit
 * AudioInfo cdInfo(44100, 2, 16);
 * RTSPFormatPCM cdFormat(cdInfo);
 *
 * // Custom configuration
 * RTSPFormatPCM customFormat(48000, 1, 16);
 * @endcode
 *
 * @see https://en.wikipedia.org/wiki/RTP_payload_formats
 * @note Automatically handles endian conversion for network transmission
 */
class RTSPFormatPCM : public RTSPFormat {
 public:
  RTSPFormatPCM(AudioInfo info, int fragmentSize = DEFAULT_PCM_FRAGMENT_SIZE) {
    m_info = info;
    setFragmentSize(fragmentSize);
    setTimerPeriod(getTimerPeriod(fragmentSize));
  }

  RTSPFormatPCM(){
    AudioInfo tmp(16000, 1, 16);  // Default: 16kHz mono 16-bit
    m_info = tmp;
    setFragmentSize(DEFAULT_PCM_FRAGMENT_SIZE);
    setTimerPeriod(getTimerPeriod(DEFAULT_PCM_FRAGMENT_SIZE));
  }

  void begin(AudioInfo info) {
    this->m_info = info;
    setTimerPeriod(getTimerPeriod(this->fragment_size));
  }

  /**
   * @brief Get the timer period for streaming
   * @return Timer period in microseconds between audio packets
   */
  int getTimerPeriod(int fragmentSize) {
    // Calculate how many samples are in the fragment
    int sample_size_bytes = m_info.bits_per_sample / 8;
    int samples_per_fragment = fragmentSize / (sample_size_bytes * m_info.channels);
    // Calculate how long it takes to play these samples at the given sample rate
    int timer_period = (samples_per_fragment * 1000000) / m_info.sample_rate;  // microseconds
    return timer_period;
  }

  /**
   * @brief Provide format 10 or 11
   *
   * @param buffer
   * @param len
   * @return const char*
   */
  const char* format(char* buffer, int len) override {
    snprintf(buffer, len,
             "s=Microphone\r\n"      // Stream Name
             "c=IN IP4 0.0.0.0\r\n"  // Connection Information
             "t=0 0\r\n"  // start / stop - 0 -> unbounded and permanent session
             "m=audio 0 RTP/AVP %d\r\n"  // UDP sessions with format 10 or 11
             "a=rtpmap:%s\r\n"
             "a=rate:%i\r\n",  // provide sample rate
             format(channels()), payloadFormat(sampleRate(), channels()),
             sampleRate());
    log_i("ftsp format: %s", buffer);
    return (const char*)buffer;
  }

  /**
   * @brief Convert to network format
   *
   * @param data
   * @param byteSize
   */
  int convert(void* data, int samples) {
    // convert to network format (big endian)
    int16_t* pt_16 = (int16_t*)data;
    for (int j = 0; j < samples / 2; j++) {
      pt_16[j] = htons(pt_16[j]);
    }
    return samples;
  }

  AudioInfo info() { return m_info; }

  int sampleRate() { return m_info.sample_rate; }
  int channels() { return m_info.channels; }
  int bytesPerSample() { return m_info.bits_per_sample / 8; }

 protected:
  AudioInfo m_info;
  char payload_fromat[30];

  const char* payloadFormat(int sampleRate, int channels) {
    // see https://en.wikipedia.org/wiki/RTP_payload_formats
    // 11 L16/%i/%i

    switch (channels) {
      case 1:
        snprintf(payload_fromat, 30, "%d L16/%i/%i", format(channels),
                 sampleRate, channels);
        break;
      case 2:
        snprintf(payload_fromat, 30, "%d L16/%i/%i", format(channels),
                 sampleRate, channels);
        break;
      default:
        log_e("unsupported audio type");
        break;
    }
    return payload_fromat;
  }

  int format(int channels) {
    int result = 0;
    switch (channels) {
      case 1:
        result = 11;
        break;
      case 2:
        result = 10;
        break;
      default:
        log_e("unsupported audio type");
        break;
    }
    return result;
  }
};

}  // namespace audio_tools