#pragma once

#include "AudioStreamer.h"
#include "AudioTools/CoreAudio/AudioPlayer.h"
#include "AudioTools/CoreAudio/AudioStreams.h"
#include "IAudioSource.h"
#include "RTSPServer.h"

/**
 * @defgroup rtsp RTSP Streaming
 * @ingroup communications
 * @file RTSPOutput.h
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

namespace audio_tools {

/**
 * @brief PCMInfo subclass which provides the audio information from the related
 * AudioStream
 * Depends on the https://github.com/pschatzmann/Micro-RTSP-Audio/ library
 * @ingroup rtsp
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class RTSPOutputPCMInfo : public PCMInfo {
public:
  RTSPOutputPCMInfo() = default;
  virtual void begin(AudioStream &stream) { p_stream = &stream; }
  int getSampleRate() override { return p_stream->audioInfo().sample_rate; }
  int getChannels() override { return p_stream->audioInfo().channels; }
  int getSampleSizeBytes() override {
    return p_stream->audioInfo().bits_per_sample / 8;
  };
  virtual void setAudioInfo(AudioInfo ai) { p_stream->setAudioInfo(ai); }

protected:
  AudioStream *p_stream = nullptr;
};

/**
 * @brief PCMInfo subclass which provides the audio information from the
 * AudioInfo parameter
 * @ingroup rtsp
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class RTSPPCMAudioInfo : public PCMInfo {
public:
  RTSPPCMAudioInfo() = default;
  virtual void begin(AudioInfo info) { this->info = info; }
  int getSampleRate() override { return info.sample_rate; }
  int getChannels() override { return info.channels; }
  int getSampleSizeBytes() override { return info.bits_per_sample / 8; };
  virtual void setAudioInfo(AudioInfo ai) { info = ai; }

protected:
  AudioInfo info;
};

/**
 * @brief Simple Facade which can turn  AudioStream into a
 * IAudioSource. This way we can e.g. use an I2SStream as source to stream
 * data
 * Depends on the https://github.com/pschatzmann/Micro-RTSP-Audio/ library
 * @ingroup rtsp
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class RTSPSourceFromAudioStream : public IAudioSource {
public:
  RTSPSourceFromAudioStream() = default;
  /**
   * @brief Construct a new RTSPOutputSource object from an AudioStream
   *
   * @param stream
   */
  RTSPSourceFromAudioStream(AudioStream &stream) {
    setInput(stream);
    setFormat(new RTSPFormatPCM(pcmInfo));
  }

  RTSPSourceFromAudioStream(AudioStream &stream, RTSPFormat &format) {
    setInput(stream);
    setFormat(&format);
  }

  void setInput(AudioStream &stream) {
    p_audiostream = &stream;
    pcmInfo.begin(stream);
  }

  /**
   * @brief Set the Audio Info. This needs to be called if we just pass a
   * Stream. The AudioStream is usually able to provide the data from it's
   * original configuration.
   *
   * @param info
   */
  virtual void setAudioInfo(AudioInfo info) {
    TRACEI();
    p_audiostream->setAudioInfo(info);
  }

  /**
   * (Reads and) Copies up to maxSamples samples into the given buffer
   * @param dest Buffer into which the samples are to be copied
   * @param maxSamples maximum number of samples to be copied
   * @return actual number of samples that were copied
   */
  virtual int readBytes(void *dest, int byteCount) override {
    time_of_last_read = millis();
    int result = 0;
    LOGD("readDataTo: %d", byteCount);
    if (started) {
      result = p_audiostream->readBytes((uint8_t *)dest, byteCount);
    }
    return result;
  }
  // Provides the Audio Information
  virtual RTSPFormat *getFormat() override { return &format; }

  /**
   * Start preparing data in order to provide it for the stream
   */
  virtual void start() override {
    TRACEI();
    IAudioSource::start();
    p_audiostream->begin();
    started = true;
  };
  /**
   * Stop preparing data as the stream has ended
   */
  virtual void stop() override {
    TRACEI();
    IAudioSource::stop();
    started = false;
    p_audiostream->end();
  };

  void setFragmentSize(int fragmentSize) {
    format.setFragmentSize(fragmentSize);
  }

  void setTimerPeriod(int period) { format.setTimerPeriod(period); }

  /// The active state is determined by checking if we are still getting actual
  /// read calls;
  bool isActive() { return millis() - time_of_last_read < 100; }

  /// Returns true after start() has been called.
  bool isStarted() { return started; }

