/**
 * @file CodecILBC.h
 * @author Phil Schatzmann
 * @brief Codec for ilbc using https://github.com/pschatzmann/libilbc
 * @version 0.1
 * @date 2022-04-24
 *
 * @copyright Copyright (c) 2022
 *
 */
#pragma once

#include "AudioCodecs/AudioEncoded.h"
#include "iLBC.h"


namespace audio_tools {

/**
 * @brief Decoder for iLBC. Depends on
 * https://github.com/pschatzmann/libilbc
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class ILBCDecoder : public AudioDecoder {
 public:
  ILBCDecoder(EnumLBCFrameSize frameSize = ms30, bool useEnhancer = true) {
    info.sample_rate = 8000;
    info.channels = 1;
    info.bits_per_sample =  16;
    frame_size = frameSize;
    use_enhancer = useEnhancer;
  }

  ~ILBCDecoder(){
    end();
  }

  virtual AudioInfo audioInfo() { return info; }

  virtual void begin() {
    TRACEI();
    if (p_print==nullptr){
      LOGE("Output not defined");
      return;
    }

    if (p_ilbc==nullptr){
      p_ilbc = new iLBCDecode(frame_size, use_enhancer);
    }
    
    // setup buffer
    decoded_buffer.resize(p_ilbc->getSamples());
    encoded_buffer.resize(p_ilbc->getEncodedBytes());

    // update audio information
    if (notify != nullptr) {
      notify->setAudioInfo(info);
    }
  }

  virtual void end() {
    TRACEI();
    delete p_ilbc;
    p_ilbc = nullptr;
  }

  virtual void setNotifyAudioChange(AudioInfoSupport &bi) {
    notify = &bi;
  }

  virtual void setOutput(Print &out_stream) { p_print = &out_stream; }

  operator bool() { return p_ilbc != nullptr; }

  virtual size_t write(const void *input_buffer, size_t length) {
    if (p_ilbc==nullptr) return 0;
    LOGI("write: %d", length);
    int samples = length / sizeof(int16_t);
    int16_t *p_samples = (int16_t *)input_buffer;
    for (int j=0;j<samples;j++){
      encoded_buffer[encoded_buffer_pos++]=p_samples[j];
      if (encoded_buffer_pos>=encoded_buffer.size()){
        memset(decoded_buffer.data(),0,decoded_buffer.size()*sizeof(int16_t));
        p_ilbc->decode(encoded_buffer.data(), decoded_buffer.data());
        if (p_print!=nullptr){
          p_print->write((uint8_t*)decoded_buffer.data(), decoded_buffer.size()*sizeof(int16_t));
          delay(2);
        }
        encoded_buffer_pos = 0;
      }
    }
    return length;
  }

 protected:
  AudioInfo info;
  AudioInfoSupport *notify = nullptr;
  Print *p_print = nullptr;
  iLBCDecode *p_ilbc = nullptr;
  Vector<int16_t> decoded_buffer{0};
  Vector<uint8_t> encoded_buffer{0};
  int16_t encoded_buffer_pos = 0;
  EnumLBCFrameSize frame_size;
  bool use_enhancer;

};

/**
 * @brief Encoder for iLBC - Depends on
 * https://github.com/pschatzmann/libopenilbc
 * @ingroup codecs
 * @ingroup encoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class ILBCEncoder : public AudioEncoder {
 public:
  ILBCEncoder(EnumLBCFrameSize frameSize = ms30) {
    info.sample_rate = 8000;
    info.channels = 1;
    info.bits_per_sample =  16;
    frame_size = frameSize;
  }

  ~ILBCEncoder(){
    end();
  }

  void begin(AudioInfo info) {
    setAudioInfo(info);
    begin();
  }

  void setAudioInfo(AudioInfo info) {
    this->info = info;
  }

  void begin() {
    TRACEI();
    if (p_print==nullptr){
      LOGE("Output not defined");
      return;
    }
    if (info.bits_per_sample!=16){
      LOGE("bits_per_sample must be 16: %d",info.bits_per_sample);
      return;
    }
    if (info.sample_rate!=8000){
      LOGW("The sample rate should be 8000: %d", info.sample_rate);
    }
    if (info.channels!=1){
      LOGW("channels should be 1: %d", info.channels);
    }
    if (p_ilbc==nullptr){
      p_ilbc = new iLBCEncode(frame_size);
    }
    decoded_buffer.resize(p_ilbc->getSamples());
    encoded_buffer.resize(p_ilbc->getEncodedBytes());
    decoded_buffer_pos = 0;
  }

  virtual void end() {
    TRACEI();
    if (p_ilbc != nullptr) {
      delete p_ilbc;
      p_ilbc = nullptr;
    }
  }

  virtual const char *mime() { return "audio/ilbc"; }

  virtual void setOutput(Print &out_stream) { p_print = &out_stream; }

  operator bool() { return p_ilbc != nullptr; }

  virtual size_t write(const void *in_data, size_t in_size) {
    if (p_ilbc==nullptr) return 0;
    LOGI("write: %d", in_size);

    int samples = in_size / sizeof(int16_t);
    int16_t *p_samples = (int16_t *)in_data;

    for (int j=0;j<samples;j++){
      decoded_buffer[decoded_buffer_pos++]=p_samples[j];
      if (decoded_buffer_pos>=decoded_buffer.size()){
        memset(encoded_buffer.data(),0,encoded_buffer.size());
        p_ilbc->encode(decoded_buffer.data(), encoded_buffer.data());
        if (p_print!=nullptr){
          p_print->write(encoded_buffer.data(), encoded_buffer.size());
        }
        decoded_buffer_pos = 0;
      }
    }
    return in_size;
  }

 protected:
  AudioInfo info;
  Print *p_print = nullptr;
  iLBCEncode *p_ilbc = nullptr;
  Vector<float> decoded_buffer{0};
  Vector<uint8_t> encoded_buffer{0};
  int16_t decoded_buffer_pos = 0;
  EnumLBCFrameSize frame_size;
};

}  // namespace audio_tools