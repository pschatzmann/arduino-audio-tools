#pragma once

#include "AudioStreamer.h"
#include "AudioTools/AudioPlayer.h"
#include "AudioTools/AudioStreams.h"
#include "IAudioSource.h"
#include "RTSPServer.h"

namespace audio_tools {

/**
 * @brief PCMInfo subclass which provides the audio information from the related
 * AudioStream
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class RTPStreamPCMInfo : public PCMInfo {
 public:
  RTPStreamPCMInfo() = default;
  virtual void begin(AudioStream& stream) { p_stream = &stream; }
  int getSampleRate() override { return p_stream->audioInfo().sample_rate; }
  int getChannels() override { return p_stream->audioInfo().channels; }
  int getSampleSizeBytes() override {
    return p_stream->audioInfo().bits_per_sample / 8;
  };
  virtual void setAudioInfo(AudioBaseInfo ai) { p_stream->setAudioInfo(ai); }

 protected:
  AudioStream* p_stream = nullptr;
};

/**
 * @brief PCMInfo subclass which provides the audio information from the
 * AudioBaseInfo parameter
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class RTPPCMAudioInfo : public PCMInfo {
 public:
  RTPPCMAudioInfo() = default;
  virtual void begin(AudioBaseInfo info) { this->info = info; }
  int getSampleRate() override { return info.sample_rate; }
  int getChannels() override { return info.channels; }
  int getSampleSizeBytes() override { return info.bits_per_sample / 8; };
  virtual void setAudioInfo(AudioBaseInfo ai) { info = ai; }

 protected:
  AudioBaseInfo info;
};


/**
 * @brief Simple Facade which can turn  AudioStream into a
 * IAudioSource. This way we can e.g. use an I2SStream as source to stream data
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class RTSPSourceAudioStream : public IAudioSource {
 public:
  /**
   * @brief Construct a new RTSPStreamSource object from an AudioStream
   *
   * @param stream
   */
  RTSPSourceAudioStream(AudioStream& stream) {
    p_audiostream = &stream;
    pcmInfo.begin(stream);
    setFormat(new RTSPFormatPCM(pcmInfo));
  }

  RTSPSourceAudioStream(AudioStream& stream, RTSPFormat& format) {
    p_audiostream = &stream;
    setFormat(&format);
  }

  /**
   * @brief Set the Audio Info. This needs to be called if we just pass a
   * Stream. The AudioStream is usually able to provide the data from it's
   * original configuration.
   *
   * @param info
   */
  virtual void setAudioInfo(AudioBaseInfo info) {
    TRACEI();
    p_audiostream->setAudioInfo(info);
  }

  /**
   * (Reads and) Copies up to maxSamples samples into the given buffer
   * @param dest Buffer into which the samples are to be copied
   * @param maxSamples maximum number of samples to be copied
   * @return actual number of samples that were copied
   */
  virtual int readBytes(void* dest, int byteCount) override {
    int result = 0;
    LOGD("readDataTo: %d", byteCount);
    if (active) {
      result = p_audiostream->readBytes((uint8_t*)dest, byteCount);
    }
    return result;
  }
  // Provides the Audio Information 
  virtual RTSPFormat* getFormat() override { return &format; }

  /**
   * Start preparing data in order to provide it for the stream
   */
  virtual void start() override {
    TRACEI();

    p_audiostream->begin();
    active = true;
  };
  /**
   * Stop preparing data as the stream has ended
   */
  virtual void stop() override {
    TRACEI();
    active = false;
    p_audiostream->end();
  };

  void setFragmentSize(int fragmentSize) { format.setFragmentSize(fragmentSize); }
  void setTimerPeriod(int period) { format.setTimerPeriod(period); }

 protected:
  AudioStream* p_audiostream = nullptr;
  bool active = true;
  RTPStreamPCMInfo pcmInfo;
  RTSPFormatPCM format{pcmInfo};
};

/**
 * @brief Simple Facade which can turn any Stream into a
 * IAudioSource. This way we can e.g. use an I2SStream as source to stream
 * data
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class RTSPSourceStream : public IAudioSource {
 public:
  /**
   * @brief Construct a new RTSPStreamSource object from an Arduino Stream
   * You need to provide the audio information by calling setAudioInfo()
   * @param stream
   * @param info
   */
  RTSPSourceStream(Stream& stream, AudioBaseInfo info) {
    p_stream = &stream;
    rtp_info.begin(info);
    setFormat(new RTSPFormatPCM(rtp_info));
  }

  /**
   * @brief Construct a new RTSPStreamSource object from an Arduino Stream
   * You need to provide the audio information by calling setAudioInfo()
   * @param stream
   * @param format
   */
  RTSPSourceStream(Stream& stream, RTSPFormat& format) {
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
  virtual void setAudioInfo(AudioBaseInfo info) {
    TRACEI();
    rtp_info.setAudioInfo(info);
  }

  // Provides the Audio Information 
  virtual RTSPFormat* getFormat() override { return &format; }

  /**
   * (Reads and) Copies up to maxSamples samples into the given buffer
   * @return actual number of samples that were copied
   */
  virtual int readBytes(void* dest, int byteCount) override {
    int result = 0;
    LOGD("readDataTo: %d", byteCount);
    if (active) {
      result = p_stream->readBytes((uint8_t*)dest, byteCount);
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

  void setFragmentSize(int fragmentSize) { format.setFragmentSize(fragmentSize); }
  void setTimerPeriod(int period) { format.setTimerPeriod(period); }

 protected:
  Stream* p_stream = nullptr;
  bool active = true;
  RTPPCMAudioInfo rtp_info;
  RTSPFormatPCM format{rtp_info};
};

}  // namespace audio_tools
