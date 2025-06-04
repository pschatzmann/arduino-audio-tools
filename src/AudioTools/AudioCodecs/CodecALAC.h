
#pragma once

#include "ALAC.h"  // https://github.com/pschatzmann/codec-alac
#include "AudioTools/AudioCodecs/AudioCodecsBase.h"

namespace audio_tools {

/// Magic Cookie
class ALACBinaryConfig {
 public:
  void setChannels(int inNumChannels) {
    int size = (inNumChannels > 2)
                   ? sizeof(ALACSpecificConfig) + kChannelAtomSize +
                         sizeof(ALACAudioChannelLayout)
                   : sizeof(ALACSpecificConfig);
    vector.resize(size);
  }

  uint32_t size() { return vector.size(); }
  uint8_t* data() { return vector.data(); }

 protected:
  Vector<uint8_t> vector;
};

/**
 * @brief ALAC (Apple Lossless Audio Codec) decoder. This class depends on
 * https://github.com/pschatzmann/codec-alac. This implementaion is based on
 * https://github.com/macosforge/alac
 * @note Please note that this codec usually needs a container:
 * The write() method expects a complete frame to be written!
 * The decoder also expects to get the config from the encoder, however we have
 * some fallback functionality that uses the AudioInfo and the frame size
 * defined in the constructor.
 * @ingroup codecs
 * @author Phil Schatzmann
 */
class DecoderALAC : public AudioDecoder {
 public:
  /// Default constructor: you can define your own optimized frame size
  DecoderALAC(int frameSize = kALACDefaultFrameSize) {
    // this is used when setCodecConfig() is not called with encoder info
    setFrameSize(frameSize);
    //setDefaultConfig();
  }

  // define ALACSpecificConfig
  bool setCodecConfig(ALACSpecificConfig config) {
    return setCodecConfig((uint8_t*)&config, sizeof(config));
  }

  /// write Magic Cookie (ALACSpecificConfig)
  bool setCodecConfig(ALACBinaryConfig cfg) {
    size_t result = setCodecConfig(cfg.data(), cfg.size());
    is_init = true;
    return result;
  }

  /// write Magic Cookie (ALACSpecificConfig)
  bool setCodecConfig(const uint8_t* data, size_t len) override {
    LOGI("DecoderALAC::setCodecConfig: %d", (int)len);
    // Call Init() to set up the decoder
    int32_t rc = dec.Init((void*)data, len);
    if (rc != 0) {
      LOGE("Init failed");
      return false;
    }
    LOGI("ALAC Decoder Setup - SR: %d, Channels: %d, Bits: %d, Frame Size: %d",
         (int)dec.mConfig.sampleRate, (int)dec.mConfig.numChannels,
         (int)dec.mConfig.bitDepth, (int)dec.mConfig.frameLength);
    AudioInfo tmp;
    tmp.bits_per_sample = dec.mConfig.bitDepth;
    tmp.channels = dec.mConfig.numChannels;
    tmp.sample_rate = dec.mConfig.sampleRate;
    setAudioInfo(tmp);
    is_init = true;
    return true;
  }

  /// Update the global decoder info
  void setAudioInfo(AudioInfo from) override {
    AudioDecoder::setAudioInfo(from);
    dec.mConfig.sampleRate = from.sample_rate;
    dec.mConfig.numChannels = from.channels;
    dec.mConfig.bitDepth = from.bits_per_sample;
  }


  /// we expect the write is called for a complete frame!
  size_t write(const uint8_t* encodedFrame, size_t encodedLen) override {
    LOGD("DecoderALAC::write: %d", (int)encodedLen);
    // Make sure we have a config: we can't do this in begin because the setConfig()
    // might be called after begin()
    if (!is_init) setDefaultConfig();

    // Make sure we have the output buffer set up
    if (result_buffer.size() != outputBufferSize()) {
      result_buffer.resize(outputBufferSize());
    }

    // Init bit buffer
    BitBufferInit(&bits, (uint8_t*)encodedFrame, encodedLen);

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
    LOGI("DecoderALAC::write-pcm: %d", (int)outputSize);

    // Output the result in chunks of 1k
    int open = outputSize;
    int processed = 0;
    while (open > 0) {
      int writeSize = MIN(1024, open);
      size_t written =
          p_print->write(result_buffer.data() + processed, writeSize);
      if (writeSize != written) {
        LOGE("write error: %d -> %d", (int)outputSize, (int)written);
      }
      open -= written;
      processed += written;
    }
    return encodedLen;
  }

