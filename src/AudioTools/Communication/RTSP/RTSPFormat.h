/*
 * Author: Phil Schatzmann
 *
 * Based on Micro-RTSP library:
 * https://github.com/geeksville/Micro-RTSP
 * https://github.com/Tomp0801/Micro-RTSP-Audio
 *
 */
#pragma once
#include "AudioTools/AudioCodecs/AudioCodecsBase.h"
#include "AudioTools/CoreAudio/AudioStreams.h"
#include "AudioTools/CoreAudio/AudioTypes.h"
#include "RTSPPlatform.h"
#include "stdint.h"

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
 * @author Phil Schatzmann
 */

class RTSPFormat {
 public:
  // Provides the format information: see see
  // https://en.wikipedia.org/wiki/RTP_payload_formats
  virtual const char *format(char *buffer, int len) = 0;

  // Data convertsion e.g. to network format. len is in samples
  virtual int convert(void *data, int sampleCount) { return sampleCount; }

  // Initialize with AudioInfo like data
  virtual void begin(AudioInfo info) { cfg = info; }
  // Provide a default config (must be overridden by concrete subclass)
  virtual AudioInfo defaultConfig() = 0;
  // Access current AudioInfo
  virtual AudioInfo audioInfo() { return cfg; }
  // Name accessors
  virtual const char *name() { return name_str; }
  /// Defines the name of the stream
  void setName(const char *name) { name_str = name; }

  /// Defines the fragment size in bytes
  void setFragmentSize(int fragmentSize) { fragment_size = fragmentSize; }

  /// Fragment (=write) size in bytes
  virtual int fragmentSize() { return fragment_size; }

  /// Fragment size in samples
  virtual int timestampIncrement() {
    int sample_size_bytes = cfg.bits_per_sample / 8;
    int samples_per_fragment =
        fragmentSize() / (sample_size_bytes * cfg.channels);
    return samples_per_fragment;
  }

  /// Defines the timer period in microseconds
  void setTimerPeriodUs(int period) { timer_period_us = period; }

  /// Timer period in microseconds
  virtual int timerPeriodUs() { return timer_period_us; }

  /// default dynamic
  virtual int rtpPayloadType() { return 96; }

  /// Optional header: e.g. rfc2250
  virtual int readHeader(uint8_t *data) { return 0; }

  /// Optional: Configure RFC2250 header usage (default: no-op)
  virtual void setUseRfc2250Header(bool /*enable*/) {}
  virtual bool useRfc2250Header() const { return false; }

 protected:
  const char *STD_URL_PRE_SUFFIX = "trackID";
  // for sample rate 16000
  int fragment_size = DEFAULT_PCM_FRAGMENT_SIZE;
  int timer_period_us = 10000;
  AudioInfo cfg{16000, 1, 16};
  const char *name_str = "RTSPAudioTools";
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
 * @see https://en.wikipedia.org/wiki/RTP_payload_formats
 * @note Automatically handles endian conversion for network transmission
 */
class RTSPFormatPCM : public RTSPFormat {
 public:
  RTSPFormatPCM(AudioInfo info, int fragmentSize = DEFAULT_PCM_FRAGMENT_SIZE) {
    cfg = info;
    setFragmentSize(fragmentSize);
    setTimerPeriodUs(getTimerPeriod(fragmentSize));
  }

  RTSPFormatPCM() {
    AudioInfo tmp(16000, 1, 16);  // Default: 16kHz mono 16-bit
    cfg = tmp;
    setFragmentSize(DEFAULT_PCM_FRAGMENT_SIZE);
    setTimerPeriodUs(getTimerPeriod(DEFAULT_PCM_FRAGMENT_SIZE));
  }

  void begin(AudioInfo info) {
    this->cfg = info;
    setTimerPeriodUs(getTimerPeriod(this->fragment_size));
  }