protected:
  AudioStream *p_audiostream = nullptr;
  uint32_t time_of_last_read = 0;
  bool started = true;
  RTSPOutputPCMInfo pcmInfo;
  RTSPFormatPCM format{pcmInfo};
};

/**
 * @brief Simple Facade which can turn any Stream into a
 * IAudioSource. This way we can e.g. use an I2SStream as source to stream
 * data
 * Depends on the https://github.com/pschatzmann/Micro-RTSP-Audio/ library
 * @ingroup rtsp
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class RTSPSourceStream : public IAudioSource {
public:
  /**
   * @brief Construct a new RTSPOutputSource object from an Arduino Stream
   * You need to provide the audio information by calling setAudioInfo()
   * @param stream
   * @param info
   */
  RTSPSourceStream(Stream &stream, AudioInfo info) {
    p_stream = &stream;
    rtp_info.begin(info);
    setFormat(new RTSPFormatPCM(rtp_info));
  }

  /**
   * @brief Construct a new RTSPOutputSource object from an Arduino Stream
   * You need to provide the audio information by calling setAudioInfo()
   * @param stream
   * @param format
   */
  RTSPSourceStream(Stream &stream, RTSPFormat &format) {
    p_stream = &stream;
    setFormat(&format);
  }

  /**
   * @brief Set the Audio Info. This needs to be called if we just pass a
   * Stream. The AudioStream is usually able to provide the data from it's
   * original configuration.
   *
   * @param info
   */
  virtual void setAudioInfo(AudioInfo info) {
    TRACEI();
    rtp_info.setAudioInfo(info);
  }

  // Provides the Audio Information
  virtual RTSPFormat *getFormat() override { return &format; }

  /**
   * (Reads and) Copies up to maxSamples samples into the given buffer
   * @return actual number of samples that were copied
   */
  virtual int readBytes(void *dest, int byteCount) override {
    int result = 0;
    LOGD("readDataTo: %d", byteCount);
    if (active) {
      result = p_stream->readBytes((uint8_t *)dest, byteCount);
    }
    return result;
  }
  /**
   * Start preparing data in order to provide it for the stream
   */
  virtual void start() override {
    TRACEI();
    active = true;
  };
  /**
   * Stop preparing data as the stream has ended
   */
  virtual void stop() override {
    TRACEI();
    active = false;
  };

  void setFragmentSize(int fragmentSize) {
    format.setFragmentSize(fragmentSize);
  }

  void setTimerPeriod(int period) { format.setTimerPeriod(period); }

protected:
  Stream *p_stream = nullptr;
  bool active = true;
  RTSPPCMAudioInfo rtp_info;
  RTSPFormatPCM format{rtp_info};
};

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
  const char* name() {
    return name_str;
  }
  /// Defines the  name
  void setName(const char* name){
    name_str = name;
  }

  void setFragmentSize(int fragmentSize) { RTSPFormat::setFragmentSize(fragmentSize);}
  int fragmentSize() { return RTSPFormat::fragmentSize(); }

  void setTimerPeriod(int period) { RTSPFormat::setTimerPeriod(period); }
  int timerPeriod() { return RTSPFormat::timerPeriod(); }