  operator bool() { return true; }

  /// Set the default frame size: this will be overwritten if you call
  /// setCodecConfig()
  void setFrameSize(int frames) { dec.mConfig.frameLength = frames; }

  /// Provides the actual frame size
  int frameSize() { return dec.mConfig.frameLength; }

 protected:
  ALACDecoder dec;
  Vector<uint8_t> result_buffer;
  bool is_init = false;
  struct BitBuffer bits;

  void setDefaultConfig() {
    // LOGW("Setting up default ALAC config")
    AudioInfo info = audioInfo();
    ALACSpecificConfig tmp;
    // Essential parameters for ALAC compression
    tmp.frameLength = frameSize();
    tmp.compatibleVersion = 0;
    tmp.bitDepth = info.bits_per_sample;
    tmp.pb = 40;  // Rice parameter limit
    tmp.mb = 10;  // Maximum prefix length for Rice coding
    tmp.kb = 14;  // History multiplier
    tmp.numChannels = info.channels;
    tmp.maxRun = 255;  // Maximum run length supported
    tmp.avgBitRate = 0;

    tmp.sampleRate = info.sample_rate;

    // Calculate max frame bytes - must account for:
    // 1. Uncompressed frame size
    // 2. ALAC frame headers
    // 3. Potential compression inefficiency
    uint32_t bytesPerSample = info.bits_per_sample / 8;
    uint32_t uncompressedFrameSize =
        frameSize() * info.channels * bytesPerSample;

    // Add safety margins:
    // - ALAC header (~50 bytes)
    // - Worst case compression overhead (50%)
    // - Alignment padding (64 bytes)
    tmp.maxFrameBytes =
        uncompressedFrameSize + (uncompressedFrameSize / 2) + 64 + 50;

    convertToNetworkFormat(tmp);
    setCodecConfig(tmp);
  }

  /// Calculate the output buffer size based on the current configuration
  int outputBufferSize() {
    return dec.mConfig.frameLength * dec.mConfig.numChannels *
           dec.mConfig.bitDepth / 8;
  }

  /// Convert to big endian so that we can use it in Init()
  void convertToNetworkFormat(ALACSpecificConfig& config) {
    config.frameLength = Swap32NtoB(config.frameLength);
    config.maxRun = Swap16NtoB((uint16_t)config.maxRun);
    config.maxFrameBytes = Swap32NtoB(config.maxFrameBytes);
    config.avgBitRate = Swap32NtoB(config.avgBitRate);
    config.sampleRate = Swap32NtoB(config.sampleRate);
  }
};

/**
 * @brief ALAC (Apple Lossless Audio Codec) encoder. This class is responsible
 * for encoding audio data into ALAC format.
 * The implementaion is based on https://github.com/macosforge/alac
 * @ingroup codecs
 * @author Phil Schatzmann
 */
class EncoderALAC : public AudioEncoder {
 public:
  /// Default constructor: you can define your own optimized frame size
  EncoderALAC(int frameSize = kALACDefaultFrameSize) {
    setFrameSize(frameSize);
  }
  void setOutput(Print& out_stream) override { p_print = &out_stream; };

  bool begin() override {
    if (p_print == nullptr) {
      LOGE("No output stream set");
      return false;
    }
    // define input format
    input_format = getInputFormat();
    out_format = getOutputFormat();

    // Setup Encoder
    enc.SetFrameSize(frame_size);
    int rc = enc.InitializeEncoder(out_format);

    // Calculate exact buffer sizes based on frame settings
    uint32_t bytesPerSample = info.bits_per_sample / 8;
    uint32_t inputBufferSize = frame_size * info.channels * bytesPerSample;
    // Calculate output buffer size
    uint32_t outputBufferSize = inputBufferSize * 2;  // Ensure enough space

    LOGI(
        "ALAC Encoder: frame_size=%d, inputBuf=%d, outputBuf=%d, channels=%d, "
        "bits=%d",
        frame_size, inputBufferSize, outputBufferSize, info.channels,
        info.bits_per_sample);

    in_buffer.resize(inputBufferSize);
    out_buffer.resize(outputBufferSize);
    is_started = rc == 0;
    return is_started;
  }

  void end() override {
    enc.Finish();
    is_started = false;
  }

