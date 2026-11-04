#pragma once

#include <string.h>

#include <algorithm>

#include "ATRACDecoder.h"  // https://github.com/pschatzmann/codec-atrac
#include "AudioTools/AudioCodecs/AudioCodecsBase.h"

namespace audio_tools {

/**
 * @brief ATRAC decoder (ATRAC1, ATRAC3, ATRAC3+ and ATRAC9) which converts the
 * raw ATRAC frames into 16 bit PCM data. This class depends on
 * https://github.com/pschatzmann/codec-atrac which is based on the ATRAC
 * decoders of FFmpeg.
 *
 * The decoder needs to know the stream parameters which are usually found in
 * the header of a container: register one decoder per type with
 * WAVDecoder::addDecoder() to process WAV files (.at3, .at9), use
 * ContainerATRACOMA or ContainerATRACAEA (see ContainerATRAC.h) for OMA and
 * AEA files, or call
 * begin(type, sampleRate, channels, blockAlign, extradata, extradataSize)
 * yourself if you get the raw frames from somewhere else. The write() method
 * can be called with any amount of data: the frames of blockAlign bytes are
 * assembled internally.
 *
 * | Type       | blockAlign                    | extradata                    |
 * |------------|-------------------------------|------------------------------|
 * | ATRAC1     | 212 * channels                | -                            |
 * | ATRAC3     | frame size                    | 10, 12 or 14 bytes           |
 * | ATRAC3PLUS | frame size                    | -                            |
 * | ATRAC9     | frame size (or 0: derived)    | 12 bytes (rate and channels) |
 *
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 */
class DecoderATRAC : public AudioDecoderExt {
 public:
  DecoderATRAC(ATRACType type = ATRAC3) { this->type = type; }
  ~DecoderATRAC() { end(); }

  const char *mime() override { return "audio/atrac"; }

  /// Defines the ATRAC variant (must be called before begin())
  void setType(ATRACType type) {
    if (is_open) {
      LOGE("setType() must be called before begin()");
      return;
    }
    this->type = type;
  }

  ATRACType getType() { return type; }

  /// Defines the size of one ATRAC frame (block align) in bytes
  void setBlockSize(int blockSize) override { block_align = blockSize; }

  /// Defines the codec specific header (e.g. the WAV fmt extension)
  void setExtraData(const uint8_t *data, size_t len) override {
    setCodecConfig(data, len);
  }

  /// Provides the WAV format tag of the selected type, so that the decoder
  /// can be registered with WAVDecoder::addDecoder(): ATRAC3, ATRAC3+ and
  /// ATRAC9 are supported in WAV files
  AudioFormat wavFormat() override {
    switch (type) {
      case ATRAC3:
        return AudioFormat::ATRAC3;
      case ATRAC3PLUS:
        return AudioFormat::ATRAC3_PLUS;
      case ATRAC9:
        return AudioFormat::ATRAC9;
      default:
        return AudioFormat::UNKNOWN;
    }
  }

  /// Defines the codec specific header (e.g. the WAV fmt extension)
  bool setCodecConfig(const uint8_t *data, size_t len) override {
    extradata.resize(len);
    if (len > 0) memcpy(extradata.data(), data, len);
    return true;
  }

  /// Starts the decoder with the values defined with setAudioInfo(),
  /// setBlockSize() and setCodecConfig()
  bool begin() override {
    return begin(type, info.sample_rate, info.channels, block_align,
                 extradata.data(), extradata.size());
  }

