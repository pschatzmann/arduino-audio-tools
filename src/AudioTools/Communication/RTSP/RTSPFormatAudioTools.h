#pragma once

#include "AudioTools/CoreAudio/AudioStreams.h"
#include "RTSPFormat.h"

/**
 * @defgroup rtsp RTSP Streaming
 * @ingroup communications
 * @file RTSPFormatAudioTools.h
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

namespace audio_tools {

/**
 * @brief RTSPFormat which supports the AudioInfo class
 * @ingroup rtsp
 * @author Phil Schatzmann
 */
class RTSPFormatAudioTools : public RTSPFormat {
 public:
  virtual void begin(AudioInfo info) { cfg = info; }
  /// Provides the format string
  virtual const char *format(char *buffer, int len) = 0;
  /// Provide a default format that will just work
  virtual AudioInfo defaultConfig() = 0;
  /// Proides the name
  const char *name() { return name_str; }
  /// Defines the  name
  void setName(const char *name) { name_str = name; }

  void setFragmentSize(int fragmentSize) {
    RTSPFormat::setFragmentSize(fragmentSize);
  }
  int fragmentSize() { return RTSPFormat::fragmentSize(); }

  void setTimerPeriod(int period) { RTSPFormat::setTimerPeriodUs(period); }

  int timerPeriod() { return RTSPFormat::timerPeriodUs(); }

 protected:
  AudioInfo cfg;
  const char *name_str = "RTSP-Demo";
  ;
};

/**
 * @brief Opus format for RTSP
 * https://en.wikipedia.org/wiki/RTP_payload_formats
 * @ingroup rtsp
 * @author Phil Schatzmann
 */
class RTSPFormatOpus : public RTSPFormatAudioTools {
 public:
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
    AudioInfo cfg(44100, 2, 16);
    return cfg;
  }
};

/**
 * @brief abtX format for RTSP
 * https://en.wikipedia.org/wiki/RTP_payload_formats
 * @ingroup rtsp
 * @author Phil Schatzmann
 */
class RTSPFormatAbtX : public RTSPFormatAudioTools {
 public:
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
};

/**
 * @brief GSM format for RTSP
 * https://en.wikipedia.org/wiki/RTP_payload_formats
 * @ingroup rtsp
 * @author Phil Schatzmann
 */
class RTSPFormatGSM : public RTSPFormatAudioTools {
 public:
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
class RTSPFormatG711 : public RTSPFormatAudioTools {
 public:
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
 * @brief PCM format for RTSP
 * https://en.wikipedia.org/wiki/RTP_payload_formats
 * @ingroup rtsp
 * @author Phil Schatzmann
 */
class RTSPFormatAudioToolsPCM : public RTSPFormatAudioTools {
 public:
  /// Provides the GSM format information
  const char *format(char *buffer, int len) override {
    TRACEI();
    snprintf(buffer, len,
             "s=%s\r\n"              // Stream Name
             "c=IN IP4 0.0.0.0\r\n"  // Connection Information
             "t=0 0\r\n"  // start / stop - 0 -> unbounded and permanent session
             "m=audio 0 RTP/AVP %d\r\n"  // UDP sessions with format 10 or 11
             "a=rtpmap:%s\r\n"
             "a=rate:%i\r\n",  // provide sample rate
             name(), format(cfg.channels),
             payloadFormat(cfg.sample_rate, cfg.channels), cfg.sample_rate);
    return (const char *)buffer;
  }

  AudioInfo defaultConfig() {
    AudioInfo cfg(16000, 2, 16);
    return cfg;
  }

 protected:
  char payload_fromat[30];

  // format for mono is 11 and stereo 11
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
        LOGE("unsupported audio type");
        break;
    }
    return result;
  }

  const char *payloadFormat(int sampleRate, int channels) {
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
        LOGE("unsupported audio type");
        break;
    }
    return payload_fromat;
  }
};

/**
 * @brief L8 format for RTSP
 * https://en.wikipedia.org/wiki/RTP_payload_formats
 * @ingroup rtsp
 * @author Phil Schatzmann
 */
class RTSPFormatPCM8 : public RTSPFormatAudioTools {
 public:
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
};

/**
 * @brief ADPCM format for RTSP for the following mono
 * sample rates: 8000, 16000, 11025, 22050
 * https://en.wikipedia.org/wiki/RTP_payload_formats
 * @ingroup rtsp
 * @author Phil Schatzmann
 */

class RTSPFormatADPCM : public RTSPFormatAudioTools {
 public:
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
};

/**
 * @brief MP3 format for RTSP
 * https://en.wikipedia.org/wiki/RTP_payload_formats
 * @ingroup rtsp
 * @author Phil Schatzmann
 */
class RTSPFormatMP3 : public RTSPFormatAudioTools {
 public:
  // Provides the MP3 format information:
  //   m=audio 0 RTP/AVP 14
  //   a=rtpmap:14 MPEG/44100
  // See RFC 3551 for details
  const char *format(char *buffer, int len) override {
    TRACEI();
    int payload_type = 14;  // RTP/AVP 14 = MPEG audio (MP3)
    int sr = cfg.sample_rate;
    snprintf(buffer, len,
             "s=%s\r\n"
             "c=IN IP4 0.0.0.0\r\n"
             "t=0 0\r\n"
             "m=audio 0 RTP/AVP %d\r\n"
             "a=rtpmap:%d MPEG/%d\r\n",
             name(), payload_type, payload_type, sr);
    return (const char *)buffer;
  }
  AudioInfo defaultConfig() {
    AudioInfo cfg(44100, 2, 16);  // Typical MP3 config
    return cfg;
  }
};

/**
 * @brief AAC format for RTSP
 * https://en.wikipedia.org/wiki/RTP_payload_formats
 * See RFC 3640 for details
 * @ingroup rtsp
 * @author Phil Schatzmann
 */
class RTSPFormatAAC : public RTSPFormatAudioTools {
 public:
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
};

}  // namespace audio_tools