  /// Encode the audio samples into ALAC format
  size_t write(const uint8_t* data, size_t len) override {
    if (!is_started) return 0;
    LOGD("EncoderALAC::write: %d", (int)len);
    for (int j = 0; j < len; j++) {
      in_buffer.write(data[j]);
      if (in_buffer.isFull()) {
        // provide available encoded data length
        int32_t ioNumBytes = in_buffer.size();
        int rc = enc.Encode(input_format, out_format, (uint8_t*)in_buffer.data(),
                       out_buffer.data(), &ioNumBytes);
        // Output encoded data
        size_t written = p_print->write(out_buffer.data(), ioNumBytes);
        if (ioNumBytes != written) {
          LOGE("write error: %d -> %d", (int)ioNumBytes, (int)written);
        }
        in_buffer.reset();
      }
    }
    return len;
  }

  /// Provide the configuration of the encoder
  ALACSpecificConfig config() {
    enc.GetConfig(cfg);
    return cfg;
  }

  /// Provide the magic coookie for the decoder
  ALACBinaryConfig& binaryConfig() {
    bin.setChannels(info.channels);
    uint32_t size = bin.size();
    enc.GetMagicCookie(bin.data(), &size);
    return bin;
  }

  /// Check if the encoder is ready to encode
  operator bool() { return is_started && p_print != nullptr; }

  /// Mime type: returns audio/alac
  const char* mime() override { return "audio/alac"; }

  /// Defines if the encoder should use fast mode
  void setFastMode(bool fast) {
    enc.SetFastMode(fast);
  }

  /// Defines the frame size for the decoder: default is 4096 frames 
  void setFrameSize(int frames) {
    if (is_started) {
      LOGE("Can't change frame size on started encoder")
      return;
    }
    frame_size = frames;
  }

  /// Determins the actually defined number of frames
  int frameSize() { return frame_size; }

 protected:
  int frame_size = kALACDefaultFrameSize;
  ALACEncoder enc;
  SingleBuffer<uint8_t> in_buffer;
  Vector<uint8_t> out_buffer;
  AudioFormatDescription input_format;
  AudioFormatDescription out_format;
  ALACSpecificConfig cfg;
  ALACBinaryConfig bin;
  Print* p_print = nullptr;
  bool is_started = false;

  AudioFormatDescription getInputFormat() {
    AudioFormatDescription result;
    memset(&result, 0, sizeof(AudioFormatDescription));
    result.mSampleRate = info.sample_rate;
    result.mFormatID = kALACFormatLinearPCM;
    result.mFormatFlags =
        kALACFormatFlagIsSignedInteger |
        kALACFormatFlagIsPacked;  // Native endian, signed integer
    result.mBytesPerPacket = info.channels * (info.bits_per_sample / 8);
    result.mFramesPerPacket = 1;
    result.mBytesPerFrame = info.channels * (info.bits_per_sample / 8);
    result.mChannelsPerFrame = info.channels;
    result.mBitsPerChannel = info.bits_per_sample;

    return result;
  }

  AudioFormatDescription getOutputFormat() {
    AudioFormatDescription result;
    memset(&result, 0, sizeof(AudioFormatDescription));
    result.mSampleRate = info.sample_rate;
    result.mFormatID = kALACCodecFormat;
    result.mFormatFlags = getOutputFormatFlags(info.bits_per_sample);  // or 0 ?
    result.mBytesPerPacket = 0;            // Variable for compressed format
    result.mFramesPerPacket = frame_size;  // Common ALAC frame size
    result.mBytesPerFrame = 0;             // Variable for compressed format
    result.mChannelsPerFrame = info.channels;
    result.mBitsPerChannel = info.bits_per_sample;
    return result;
  }

  // Adapted from CoreAudioTypes.h
  enum {
    kFormatFlag_16BitSourceData = 1,
    kFormatFlag_20BitSourceData = 2,
    kFormatFlag_24BitSourceData = 3,
    kFormatFlag_32BitSourceData = 4
  };

  uint32_t getOutputFormatFlags(uint32_t bits) {
    switch (bits) {
      case 16:
        return kFormatFlag_16BitSourceData;
      case 20:
        return kFormatFlag_20BitSourceData;
      case 24:
        return kFormatFlag_24BitSourceData;
      case 32:
        return kFormatFlag_32BitSourceData;
        break;
      default:
        LOGE("Unsupported bit depth: %d", bits);
        return 0;
    }
  }
};

}  // namespace audio_tools