  /**
   * @brief Provide format 10 or 11
   *
   * @param buffer
   * @param len
   * @return const char*
   */
  const char *format(char *buffer, int len) override {
     int pt = rtpPayloadType();
     snprintf(buffer, len,
           "s=Microphone\r\n"
           "c=IN IP4 0.0.0.0\r\n"
           "t=0 0\r\n"
           "m=audio 0 RTP/AVP %d\r\n"
           "a=rtpmap:%d L16/%d/%d\r\n",
           pt, pt, sampleRate(), channels());
     LOGI("ftsp format: %s", buffer);
     return (const char *)buffer;
  }

  /**
   * @brief Convert to network format
   *
   * @param data
   * @param byteSize
   */
  int convert(void *data, int samples) {
    // convert to network format (big endian)
    int16_t *pt_16 = (int16_t *)data;
    for (int j = 0; j < samples / 2; j++) {
      pt_16[j] = htons(pt_16[j]);
    }
    return samples;
  }

  AudioInfo info() { return cfg; }

  AudioInfo defaultConfig() override { return AudioInfo(16000, 1, 16); }

  int rtpPayloadType() override {
    // Static assignments per RFC 3551 only valid for 44100Hz mono/stereo
    if (cfg.sample_rate == 44100) {
      if (channels() == 1) return 11; // L16 mono 44.1kHz
      if (channels() == 2) return 10; // L16 stereo 44.1kHz
    }
    return 96; // dynamic otherwise
  }

protected:

  int sampleRate() { return cfg.sample_rate; }
  int channels() { return cfg.channels; }
  int bytesPerSample() { return cfg.bits_per_sample / 8; }
  
  /**
   * @brief Get the timer period for streaming
   * @return Timer period in microseconds between audio packets
   */
  int getTimerPeriod(int fragmentSize) {
    // Calculate how many samples are in the fragment
    int samples_per_fragment = timestampIncrement();
    // Calculate how long it takes to play these samples at the given sample
    // rate
    int timer_period =
        (samples_per_fragment * 1000000) / cfg.sample_rate;  // microseconds
    return timer_period;
  }


};

/**
 * @brief Opus format for RTSP
 * https://en.wikipedia.org/wiki/RTP_payload_formats
 * @ingroup rtsp
 * @author Phil Schatzmann
 */
class RTSPFormatOpus : public RTSPFormat {
 public:
  RTSPFormatOpus() {
    setTimerPeriodUs(20000);  // 20ms standard for Opus
  }

  /// Determine timer duration from opus configuration
  RTSPFormatOpus(AudioEncoder &encoder) {
    p_encoder = &encoder;
    setTimerPeriodUs(encoder.frameDurationUs());  // Convert ms to us
  }

  // Provides the Opus format information:
  //  m=audio 54312 RTP/AVP 101
  //  a=rtpmap:101 opus/48000/2
  //  a=fmtp:101 stereo=1; sprop-stereo=1
  const char *format(char *buffer, int len) override {
    TRACEI();
    snprintf(buffer, len,
             "s=%s\r\n"              // Stream Name
             "c=IN IP4 0.0.0.0\r\n"  // Connection Information
             "t=0 0\r\n"  // start / stop - 0 -> unbounded and permanent session
             "m=audio 0 RTP/AVP 101\r\n"  // UDP sessions with format 101=opus
             "a=rtpmap:101 opus/%d/2\r\n"
             "a=fmtp:101 stereo=1; sprop-stereo=%d\r\n",
             name(), cfg.sample_rate, cfg.channels == 2);
    return (const char *)buffer;
  }
  AudioInfo defaultConfig() {
    AudioInfo cfg(48000, 2, 16);
    return cfg;
  }

  void begin(AudioInfo info) override {
    RTSPFormat::begin(info);
    // Update timer period based on encoder configuration if available
    if (p_encoder != nullptr) {
      // p_encoder->setAudioInfo(info);
      setTimerPeriodUs(p_encoder->frameDurationUs());  // Convert ms to us
    }
  }

