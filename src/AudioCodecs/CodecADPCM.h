#pragma once
#include "ADPCM.h"  // https://github.com/pschatzmann/adpcm
#include "AudioCodecs/AudioEncoded.h"

namespace audio_tools {

/**
 * @brief Decoder for ADPCM. Depends on https://github.com/pschatzmann/adpcm
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class ADPCMDecoder : public AudioDecoderExt {
 public:
  ADPCMDecoder(AVCodecID id, int blockSize = ADAPCM_DEFAULT_BLOCK_SIZE) {
    info.sample_rate = 44100;
    info.channels = 2;
    info.bits_per_sample = 16;
    decoder.setCodecID(id);
    decoder.setBlockSize(blockSize);
  }

  // defines the block size
  void setBlockSize(int blockSize) override {
    decoder.setBlockSize(blockSize);
  }

  void begin() override {
    TRACEI();
    current_byte = 0;
    LOGI("sample_rate: %d, channels: %d", info.sample_rate, info.channels);
    decoder.begin(info.sample_rate, info.channels);
    LOGI("frameSize: %d", (int)decoder.frameSize());
    block_size = decoder.blockSize();
    assert(block_size > 0);
    assert(decoder.frameSize() > 0);
    adpcm_block.resize(block_size);

    if (p_notify != nullptr) {
      p_notify->setAudioInfo(info);
    }
    is_started = true;
  }

  void end() override {
    TRACEI();
    decoder.end();
    adpcm_block.resize(0);
    is_started = false;
  }

  virtual void setOutput(Print &out_stream) override { p_print = &out_stream; }

  virtual size_t write(const void *input_buffer, size_t length) override {
    TRACED();

    uint8_t *input_buffer8 = (uint8_t *)input_buffer;
    LOGD("write: %d", (int)length);
    for (int j = 0; j < length; j++) {
      decode(input_buffer8[j]);
    }
    return length;
  }

  operator bool() override { return is_started; }

 protected:
  adpcm_ffmpeg::ADPCMDecoder decoder;
  Vector<uint8_t> adpcm_block;
  Print *p_print = nullptr;
  int current_byte = 0;
  int block_size = 0;
  bool is_started = false;

  bool decode(uint8_t byte) {
    adpcm_block[current_byte++] = byte;

    if (current_byte >= block_size) {
      TRACED();
      AVFrame &frame = decoder.decode(&adpcm_block[0], block_size);
      // print the result
      int16_t *data = (int16_t *)frame.data[0];
      size_t byte_count = frame.nb_samples * sizeof(int16_t) * info.channels;
      size_t written = p_print->write((uint8_t *)data, byte_count);
      if (written != byte_count) {
        LOGE("encode %d->%d", (int)byte_count, (int)written);
      }

      // restart from array begin
      current_byte = 0;
    }
    return true;
  }
};

/**
 * @brief Encoder for ADPCM - Depends on https://github.com/pschatzmann/adpcm
 * @ingroup codecs
 * @ingroup encoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class ADPCMEncoder : public AudioEncoderExt {
 public:
  ADPCMEncoder(AVCodecID id, int blockSize = ADAPCM_DEFAULT_BLOCK_SIZE) {
    info.sample_rate = 44100;
    info.channels = 2;
    info.bits_per_sample = 16;
    encoder.setCodecID(id);
    encoder.setBlockSize(blockSize);
  }

  /// Provides the block size (only available after calling begin)
  int blockSize() override {
    return encoder.blockSize();
  }


  void begin() override {
    TRACEI();
    LOGI("sample_rate: %d, channels: %d", info.sample_rate, info.channels);
    encoder.begin(info.sample_rate, info.channels);
    LOGI("frameSize: %d", (int)encoder.frameSize());
    assert(info.sample_rate != 0);
    assert(encoder.frameSize() != 0);
    total_samples = encoder.frameSize()*info.channels;
    pcm_block.resize(total_samples);
    current_sample = 0;

    is_started = true;
  }

  void end() override {
    TRACEI();
    pcm_block.resize(0);
    encoder.end();
    is_started = false;
  }

  const char *mime() override { return "audio/adpcm"; }

  void setAudioInfo(AudioInfo info) override { this->info = info; }

  void setOutput(Print &out_stream) override { p_print = &out_stream; }

  operator bool() override { return is_started; }

  size_t write(const void *in_ptr, size_t in_size) override {
    LOGD("write: %d", (int)in_size);
    int16_t *data16 = (int16_t *)in_ptr;
    for (int j = 0; j < in_size / 2; j++) {
      encode(data16[j]);
    }
    return in_size;
  }

 protected:
  AudioInfo info;
  adpcm_ffmpeg::ADPCMEncoder encoder;
  Vector<int16_t> pcm_block;
  Print *p_print = nullptr;
  bool is_started = false;
  int current_sample = 0;
  int total_samples=0;

  bool encode(int16_t sample) {
    pcm_block[current_sample++] = sample;
    if (current_sample >= total_samples) {
      TRACED();
      AVPacket &packet = encoder.encode(&pcm_block[0], total_samples);
      if (packet.size > 0) {
        size_t written = p_print->write(packet.data, packet.size);
        if (written != packet.size) {
          LOGE("encode %d->%d", (int)packet.size, (int)written);
        }
      }
      // restart from array begin
      current_sample = 0;
    }
    return true;
  }
};

}  // namespace audio_tools