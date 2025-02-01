#pragma once
#include "AudioConfig.h"
#include "AudioTools/CoreAudio/AudioEffects/SoundGenerator.h"
#include "AudioTools/CoreAudio/AudioLogger.h"
#include "AudioTools/CoreAudio/AudioStreamsConverter.h"
#include "AudioTools/CoreAudio/BaseStream.h"
#include "AudioTools/CoreAudio/Buffers.h"

namespace audio_tools {

/**
 * @brief Templated audio source for reading generated tones from a
 * SoundGeneratorT. The requested data can be smaller then the sample or frame
 * size. If more then one channel is requested all the channels are filled with
 * the a copy of the next sample.
 * @ingroup io
 * @param T audio data type (e.g int16_t)
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

template <class T>
class GeneratedSoundStreamT : public AudioStream {
 public:
  GeneratedSoundStreamT() = default;

  GeneratedSoundStreamT(SoundGeneratorT<T> &generator) {
    TRACED();
    setInput(generator);
  }

  void setInput(SoundGeneratorT<T> &generator) {
    this->generator_ptr = &generator;
  }

  AudioInfo defaultConfig() { return this->generator_ptr->defaultConfig(); }

  void setAudioInfo(AudioInfo newInfo) override {
    if (newInfo.bits_per_sample != sizeof(T) * 8) {
      LOGE("Wrong bits_per_sample: %d", newInfo.bits_per_sample);
    }
    generator_ptr->setAudioInfo(newInfo);
    AudioStream::setAudioInfo(newInfo);
  }

  /// start the processing
  bool begin() override {
    TRACED();
    if (generator_ptr == nullptr) {
      LOGE("%s", source_not_defined_error);
      return false;
    }
    generator_ptr->begin();
    notifyAudioChange(generator_ptr->audioInfo());
    return true;
  }

  /// start the processing
  bool begin(AudioInfo cfg) {
    TRACED();
    if (generator_ptr == nullptr) {
      LOGE("%s", source_not_defined_error);
      return false;
    }
    generator_ptr->begin(cfg);
    notifyAudioChange(generator_ptr->audioInfo());
    return true;
  }

  /// stop the processing
  void end() override {
    TRACED();
    ring_buffer.resize(0);
    if (generator_ptr == nullptr) return;
    generator_ptr->end();
  }

  AudioInfo audioInfo() override { return generator_ptr->audioInfo(); }

  /// This is unbounded so we just return the buffer size
  virtual int available() override {
    return isActive() ? DEFAULT_BUFFER_SIZE * 2 : 0;
  }

  /// provide the data as byte stream
  size_t readBytes(uint8_t *data, size_t len) override {
    if (!isActive()) return 0;
    LOGI("GeneratedSoundStreamT::readBytes: %u", (unsigned int)len);
    return readBytes1(data, len);
  }

  bool isActive() {
    if (generator_ptr == nullptr) return false;
    return generator_ptr->isActive();
  }

  operator bool() { return isActive(); }

  void flush() override {}

  /// Defines the frequency if this is supported by the generator
  void setFrequency(float frequeny) {
    if (generator_ptr) generator_ptr->setFrequency(frequeny);
  }

  /// Defines the amplitude if this is supported by the generator
  void setAmplitude(float amplitude) {
    if (generator_ptr) generator_ptr->setAmplitude(amplitude);
  }

 protected:
  SoundGeneratorT<T> *generator_ptr;
  RingBuffer<uint8_t> ring_buffer{0};
  const char *source_not_defined_error = "Source not defined";

  /// Provides the data as byte array with the requested number of channels
  virtual size_t readBytes1(uint8_t *data, size_t len) {
    LOGD("readBytes: %d", (int)len);
    int channels = audioInfo().channels;
    int frame_size = sizeof(T) * channels;
    int frames = len / frame_size;
    if (len >= frame_size) {
      return readBytesFrames(data, len, frames, channels);
    }
    return readBytesFromBuffer(data, len, frame_size, channels);
  }

  size_t readBytesFrames(uint8_t *buffer, size_t lengthBytes, int frames,
                         int channels) {
    T *result_buffer = (T *)buffer;
    for (int j = 0; j < frames; j++) {
      T sample = generator_ptr->readSample();
      for (int ch = 0; ch < channels; ch++) {
        *result_buffer++ = sample;
      }
    }
    return frames * sizeof(T) * channels;
  }

  size_t readBytesFromBuffer(uint8_t *buffer, size_t lengthBytes,
                             int frame_size, int channels) {
    // setup ringbuffer
    if (ring_buffer.size() == 0) ring_buffer.resize(info.channels * sizeof(T));

    // fill ringbuffer with one frame
    if (ring_buffer.isEmpty()) {
      uint8_t tmp[frame_size];
      readBytesFrames(tmp, frame_size, 1, channels);
      ring_buffer.writeArray(tmp, frame_size);
    }
    // provide result
    return ring_buffer.readArray(buffer, lengthBytes);
  }
};

/**
 * @brief Audio source for reading generated tones from an int16_t
 * SoundGenerator. The requested data can be smaller then the sample or
 * frame size. If more then one channel is requested all the channels are filled
 * with the a copy of the same sample. If the requested bits are not 16, then
 * the format is converted to the requested bit size.
 * @ingroup io
 * @param T audio data type (e.g int16_t)
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class GeneratedSoundStream : public AudioStream {
 public:
  GeneratedSoundStream() = default;

  GeneratedSoundStream(SoundGenerator &generator) {
    TRACED();
    setInput(generator);
  }

  void setInput(SoundGenerator &generator) { gss.setInput(generator); }

  AudioInfo defaultConfig() {
    AudioInfo result;
    return result;
  }

  void setAudioInfo(AudioInfo newInfo) override {
    AudioStream::setAudioInfo(newInfo);
    // format of generator must be 16 bits
    AudioInfo info16 = newInfo;
    info16.bits_per_sample = 16;
    return gss.setAudioInfo(info16);
  }

  /// start the processing
  bool begin() override {
    bool result = gss.begin();
    number_format_converter.setBuffered(false);
    number_format_converter.setStream(gss);
    number_format_converter.begin(infoFrom(), infoTo());
    return result;
  }

  /// start the processing
  bool begin(AudioInfo cfg) {
    setAudioInfo(cfg);
    return begin();
  }

  /// stop the processing
  void end() override {
    gss.end();
    number_format_converter.end();
  }

  AudioInfo audioInfo() override { return AudioStream::audioInfo(); }

  /// This is unbounded so we just return the buffer size
  virtual int available() override { return gss.available(); }

  /// provide the data as byte stream
  size_t readBytes(uint8_t *data, size_t len) override {
    return number_format_converter.readBytes(data, len);
  }

  bool isActive() { return gss.isActive(); }

  operator bool() { return gss.isActive(); }

  void flush() override { gss.flush(); }

  /// Defines the frequency if this is supported by the generator
  void setFrequency(float frequeny) {
    gss.setFrequency(frequeny);
  }

  /// Defines the amplitude if this is supported by the generator
  void setAmplitude(float amplitude) {
    gss.setAmplitude(amplitude);
  }

 protected:
  GeneratedSoundStreamT<int16_t> gss;
  NumberFormatConverterStream number_format_converter;

  AudioInfo infoFrom() { return gss.audioInfo(); }

  AudioInfo infoTo() { return audioInfo(); }
};

}  // namespace audio_tools