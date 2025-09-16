#pragma once
#include "ADPCM.h"  // https://github.com/pschatzmann/adpcm
#include "AudioTools/AudioCodecs/AudioCodecsBase.h"

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
  ADPCMDecoder() = default;

  ADPCMDecoder(AVCodecID id, int blockSize = ADAPCM_DEFAULT_BLOCK_SIZE) {
    setBlockSize(blockSize);
    setId(id);
  }

  /// Destructor
  ~ADPCMDecoder() {
    if (p_decoder) delete p_decoder;
  }

  // (re) defines the codec id: set the block size first
  void setId(AVCodecID id) {
    codec_id = id;
    if (p_decoder != nullptr) {
      setImplementation();
    }
  }

  // defines the block size (= size of encoded frame)
  void setBlockSize(int blockSize) override {
    block_size = blockSize;
    if (p_decoder == nullptr) return;
    p_decoder->setBlockSize(blockSize);
  }

  /// Provides the block size (= size of encoded frame) (only available after
  /// calling begin)
  int blockSize() {
    if (p_decoder == nullptr) return block_size;
    return p_decoder->blockSize();
  }

  /// Provides the frame size (size of decoded frame) (only available after
  /// calling begin)
  int frameSize() {
    if (p_decoder == nullptr) return 0;
    return p_decoder->frameSize() * 2;
  }

  bool begin() override {
    TRACEI();
    if (p_decoder == nullptr) {
      setImplementation();
    }
    if (is_started) return true;
    current_byte = 0;
    LOGI("sample_rate: %d, channels: %d", info.sample_rate, info.channels);
    p_decoder->begin(info.sample_rate, info.channels);
    LOGI("frameSize: %d", (int)frameSize());
    LOGI("blockSize: %d", (int)blockSize());
    block_size = p_decoder->blockSize();
    assert(block_size > 0);
    assert(p_decoder->frameSize() > 0);
    adpcm_block.resize(block_size);

    notifyAudioChange(info);
    is_started = true;
    return true;
  }

  void end() override {
    TRACEI();
    if (p_decoder != nullptr) p_decoder->end();
    adpcm_block.resize(0);
    is_started = false;
  }

  virtual void setOutput(Print &out_stream) override { p_print = &out_stream; }

  virtual size_t write(const uint8_t *data, size_t len) override {
    TRACED();

    uint8_t *input_buffer8 = (uint8_t *)data;
    LOGD("write: %d", (int)len);
    for (int j = 0; j < len; j++) {
      decode(input_buffer8[j]);
    }
    return len;
  }

  void flush() {
    if (p_decoder != nullptr) p_decoder->flush();
  }

  operator bool() override { return is_started; }

 protected:
  adpcm_ffmpeg::ADPCMDecoder *p_decoder = nullptr;
  Vector<uint8_t> adpcm_block;
  Print *p_print = nullptr;
  int current_byte = 0;
  int block_size = ADAPCM_DEFAULT_BLOCK_SIZE;
  AVCodecID codec_id = AV_CODEC_ID_ADPCM_MS;
  bool is_started = false;

  virtual bool decode(uint8_t byte) {
    if (p_decoder == nullptr) return false;
    adpcm_block[current_byte++] = byte;

    if (current_byte >= block_size) {
      TRACED();
      adpcm_ffmpeg::AVFrame &frame =
          p_decoder->decode(&adpcm_block[0], block_size);
      // print the result
      int16_t *data = (int16_t *)frame.data[0];
      size_t byte_count = frame.nb_samples * sizeof(int16_t) * info.channels;
      size_t written = p_print->write((uint8_t *)data, byte_count);
      if (written != byte_count) {
        LOGE("decode %d -> %d -> %d", block_size, (int)byte_count,
             (int)written);
      } else {
        LOGD("decode %d -> %d -> %d", block_size, (int)byte_count,
             (int)written);
      }

      // restart from array begin
      current_byte = 0;
    }
    return true;
  }

  /// change the decoder implementation
  void setImplementation() {
    // delete the old decoder
    if (p_decoder != nullptr) {
      p_decoder->end();
      delete p_decoder;
      p_decoder = nullptr;
    }

    if (codec_id == AV_CODEC_ID_ADPCM_IMA_AMV) {
      info.sample_rate = 22050;
      info.channels = 1;
      info.bits_per_sample = 16;
    }
    p_decoder = adpcm_ffmpeg::ADPCMDecoderFactory::create(codec_id);
    if (p_decoder != nullptr) {
      p_decoder->setCodecID(codec_id);
      p_decoder->setBlockSize(block_size);
    } else {
      LOGE("Decoder not implemented");
    }
  }
};