 protected:
  AudioEncoder *p_encoder = nullptr;
};

/**
 * @brief abtX format for RTSP
 * https://en.wikipedia.org/wiki/RTP_payload_formats
 * @ingroup rtsp
 * @author Phil Schatzmann
 */
class RTSPFormatAbtX : public RTSPFormat {
 public:
  RTSPFormatAbtX() {
    setTimerPeriodUs(20000);  // Default 20ms, will be recalculated in begin()
  }

  // Provides the SBC format information:
  //    m=audio 5004 RTP/AVP 98
  //    a=rtpmap:98 aptx/44100/2
  //    a=fmtp:98 variant=standard; bitresolution=16;
  const char *format(char *buffer, int len) override {
    TRACEI();
    snprintf(buffer, len,
             "s=%s\r\n"              // Stream Name
             "c=IN IP4 0.0.0.0\r\n"  // Connection Information
             "t=0 0\r\n"  // start / stop - 0 -> unbounded and permanent session
             "m=audio 0 RTP/AVP 98\r\n"  // UDP sessions with format 98=aptx
             "a=rtpmap:98 aptx/%d/%d\r\n"
             "a=fmtp:98 variant=standard; bitresolution=%d\r\n",
             name(), cfg.sample_rate, cfg.channels, cfg.bits_per_sample);
    return (const char *)buffer;
  }
  AudioInfo defaultConfig() {
    AudioInfo cfg(44100, 2, 16);
    return cfg;
  }

  void begin(AudioInfo info) override {
    RTSPFormat::begin(info);
    // Calculate fragment-based timing for AptX
    if (fragmentSize() > 0 && cfg.sample_rate > 0 && cfg.channels > 0) {
      int bytesPerSample = (cfg.bits_per_sample + 7) / 8;
      int samplesPerFragment = fragmentSize() / (cfg.channels * bytesPerSample);
      int period = (samplesPerFragment * 1000000) / cfg.sample_rate;
      setTimerPeriodUs(period);
    }
  }
};

/**
 * @brief GSM format for RTSP
 * https://en.wikipedia.org/wiki/RTP_payload_formats
 * @ingroup rtsp
 * @author Phil Schatzmann
 */
class RTSPFormatGSM : public RTSPFormat {
 public:
  RTSPFormatGSM() {
    setTimerPeriodUs(20000);  // 20ms standard for GSM (160 samples at 8kHz)
  }

  /// Provides the GSM format information
  const char *format(char *buffer, int len) override {
    TRACEI();
    assert(cfg.sample_rate == 8000);
    assert(cfg.channels == 1);

    snprintf(buffer, len,
             "s=%s\r\n"              // Stream Name
             "c=IN IP4 0.0.0.0\r\n"  // Connection Information
             "t=0 0\r\n"  // start / stop - 0 -> unbounded and permanent session
             "m=audio 0 RTP/AVP 3\r\n"  // UDP sessions with format 3=GSM
             ,
             name());
    return (const char *)buffer;
  }

  AudioInfo defaultConfig() {
    AudioInfo cfg(8000, 1, 16);
    return cfg;
  }
};

/**
 * @brief G711 μ-Law format for RTSP
 * https://en.wikipedia.org/wiki/RTP_payload_formats
 * Packet intervall: 20, frame size: any
 * @ingroup rtsp
 * @author Phil Schatzmann
 */
class RTSPFormatG711 : public RTSPFormat {
 public:
  RTSPFormatG711(bool isUlaw) {
    setTimerPeriodUs(20000);  // 20ms standard for G.711 (160 samples at 8kHz)
    setIsULaw(isUlaw);
  }

  /// Provides the G711  format information
  const char *format(char *buffer, int len) override {
    TRACEI();
    assert(cfg.sample_rate == 8000);
    assert(cfg.channels == 1);

    snprintf(
        buffer, len,
        "s=%s\r\n"              // Stream Name
        "c=IN IP4 0.0.0.0\r\n"  // Connection Information
        "t=0 0\r\n"  // start / stop - 0 -> unbounded and permanent session
        "m=audio 0 RTP/AVP %d\r\n"  // UDP sessions with format 0=G711 μ-Law
        ,
        name(), getFormat());
    return (const char *)buffer;
  }