protected:
  AudioInfo cfg;
  const char* name_str="RTSP-Demo";;
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
             "s=%s\r\n"     // Stream Name
             "c=IN IP4 0.0.0.0\r\n" // Connection Information
             "t=0 0\r\n" // start / stop - 0 -> unbounded and permanent session
             "m=audio 0 RTP/AVP 101\r\n" // UDP sessions with format 101=opus
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
             "s=%s\r\n"     // Stream Name
             "c=IN IP4 0.0.0.0\r\n" // Connection Information
             "t=0 0\r\n" // start / stop - 0 -> unbounded and permanent session
             "m=audio 0 RTP/AVP 98\r\n" // UDP sessions with format 98=aptx
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
             "s=%s\r\n"     // Stream Name
             "c=IN IP4 0.0.0.0\r\n" // Connection Information
             "t=0 0\r\n" // start / stop - 0 -> unbounded and permanent session
             "m=audio 0 RTP/AVP 3\r\n" // UDP sessions with format 3=GSM
             , name());
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

    snprintf(buffer, len,
             "s=%s\r\n"     // Stream Name
             "c=IN IP4 0.0.0.0\r\n" // Connection Information
             "t=0 0\r\n" // start / stop - 0 -> unbounded and permanent session
             "m=audio 0 RTP/AVP %d\r\n" // UDP sessions with format 0=G711 μ-Law
             , name(), getFormat());
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
class RTSPFormatPCM : public RTSPFormatAudioTools {
public:
  /// Provides the GSM format information
  const char *format(char *buffer, int len) override {
    TRACEI();
    snprintf(buffer, len,
             "s=%s\r\n"     // Stream Name
             "c=IN IP4 0.0.0.0\r\n" // Connection Information
             "t=0 0\r\n" // start / stop - 0 -> unbounded and permanent session
             "m=audio 0 RTP/AVP %d\r\n" // UDP sessions with format 10 or 11
             "a=rtpmap:%s\r\n"
             "a=rate:%i\r\n", // provide sample rate
             name(), format(cfg.channels), payloadFormat(cfg.sample_rate, cfg.channels),
             cfg.sample_rate);
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
      snprintf(payload_fromat, 30, "%d L16/%i/%i", format(channels), sampleRate,
               channels);
      break;
    case 2:
      snprintf(payload_fromat, 30, "%d L16/%i/%i", format(channels), sampleRate,
               channels);
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
             "s=%s\r\n"     // Stream Name
             "c=IN IP4 0.0.0.0\r\n" // Connection Information
             "t=0 0\r\n" // start / stop - 0 -> unbounded and permanent session
             "m=audio 0 RTP/AVP 96\r\n" // UDP sessions with format 96=dynamic
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
 * @brief We can write PCM data to the RTSPOutput. This is encoded by the
 * indicated encoder (e.g. SBCEncoder) and can be consumed by a RTSPServer.
 * You have to make sure that the codec supports the provided audio format:
 * e.g. GSM support only 8000 samples per second with one channel.
 * Depends on the https://github.com/pschatzmann/Micro-RTSP-Audio/ library
 * @ingroup rtsp
 * @ingroup io
 * @author Phil Schatzmann
 */
class RTSPOutput : public AudioOutput {
public:
  /// Default constructor
  RTSPOutput(RTSPFormatAudioTools &format, AudioEncoder &encoder,
             int buffer_size = 1024 * 2) {
    buffer.resize(buffer_size);
    p_format = &format;
    p_encoder = &encoder;
    p_encoder->setOutput(buffer);
  }

  /// Construcor using RTSPFormatPCM and no encoder
  RTSPOutput(int buffer_size = 1024) {
    buffer.resize(buffer_size);
    p_encoder->setOutput(buffer);
  }

  AudioStreamer *streamer() { return &rtsp_streamer; }

  bool begin(AudioInfo info) {
    cfg = info;
    return begin();
  }

  bool begin() {
    TRACEI();
    if (p_input == nullptr) {
      LOGE("input is null");
      return false;
    }
    if (p_encoder == nullptr) {
      LOGE("encoder is null");
      return false;
    }
    if (p_format == nullptr) {
      LOGE("format is null");
      return false;
    }
    p_encoder->setAudioInfo(cfg);
    p_encoder->begin();
    p_format->begin(cfg);
    // setup the AudioStreamer
    rtsp_streamer.setAudioSource(&rtps_source);
    // setup the RTSPSourceFromAudioStream
    rtps_source.setInput(*p_input);
    rtps_source.setFormat(p_format);
    rtps_source.setAudioInfo(cfg);
    rtps_source.start();
    return true;
  }

  void end() { rtps_source.stop(); }

  /** We do not know exactly how much we can write because the encoded audio
   * is using less space. But providing the available buffer should cover
   * the worst case.
   */
  int availableForWrite() {
    return rtps_source.isStarted() ? buffer.available() : 0;
  }

  /**
   * We write PCM data which is encoded on the fly by the indicated encoder.
   * This data is provided by the IAudioSource
   */
  size_t write(const uint8_t *data, size_t len) override {
    TRACED();
    size_t result = p_encoder->write(data, len);
    return result;
  }

  /// @brief Returns true if the server has been started
  operator bool() { return rtps_source.isActive(); }

protected:
  RTSPFormatPCM pcm;
  CopyEncoder copy_encoder;
  RTSPSourceFromAudioStream rtps_source;
  RingBufferStream buffer{0};
  AudioStream *p_input = &buffer;
  AudioEncoder *p_encoder = &copy_encoder;
  RTSPFormatAudioTools *p_format = &pcm;
  AudioStreamer rtsp_streamer;
};

// legacy name
#if USE_OBSOLETE
using RTSPStream = RTSPOutput;
#endif

} // namespace audio_tools
