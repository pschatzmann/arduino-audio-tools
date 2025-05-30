
#pragma once

#include "ALAC.h"  // https://github.com/pschatzmann/codec-alac
#include "AudioTools/AudioCodecs/AudioCodecsBase.h"

namespace audio_tools {

/**
 * @brief ALAC (Apple Lossless Audio Codec) decoder. Please note that this codec
 * usually needs a container (e.g. M4A) to provide the relevant codec
 * information. This class depends on https://github.com/pschatzmann/codec-alac
 * @ingroup codecs
 * @author Phil Schatzmann
 */
class DecoderALAC : public AudioDecoder {
 public:
  /// write Magic Cookie (ALACSpecificConfig)
  size_t writeCodecConfig(const uint8_t* data, size_t len) override {
    int32_t rc = dec.Init((void*)data, len);
    if (rc != 0) {
      LOGE("Init failed");
      return 0;
    }
    AudioInfo info;
    info.bits_per_sample = dec.mConfig.bitDepth;
    info.channels = dec.mConfig.numChannels;
    info.sample_rate = dec.mConfig.sampleRate;
    setAudioInfo(info);
    is_init = true;
    return len;
  }

  // define ALACSpecificConfig
  size_t writeCodecConfig(ALACSpecificConfig config) {
    return writeCodecConfig((uint8_t*)&config, sizeof(config));
  }

  bool begin(ALACSpecificConfig config) {
    size_t written = writeCodecConfig(config);
    return written == sizeof(ALACSpecificConfig);
  }

  /// we expect the write is called for a complete frame!
  size_t write(const uint8_t* encodedFrame, size_t len) override {
    LOGI("DecoderALAC::write: %d", (int)len);
    if (!is_init) {
      ALACSpecificConfig config = {};
      int frame_size = info.bits_per_sample / 8 * info.channels;
      AudioInfo info = audioInfo();
      config.frameLength = len / frame_size; // frames per packet
      config.compatibleVersion = 0;
      config.pb = 40;
      config.mb = 10;
      config.kb = 14;
      config.maxRun = 255;
      config.maxFrameBytes = len;
      config.avgBitRate = 0; // not known
      config.bitDepth = info.bits_per_sample;
      config.numChannels = info.channels;
      config.sampleRate = info.sample_rate;
      writeCodecConfig((uint8_t*)&config, sizeof(config));
      is_init = true;
    }
    // Make sure we have the output buffer set up
    if (result_buffer.size() != outputBufferSize()) {
      result_buffer.resize(outputBufferSize());
    }

    // Init bit buffer
    struct BitBuffer bits;
    BitBufferInit(&bits, (uint8_t*)encodedFrame, dec.mConfig.frameLength);

    // Decode
    uint32_t outNumSamples = 0;
    int32_t status =
        dec.Decode(&bits, result_buffer.data(), dec.mConfig.frameLength,
                   dec.mConfig.numChannels, &outNumSamples);

    if (status != 0) {
      LOGE("Decode failed with error: %d", status);
      return 0;
    }

    // Process result
    size_t outputSize =
        outNumSamples * dec.mConfig.numChannels * dec.mConfig.bitDepth / 8;
    size_t written = p_print->write(result_buffer.data(), outputSize);
    if (outputSize != written) {
      LOGE("write error: %d -> %d", outputSize, written);
    }
    return len;
  }

  operator bool() { return true; }

 protected:
  ALACDecoder dec;
  Vector<uint8_t> result_buffer;
  bool is_init = false;

  int outputBufferSize() {
    return dec.mConfig.frameLength * dec.mConfig.numChannels *
           dec.mConfig.bitDepth / 8;
  }
};

/**
 * @brief ALAC (Apple Lossless Audio Codec) encoder. This class is responsible
 * for encoding audio data into ALAC format.
 * @ingroup codecs
 * @author Phil Schatzmann
 */
class EncoderALAC : public AudioEncoder {
 public:
  EncoderALAC(int bytesPerPacket = 1024) {
    setDefaultBytesPerPacket(bytesPerPacket);
  }
  void setOutput(Print& out_stream) override { p_print = &out_stream; };

  bool begin() override {
    if (p_print == nullptr) {
      LOGE("No output stream set");
      return false;
    }
    // define input format
    input_format.mSampleRate = info.sample_rate;
    input_format.mFormatID = kALACFormatLinearPCM;
    input_format.mFormatFlags = kALACFormatFlagIsSignedInteger;
    input_format.mBytesPerPacket = default_bytes_per_packet;
    input_format.mBytesPerFrame = info.channels * info.bits_per_sample / 8;
    input_format.mFramesPerPacket =
        default_bytes_per_packet / input_format.mBytesPerFrame;
    input_format.mChannelsPerFrame = info.channels;
    input_format.mBitsPerChannel = info.bits_per_sample;

    // define output format
    out_format = input_format;
    out_format.mFormatID = kALACFormatAppleLossless;

    int rc = enc.InitializeEncoder(out_format);

    in_buffer.resize(default_bytes_per_packet);
    out_buffer.resize(default_bytes_per_packet);
    is_started = rc == 0;
    return is_started;
  }

  void end() override {
    enc.Finish();
    is_started = false;
  }

  /// Encode the audio samples into ALAC format
  size_t write(const uint8_t* data, size_t len) override {
    LOGI("EncoderALAC::write: %d", (int)len);
    for (int j = 0; j < len; j++) {
      in_buffer.write(data[j]);
      if (in_buffer.isFull()) {
        int32_t ioNumBytes = in_buffer.available();
        int rc = enc.Encode(input_format, out_format, (uint8_t*)in_buffer.data(),
                            out_buffer.data(), &ioNumBytes);
        size_t written = p_print->write(out_buffer.data(), ioNumBytes);
        if (ioNumBytes != written) {
          LOGE("write error: %d -> %d", ioNumBytes, written);
        }
        in_buffer.reset();
      }
    }
    return len;
  }

  ALACSpecificConfig config() {
    enc.GetConfig(cfg);
    return cfg;
  }

  operator bool() { return is_started && p_print != nullptr; }

  const char* mime() override { return "audio/alac"; }

  void setDefaultBytesPerPacket(int bytes) { default_bytes_per_packet = bytes; }

 protected:
  int default_bytes_per_packet = 1024;
  ALACEncoder enc;
  SingleBuffer<uint8_t> in_buffer;
  Vector<uint8_t> out_buffer;
  AudioFormatDescription input_format;
  AudioFormatDescription out_format;
  ALACSpecificConfig cfg;
  Print* p_print = nullptr;
  bool is_started = false;
};

}  // namespace audio_tools