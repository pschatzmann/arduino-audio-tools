#pragma once

// #include "Stream.h"
#include "AudioCodecs/AudioEncoded.h"
#include "faad.h"

#ifndef FAAD_INPUT_BUFFER_SIZE
#define FAAD_INPUT_BUFFER_SIZE 1024*2
#endif

// to prevent Decoding error: Maximum number of bitstream elements exceeded
#ifndef FAAD_UNDERFLOW_LIMIT
#define FAAD_UNDERFLOW_LIMIT 500
#endif


namespace audio_tools {

/**
 * @brief AAC Decoder using faad: https://github.com/pschatzmann/arduino-libfaad
 * This needs a stack of around 60000 and you need to make sure that memory is allocated on PSRAM.
 * See https://www.pschatzmann.ch/home/2023/09/12/arduino-audio-tools-faat-aac-decoder/
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AACDecoderFAAD : public AudioDecoder {
 public:
  AACDecoderFAAD() {
    info.channels = 2;
    info.sample_rate = 44100;
    info.bits_per_sample = 16;
  };

  ~AACDecoderFAAD() { end(); }

  /// Starts the processing
  void begin() {
    TRACED();

    unsigned long cap = NeAACDecGetCapabilities();
    // Check if decoder has the needed capabilities

    if (!cap & FIXED_POINT_CAP) {
      LOGE("Fixed Point");
    }

    // Open the library
    hAac = NeAACDecOpen();

    // // Get the current config
    conf = NeAACDecGetCurrentConfiguration(hAac);

    // // If needed change some of the values in conf
    conf->outputFormat = FAAD_FMT_16BIT;
    //conf->defObjectType = LC;
    conf->defSampleRate = info.sample_rate;
    conf->downMatrix = false;
    conf->useOldADTSFormat = false;
    conf->dontUpSampleImplicitSBR = false;

    // Set the new configuration
    if (!NeAACDecSetConfiguration(hAac, conf)) {
      LOGE("NeAACDecSetConfiguration");
    }

    // setup input buffer
    if (input_buffer.size() != buffer_size_input){
      input_buffer.resize(buffer_size_input);
    }
    is_init = false;
  }

  /// Releases the reserved memory
  virtual void end() {
    TRACED();
    flush();
    if (hAac != nullptr) {
      NeAACDecClose(hAac);
      hAac = nullptr;
    }
  }

  /// Write AAC data to decoder
  size_t write(const void *aac_data, size_t len) {
    // Write supplied data to input buffer
    size_t result = input_buffer.writeArray((uint8_t *)aac_data, len);
    // Decode from input buffer
    decode(underflow_limit);

    return result;
  }

  void flush() {
    decode(0);
  }

  /// Defines the input buffer size
  void setInputBufferSize(int len){
    buffer_size_input = len;
  }

  /// Defines the min number of bytes that are submitted to the decoder 
  void setUnderflowLimit(int len){
    underflow_limit = len;
  }

  /// checks if the class is active
  virtual operator bool() { return hAac != nullptr; }

 protected:
  int buffer_size_input = FAAD_INPUT_BUFFER_SIZE;
  int underflow_limit = FAAD_UNDERFLOW_LIMIT;
  NeAACDecHandle hAac = nullptr;
  NeAACDecConfigurationPtr conf;
  SingleBuffer<uint8_t> input_buffer{0};
  bool is_init = false;

  void init(uint8_t *data, size_t len) {
    TRACEI();
    // Initialise the library using one of the initialization functions
    unsigned long samplerate = info.sample_rate;
    unsigned char channels = info.channels;

    if (NeAACDecInit(hAac, data, len, &samplerate, &channels)==-1) {
      LOGE("NeAACDecInit");
    }
    info.sample_rate = samplerate;
    info.channels = channels;
    is_init = true;
  }

  void decode(int minBufferSize) {
    TRACED();
    NeAACDecFrameInfo hInfo;

    // decode until we do not conume any bytes
    while (input_buffer.available()>minBufferSize) {
      int eff_len = input_buffer.available();

      if (!is_init) {
        init(input_buffer.data(), eff_len);
      }

      uint8_t *sample_buffer=(uint8_t *)NeAACDecDecode(hAac, &hInfo, input_buffer.address(), eff_len);

      LOGD("bytesconsumed: %d of %d", (int)hInfo.bytesconsumed, (int)eff_len);
      if (hInfo.error != 0) {
        LOGW("Decoding error: %s", NeAACDecGetErrorMessage(hInfo.error));
      } 
      
      if (hInfo.bytesconsumed == 0 ) {
        break;
      }

      LOGD("Decoded %lu samples", hInfo.samples);
      LOGD("  bytesconsumed: %lu", hInfo.bytesconsumed);
      LOGD("  channels: %d", hInfo.channels);
      LOGD("  samplerate: %lu", hInfo.samplerate);
      LOGD("  sbr: %u", hInfo.sbr);
      LOGD("  object_type: %u", hInfo.object_type);
      LOGD("  header_type: %u", hInfo.header_type);
      LOGD("  num_front_channels: %u", hInfo.num_front_channels);
      LOGD("  num_side_channels: %u", hInfo.num_side_channels);
      LOGD("  num_back_channels: %u", hInfo.num_back_channels);
      LOGD("  num_lfe_channels: %u", hInfo.num_lfe_channels);
      LOGD("  ps: %u", hInfo.ps);

      // removed consumed data
      input_buffer.clearArray(hInfo.bytesconsumed);

      // check for changes in config
      AudioInfo tmp{(int)hInfo.samplerate, hInfo.channels, 16};
      if (tmp != info) {
        setAudioInfo(tmp);
      }

      int bytes = hInfo.samples * sizeof(int16_t);
      size_t len = p_print->write(sample_buffer, bytes);
      if (len != bytes) {
        TRACEE();
      }
    }
  }
};

}  // namespace audio_tools