  /// Starts the decoder with the parameters found in the header of the stream
  bool begin(ATRACType type, int sampleRate, int channels, int blockAlign,
             const uint8_t *extra = nullptr, size_t extraSize = 0) {
    TRACED();
    end();
    this->type = type;
    ATRACConfig cfg;
    cfg.sampleRate = sampleRate;
    cfg.channels = channels;
    cfg.blockAlign = blockAlign;
    cfg.extradata = extra;
    cfg.extradataSize = extraSize;
    p_decoder = new ATRACDecoder(type);
    if (p_decoder == nullptr || !p_decoder->begin(cfg)) {
      LOGE("ATRAC decoder could not be opened (type: %d)", (int)type);
      end();
      return false;
    }

    // ATRAC9 provides the block align via the extradata
    frame_size = blockAlign;
    if (frame_size <= 0 && type == ATRAC9) {
      frame_size = atrac9FrameSize(extra, extraSize);
    }
    if (frame_size <= 0) {
      LOGE("ATRAC block size is not defined");
      end();
      return false;
    }
    frame.resize(frame_size);
    frame_pos = 0;

    int ch = p_decoder->channels();
    pcm.resize(MAX_SAMPLES_PER_FRAME * ch);
    block_align = frame_size;

    AudioInfo tmp(p_decoder->sampleRate(), ch, 16);
    setAudioInfo(tmp);
    LOGI("ATRAC %s: %d Hz, %d channels, %d bytes/frame", p_decoder->codecName(),
         (int)tmp.sample_rate, (int)tmp.channels, frame_size);
    is_open = true;
    return true;
  }

  void end() override {
    if (p_decoder != nullptr) {
      delete p_decoder;
      p_decoder = nullptr;
    }
    is_open = false;
    frame_pos = 0;
  }

  /// Drops partial data and resets the decoder state, e.g. after a seek
  void flush() {
    frame_pos = 0;
    if (p_decoder != nullptr) p_decoder->flush();
  }

  /// Decodes the data: any amount of data can be provided
  size_t write(const uint8_t *data, size_t len) override {
    if (!is_open) return 0;
    size_t pos = 0;
    while (pos < len) {
      size_t n = std::min(len - pos, (size_t)frame_size - frame_pos);
      memcpy(frame.data() + frame_pos, data + pos, n);
      frame_pos += n;
      pos += n;
      if (frame_pos == (size_t)frame_size) {
        decodeFrame();
        frame_pos = 0;
      }
    }
    return len;
  }

  operator bool() override { return is_open; }

  /// Size of one ATRAC frame in bytes
  int frameSize() { return frame_size; }

 protected:
  /// ATRAC3+ has the largest frames: 2048 samples per channel
  static const int MAX_SAMPLES_PER_FRAME = 2048;
  ATRACDecoder *p_decoder = nullptr;
  ATRACType type = ATRAC3;
  Vector<uint8_t> extradata;
  Vector<uint8_t> frame;
  Vector<int16_t> pcm;
  size_t frame_pos = 0;
  int frame_size = 0;
  int block_align = 0;
  bool is_open = false;

  void decodeFrame() {
    int samples =
        p_decoder->decode(frame.data(), frame.size(), pcm.data(), pcm.size());
    if (samples <= 0) {
      // the first frame(s) of some streams do not produce any output
      LOGD("ATRAC decode: no output (error: %d)", p_decoder->lastError());
      return;
    }
    if (p_print == nullptr) return;
    size_t open = samples * p_decoder->channels() * sizeof(int16_t);
    uint8_t *ptr = (uint8_t *)pcm.data();
    while (open > 0) {
      size_t written = p_print->write(ptr, open);
      if (written == 0) {
        LOGE("write error: %d bytes lost", (int)open);
        break;
      }
      ptr += written;
      open -= written;
    }
  }

  /// ATRAC9 config header: the packet size is the frame size << superframe
  /// index (see ATRACDecoder)
  static int atrac9FrameSize(const uint8_t *extra, size_t size) {
    if (extra == nullptr || size != 12) return 0;
    const uint8_t *e = extra + 4;
    int frame_size = ((e[2] << 3) | (e[3] >> 5)) + 1;
    int superframe = (e[3] >> 3) & 0x3;
    return frame_size << superframe;
  }
};

}  // namespace audio_tools