  /// Defines if we use ulow ar alow; by default we use ulaw!
  void setIsULaw(bool flag) { is_ulaw = flag; }

  AudioInfo defaultConfig() {
    AudioInfo cfg(8000, 1, 16);
    return cfg;
  }

 protected:
  bool is_ulaw = true;
  uint8_t getFormat() { return is_ulaw ? 0 : 8; }
};

/**
 * @brief L8 format for RTSP
 * https://en.wikipedia.org/wiki/RTP_payload_formats
 * @ingroup rtsp
 * @author Phil Schatzmann
 */
class RTSPFormatPCM8 : public RTSPFormat {
 public:
  RTSPFormatPCM8() {
    setTimerPeriodUs(20000);  // Default 20ms, will be recalculated in begin()
  }

  const char *format(char *buffer, int len) override {
    TRACEI();
    snprintf(buffer, len,
             "s=%s\r\n"              // Stream Name
             "c=IN IP4 0.0.0.0\r\n"  // Connection Information
             "t=0 0\r\n"  // start / stop - 0 -> unbounded and permanent session
             "m=audio 0 RTP/AVP 96\r\n"  // UDP sessions with format 96=dynamic
             "a=rtpmap:96 l8/%d/%d\r\n",
             name(), cfg.sample_rate, cfg.channels);
    return (const char *)buffer;
  }
  AudioInfo defaultConfig() {
    AudioInfo cfg(16000, 2, 8);
    return cfg;
  }

  void begin(AudioInfo info) override {
    RTSPFormat::begin(info);
    // Calculate fragment-based timing for L8 (8-bit PCM)
    if (fragmentSize() > 0 && cfg.sample_rate > 0 && cfg.channels > 0) {
      int bytesPerSample = 1;  // 8-bit = 1 byte per sample
      int samplesPerFragment = fragmentSize() / cfg.channels;
      int period = (samplesPerFragment * 1000000) / cfg.sample_rate;
      setTimerPeriodUs(period);
    }
  }
};

/**
 * @brief RTSP/RTP formatter for mono IMA ADPCM (DVI4)
 *
 * Maps supported sample rates (8k/16k/11.025k/22.05k) to the RFC 3551 static
 * payload types (5,6,16,17) and generates minimal SDP (rtpmap line). Falls
 * back to 8 kHz/PT 5 on unsupported rates.
 *
 * Timing: if an encoder is provided, uses its `frameDurationUs()`; otherwise
 * derives the period from `fragmentSize()` (2 samples per encoded byte) or
 * retains the default 20 ms.
 *
 * Template requirements (optional AudioEncoder): `begin()`,
 * `frameDurationUs()`, `blockSize()`, and `AudioInfoSupport` (for
 * `setAudioInfo()` / `audioInfo()`).
 *
 * @note Channels must be 1 (mono). Source `AudioInfo` bits describe original
 * PCM width (e.g. 16) before 4‑bit ADPCM compression.
 * @ingroup rtsp
 */

template <class AudioEncoder>
class RTSPFormatADPCM : public RTSPFormat {
 public:
  RTSPFormatADPCM() {
    setTimerPeriodUs(20000);  // Default 20ms, will be recalculated in begin()
  }

  RTSPFormatADPCM(AudioEncoder &encoder) {
    p_encoder = &encoder;
    encoder.begin();
    setTimerPeriodUs(encoder.frameDurationUs());  // Convert ms to us
    setFragmentSize(encoder.blockSize());
  }

  int timerPeriodUs() {
    if (p_encoder != nullptr) return p_encoder->frameDurationUs();
    return RTSPFormat::timerPeriodUs();
  }

