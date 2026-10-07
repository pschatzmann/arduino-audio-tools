#pragma once

#include "AudioTools/AudioCodecs/AudioCodecsBase.h"
#include "faad2.h"
#include "neaacdec.h"

// faad2's neaacdec.h defines short, common object/header-type macros (RAW,
// ADIF, ADTS, LATM, MAIN, LC, SSR, LTP, HE_AAC, ER_LC, ER_LTP, LD,
// DRM_ER_LC, AudioSpecificConfig) that are not used outside of this header
// but leak globally and clash with identifiers (e.g. VideoFormat::RAW in
// ContainerAVI.h) in other audio-tools headers.
#undef MAIN
#undef LC
#undef SSR
#undef LTP
#undef HE_AAC
#undef ER_LC
#undef ER_LTP
#undef LD
#undef DRM_ER_LC
#undef RAW
#undef ADIF
#undef ADTS
#undef LATM
#undef AudioSpecificConfig

#ifndef FAAD2_INPUT_BUFFER_SIZE
#define FAAD2_INPUT_BUFFER_SIZE 1024 * 2
#endif

// to prevent Decoding error: Maximum number of bitstream elements exceeded
#ifndef FAAD2_UNDERFLOW_LIMIT
#define FAAD2_UNDERFLOW_LIMIT 500
#endif

namespace audio_tools {

/**
 * @brief AAC Decoder (LC, HE-AAC, MAIN, LTP) using faad2:
 * https://github.com/pschatzmann/codec-faad2
 * Supports ADTS, ADIF and raw AAC. This needs a big stack and you need to make
 * sure that memory is allocated on PSRAM.
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AACDecoderFAAD2 : public AudioDecoder {
 public:
  AACDecoderFAAD2() {
    info.channels = 2;
    info.sample_rate = 44100;
    info.bits_per_sample = 16;
  };

  ~AACDecoderFAAD2() { end(); }

  const char *mime() override { return "audio/aac"; }

  /// Starts the processing
  bool begin() override {
    TRACED();
    end();

    // Open the library
    hAac = NeAACDecOpen();
    if (hAac == nullptr) {
      LOGE("NeAACDecOpen");
      return false;
    }

    // Get the current config and adjust it
    conf = NeAACDecGetCurrentConfiguration(hAac);
    conf->outputFormat = FAAD_FMT_16BIT;
    conf->defSampleRate = info.sample_rate;
    conf->downMatrix = true;  // 5.1 channel downmatrixed to 2 channel
    conf->useOldADTSFormat = false;
    conf->dontUpSampleImplicitSBR = false;

    // Set the new configuration
    if (!NeAACDecSetConfiguration(hAac, conf)) {
      LOGE("NeAACDecSetConfiguration");
      return false;
    }

    // setup input buffer
    if (input_buffer.size() != buffer_size_input) {
      input_buffer.resize(buffer_size_input);
    }
    input_buffer.reset();
    is_init = false;
    return true;
  }

  /// Releases the reserved memory
  void end() override {
    TRACED();
    if (hAac != nullptr) {
      flush();
      NeAACDecClose(hAac);
      hAac = nullptr;
    }
  }

  /// Write AAC data to decoder
  size_t write(const uint8_t *data, size_t len) override {
    if (hAac == nullptr) return 0;
    size_t result = 0;
    while (result < len) {
      // Write supplied data to input buffer
      size_t written =
          input_buffer.writeArray((uint8_t *)data + result, len - result);
      result += written;
      // Decode from input buffer
      decode(underflow_limit);
      // decoder is stuck: avoid an endless loop
      if (written == 0 && input_buffer.availableForWrite() == 0) {
        LOGE("Input buffer full: discarding data");
        input_buffer.reset();
      }
    }
    return result;
  }

  /// Decodes all remaining data in the input buffer
  void flush() { decode(0); }

  /// Defines the input buffer size
  void setInputBufferSize(int len) { buffer_size_input = len; }

  /// Defines the min number of bytes that are submitted to the decoder
  void setUnderflowLimit(int len) { underflow_limit = len; }

  /// Provides access to the faad2 decoder handle
  NeAACDecHandle driver() { return hAac; }

  /// checks if the class is active
  operator bool() override { return hAac != nullptr; }

 protected:
  int buffer_size_input = FAAD2_INPUT_BUFFER_SIZE;
  int underflow_limit = FAAD2_UNDERFLOW_LIMIT;
  NeAACDecHandle hAac = nullptr;
  NeAACDecConfigurationPtr conf = nullptr;
  SingleBuffer<uint8_t> input_buffer{0};
  bool is_init = false;

  bool init(uint8_t *data, size_t len) {
    TRACEI();
    unsigned long samplerate = info.sample_rate;
    unsigned char channels = info.channels;

    long consumed = NeAACDecInit(hAac, data, len, &samplerate, &channels);
    if (consumed < 0) {
      LOGE("NeAACDecInit");
      return false;
    }
    // skip any data before the first frame
    if (consumed > 0) {
      input_buffer.clearArray(consumed);
    }
    info.sample_rate = samplerate;
    info.channels = channels;
    is_init = true;
    return true;
  }

  void decode(int minBufferSize) {
    TRACED();
    if (hAac == nullptr) return;
    NeAACDecFrameInfo hInfo;

    // decode until we do not consume any bytes
    while (input_buffer.available() > minBufferSize) {
      if (!is_init && !init(input_buffer.data(), input_buffer.available())) {
        // drop the data so that we can retry with the next bytes
        input_buffer.clearArray(1);
        continue;
      }

      int eff_len = input_buffer.available();
      if (eff_len <= 0) break;
      uint8_t *sample_buffer = (uint8_t *)NeAACDecDecode(
          hAac, &hInfo, input_buffer.data(), eff_len);

      LOGD("bytesconsumed: %d of %d", (int)hInfo.bytesconsumed, eff_len);
      if (hInfo.error != 0) {
        LOGW("Decoding error: %s", NeAACDecGetErrorMessage(hInfo.error));
      }

      if (hInfo.bytesconsumed == 0) {
        break;
      }

      // removed consumed data
      input_buffer.clearArray(hInfo.bytesconsumed);

      if (hInfo.error != 0 || sample_buffer == nullptr || hInfo.samples == 0) {
        continue;
      }

      // check for changes in config
      AudioInfo tmp{(sample_rate_t)hInfo.samplerate, hInfo.channels, 16};
      if (tmp != info) {
        setAudioInfo(tmp);
      }

      size_t bytes = hInfo.samples * sizeof(int16_t);
      size_t len = p_print->write(sample_buffer, bytes);
      if (len != bytes) {
        TRACEE();
      }
    }
  }
};

}  // namespace audio_tools