/**
 * @brief Encoder for ADPCM - Depends on https://github.com/pschatzmann/adpcm
 * @ingroup codecs
 * @ingroup p_encoder->
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class ADPCMEncoder : public AudioEncoderExt {
 public:
  ADPCMEncoder() = default;

  ADPCMEncoder(AVCodecID id, int blockSize = ADAPCM_DEFAULT_BLOCK_SIZE) {
    setId(id);
    setBlockSize(blockSize);
  }

  /// Destructor
  ~ADPCMEncoder() {
    if (p_encoder != nullptr) delete p_encoder;
  }

  /// (re) defines the codec id
  void setId(AVCodecID id) {
    codec_id = id;
    if (p_encoder != nullptr) {
      setImplementation();
    }
  }

  /// (re) defines the block size
  void setBlockSize(int blockSize) {
    block_size = blockSize;
    if (p_encoder == nullptr) return;
    p_encoder->setBlockSize(blockSize);
  }

  /// Provides the block size (size of encoded frame) (only available after
  /// calling begin)
  int blockSize() override {
    if (p_encoder == nullptr) return 0;
    return p_encoder->blockSize();
  }

  /// Provides the frame size (size of decoded frame) (only available after
  /// calling begin)
  int frameSize() {
    if (p_encoder == nullptr) return 0;
    return p_encoder->frameSize() * 2;
  }

  bool begin() override {
    TRACEI();
    if (p_encoder == nullptr) {
      setImplementation();
    };
    if (is_started) return true;
    LOGI("sample_rate: %d, channels: %d", info.sample_rate, info.channels);
    p_encoder->begin(info.sample_rate, info.channels);
    LOGI("frameSize: %d", (int)frameSize());
    LOGI("blockSize: %d", (int)blockSize());
    assert(info.sample_rate != 0);
    assert(p_encoder->frameSize() != 0);
    total_samples = p_encoder->frameSize() * info.channels;
    pcm_block.resize(total_samples);
    current_sample = 0;

    is_started = true;
    return true;
  }

  void end() override {
    TRACEI();
    pcm_block.resize(0);
    if (p_encoder == nullptr) return;
    p_encoder->end();
    is_started = false;
  }

  const char *mime() override { return "audio/adpcm"; }

  void setOutput(Print &out_stream) override { p_print = &out_stream; }

  operator bool() override { return is_started; }

  size_t write(const uint8_t *data, size_t len) override {
    LOGD("write: %d", (int)len);
    int16_t *data16 = (int16_t *)data;
    for (int j = 0; j < len / 2; j++) {
      encode(data16[j]);
    }
    return len;
  }

  /// provides the frame duration in us (for rtsp)
  virtual uint32_t frameDurationUs() override {
    if (p_encoder == nullptr || info.sample_rate == 0) {
      return 20000; // Default 20ms if not initialized
    }
    
    // Get the number of samples per frame from the encoder
    int samplesPerFrame = p_encoder->frameSize();
    if (samplesPerFrame <= 0) {
      return 20000; // Default 20ms if invalid frame size
    }
    
    // Calculate frame duration: (samples_per_frame / sample_rate) * 1000000 us
    uint32_t durationUs = (samplesPerFrame * 1000000) / info.sample_rate;
    return durationUs;
  }

 protected:
  AVCodecID codec_id = AV_CODEC_ID_ADPCM_MS;
  adpcm_ffmpeg::ADPCMEncoder *p_encoder = nullptr;
  Vector<int16_t> pcm_block;
  Print *p_print = nullptr;
  bool is_started = false;
  int current_sample = 0;
  int total_samples = 0;
  int current_id = -1;
  int block_size = ADAPCM_DEFAULT_BLOCK_SIZE;

  virtual bool encode(int16_t sample) {
    if (p_encoder == nullptr) return false;
    pcm_block[current_sample++] = sample;
    if (current_sample >= total_samples) {
      TRACED();
      adpcm_ffmpeg::AVPacket &packet =
          p_encoder->encode(&pcm_block[0], total_samples);
      if (packet.size > 0) {
        size_t written = p_print->write(packet.data, packet.size);
        if (written != packet.size) {
          LOGE("encode %d->%d->%d", 2 * total_samples, (int)packet.size,
               (int)written);
        } else {
          LOGD("encode %d->%d->%d", 2 * total_samples, (int)packet.size,
               (int)written);
        }
      }
      // restart from array begin
      current_sample = 0;
    }
    return true;
  }

  /// change the encoder implementation
  bool setImplementation() {
    bool rc = true;
    // delete the old encoder
    if (p_encoder != nullptr) {
      p_encoder->end();
      delete p_encoder;
      p_encoder = nullptr;
    }

    if (codec_id == AV_CODEC_ID_ADPCM_IMA_AMV) {
      info.sample_rate = 22050;
      info.channels = 1;
      info.bits_per_sample = 16;
    }
    p_encoder = adpcm_ffmpeg::ADPCMEncoderFactory::create(codec_id);
    if (p_encoder != nullptr) {
      p_encoder->setCodecID(codec_id);
      p_encoder->setBlockSize(block_size);
    } else {
      LOGE("Encoder not implemented");
      rc = false;
    }
    return rc;
  }
};

}  // namespace audio_tools