  // Provides the IMA ADPCM format information
  // See RFC 3551 for details
  const char *format(char *buffer, int len) override {
    TRACEI();
    // Only certain sample rates are valid for DVI4 (IMA ADPCM)
    int sr = cfg.sample_rate;
    int payload_type = 5;  // default to 8000 Hz
    switch (sr) {
      case 8000:
        payload_type = 5;
        break;
      case 16000:
        payload_type = 6;
        break;
      case 11025:
        payload_type = 16;
        break;
      case 22050:
        payload_type = 17;
        break;
      default:
        LOGE("Unsupported sample rate for IMA ADPCM: %d", sr);
        sr = 8000;
        payload_type = 5;
        break;
    }
    snprintf(buffer, len,
             "s=%s\r\n"
             "c=IN IP4 0.0.0.0\r\n"
             "t=0 0\r\n"
             "m=audio 0 RTP/AVP %d\r\n"
             "a=rtpmap:%d DVI4/%d\r\n",
             name(), payload_type, payload_type, sr);
    return (const char *)buffer;
  }
  AudioInfo defaultConfig() {
    AudioInfo cfg(22050, 1, 16);  // 4 bits per sample for IMA ADPCM
    return cfg;
  }

  void begin(AudioInfo info) override {
    RTSPFormat::begin(info);
    if (p_encoder != nullptr) {
      ((AudioInfoSupport *)p_encoder)->setAudioInfo(info);
      setTimerPeriodUs(p_encoder->frameDurationUs());  // Convert ms to us
    } else {
      // Calculate timing for ADPCM (4 bits per sample = 0.5 bytes per sample)
      if (fragmentSize() > 0 && cfg.sample_rate > 0) {
        int samplesPerFragment =
            fragmentSize() * 2;  // 2 samples per byte for 4-bit ADPCM
        int period = (samplesPerFragment * 1000000) / cfg.sample_rate;
        setTimerPeriodUs(period);
      }
    }
  }

  AudioInfo audioInfo() override {
    if (p_encoder != nullptr)
      return ((AudioInfoSupport *)p_encoder)->audioInfo();
    return RTSPFormat::audioInfo();
  }

 protected:
  AudioEncoder *p_encoder = nullptr;
};

/**
 * @brief MP3 format for RTSP
 * https://en.wikipedia.org/wiki/RTP_payload_formats
 * @ingroup rtsp
 * @author Phil Schatzmann
 */
class RTSPFormatMP3 : public RTSPFormat {
 public:
  RTSPFormatMP3() {
    setTimerPeriodUs(26122);  // ~26ms for MP3 frames (1152 samples at 44.1kHz)
    setFragmentSize(2884);    // Trigger read that is big enough
  }

  /// Provide dynamic frame duration if encoder is available
  RTSPFormatMP3(AudioEncoder &encoder) {
    setEncoder(encoder);
    setFragmentSize(2884);  // Trigger read that is big enough
    setTimerPeriodUs(encoder.frameDurationUs());  // Convert ms to us
  }

  void setEncoder(AudioEncoder &encoder) { p_encoder = &encoder; }

  int timerPeriodUs() {
    if (p_encoder != nullptr) return p_encoder->frameDurationUs();
    return RTSPFormat::timerPeriodUs();
  }

  virtual int timestampIncrement() {
    if (p_encoder != nullptr) return p_encoder->samplesPerFrame();
    // MP3 frame size is typically 1152 samples
    return 1152;
  }

  /// Provides the AudioInfo
  AudioInfo audioInfo() {
    if (p_encoder != nullptr)
      return ((AudioInfoSupport *)p_encoder)->audioInfo();
    return RTSPFormat::audioInfo();
  }

  // Provides the MP3 format information:
  //   m=audio 0 RTP/AVP 14
  //   a=rtpmap:14 MPA/90000[/ch]
  // See RFC 3551 for details: MP3 RTP clock is always 90kHz
  const char *format(char *buffer, int len) override {
    TRACEI();
    int payload_type = rtpPayloadType();  // RTP/AVP 14 = MPEG audio (MP3)
    int ch = cfg.channels;
    if (ch <= 0) ch = 1;
    // Include channels when not mono for clarity
    int ptime_ms =
        (cfg.sample_rate > 0) ? (int)((1152 * 1000) / cfg.sample_rate) : 26;
    if (ptime_ms < 10) ptime_ms = 10;  // clamp to a reasonable minimum
    if (ch == 1) {
      snprintf(buffer, len,
               "m=audio 0 RTP/AVP %d\r\n"
               "a=rtpmap:%d MPA/90000\r\n"
               "a=fmtp:%d layer=3\r\n"
               "a=ptime:%d\r\n",
               payload_type, payload_type, payload_type, ptime_ms);
    } else {
      snprintf(buffer, len,
               "m=audio 0 RTP/AVP %d\r\n"
               "a=rtpmap:%d MPA/90000/%d\r\n"
               "a=fmtp:%d layer=3\r\n"
               "a=ptime:%d\r\n",
               payload_type, payload_type, ch, payload_type, ptime_ms);
    }
    return (const char *)buffer;
  }

  int rtpPayloadType() override { return 14; }

  AudioInfo defaultConfig() {
    AudioInfo cfg(44100, 2, 16);  // Typical MP3 config
    return cfg;
  }

  void begin(AudioInfo info) override {
    RTSPFormat::begin(info);
    // MP3 frame size is typically 1152 samples
    int samplesPerFrame = 1152;
    if (cfg.sample_rate > 0) {
      int period = (samplesPerFrame * 1000000) / cfg.sample_rate;
      setTimerPeriodUs(period);
    }
  }

  /// rfc2250 header before the playload
  int readHeader(unsigned char *buffer) override {
    // Some clients (e.g., FFmpeg/ffplay) expect the optional RFC2250 4-byte
    // MPEG audio header for payload type 14, while others (e.g., VLC) expect
    // raw frames. Make this behavior configurable.
    if (use_rfc2250_header_) {
      memset(buffer, 0, 4);
      return 4;
    }
    (void)buffer;
    return 0;
  }

 protected:
  AudioEncoder *p_encoder = nullptr;
  bool use_rfc2250_header_ = false;

 public:
  // Enable/disable RFC2250 4-byte MPEG audio header in RTP payload
  void setUseRfc2250Header(bool enable) { use_rfc2250_header_ = enable; }
  bool useRfc2250Header() const override { return use_rfc2250_header_; }
};

/**
 * @brief AAC format for RTSP
 * https://en.wikipedia.org/wiki/RTP_payload_formats
 * See RFC 3640 for details
 * @ingroup rtsp
 * @author Phil Schatzmann
 */
class RTSPFormatAAC : public RTSPFormat {
 public:
  RTSPFormatAAC() {
    setTimerPeriodUs(23219);  // ~23ms for AAC frames (1024 samples at 44.1kHz)
  }

  // Provides the AAC format information:
  //   m=audio 0 RTP/AVP 96
  //   a=rtpmap:96 MPEG4-GENERIC/44100/2
  //   a=fmtp:96 streamtype=5; profile-level-id=1; mode=AAC-hbr; ...
  // For simplicity, only basic SDP lines are provided here
  const char *format(char *buffer, int len) override {
    TRACEI();
    int payload_type = 96;  // Dynamic payload type for AAC
    int sr = cfg.sample_rate;
    int ch = cfg.channels;
    snprintf(buffer, len,
             "s=%s\r\n"
             "c=IN IP4 0.0.0.0\r\n"
             "t=0 0\r\n"
             "m=audio 0 RTP/AVP %d\r\n"
             "a=rtpmap:%d MPEG4-GENERIC/%d/%d\r\n"
             "a=fmtp:%d streamtype=5; profile-level-id=1; mode=AAC-hbr;\r\n",
             name(), payload_type, payload_type, sr, ch, payload_type);
    return (const char *)buffer;
  }
  AudioInfo defaultConfig() {
    AudioInfo cfg(44100, 2, 16);  // Typical AAC config
    return cfg;
  }

  void begin(AudioInfo info) override {
    RTSPFormat::begin(info);
    // AAC frame size is typically 1024 samples
    int samplesPerFrame = 1024;
    if (cfg.sample_rate > 0) {
      int period = (samplesPerFrame * 1000000) / cfg.sample_rate;
      setTimerPeriodUs(period);
    }
  }
};

}  // namespace audio_tools