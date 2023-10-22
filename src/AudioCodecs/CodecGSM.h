/**
 * @file CodecGSM.h
 * @author Phil Schatzmann
 * @brief GSM Codec using  https://github.com/pschatzmann/arduino-libgsm
 * @version 0.1
 * @date 2022-04-24
 */

#pragma once

#include "AudioCodecs/AudioEncoded.h"
#include "gsm.h"


namespace audio_tools {

/**
 * @brief Decoder for GSM. Depends on
 * https://github.com/pschatzmann/arduino-libgsm.
 * Inspired by gsmdec.c
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class GSMDecoder : public AudioDecoder {
 public:
  GSMDecoder() {
    cfg.sample_rate = 8000;
    cfg.channels = 1;
  }

  virtual void setAudioInfo(AudioInfo cfg) { this->cfg = cfg; }

  virtual AudioInfo audioInfo() { return cfg; }

  virtual void begin(AudioInfo cfg) {
    setAudioInfo(cfg);
    begin();
  }

  virtual void begin() {
    TRACEI();
    // 160 13-bit samples
    result_buffer.resize(160 * sizeof(int16_t));
    // gsm_frame of 33 bytes
    input_buffer.resize(33);

    v_gsm = gsm_create();
    if (p_notify!=nullptr){
      p_notify->setAudioInfo(cfg);
    }
    is_active = true;
  }

  virtual void end() {
    TRACEI();
    gsm_destroy(v_gsm);
    is_active = false;
  }

  virtual void setNotifyAudioChange(AudioInfoSupport &bi) {
    p_notify = &bi;
  }

  virtual void setOutput(Print &out_stream) { p_print = &out_stream; }

  operator bool() { return is_active; }

  virtual size_t write(const void *data, size_t length) {
    LOGD("write: %d", length);
    if (!is_active) {
      LOGE("inactive");
      return 0;
    }

    uint8_t *p_byte = (uint8_t *) data;
    for (int j = 0; j < length; j++) {
      processByte(p_byte[j]);
    }

    return length;
  }

 protected:
  Print *p_print = nullptr;
  gsm v_gsm;
  AudioInfo cfg;
  AudioInfoSupport *p_notify = nullptr;
  bool is_active = false;
  Vector<uint8_t> input_buffer;
  Vector<uint8_t> result_buffer;
  int input_pos = 0;

  /// Build decoding buffer and decode when frame is full
  void processByte(uint8_t byte) {
    // add byte to buffer
    input_buffer[input_pos++] = byte;

    // decode if buffer is full
    if (input_pos >= input_buffer.size()) {
      if (gsm_decode(v_gsm, input_buffer.data(), (gsm_signal*)result_buffer.data())!=0){
        LOGE("gsm_decode");
      }

      //fromBigEndian(result_buffer);
      // scale to 13 to 16-bit samples
      scale(result_buffer);

      p_print->write(result_buffer.data(), result_buffer.size());
      input_pos = 0;
    }
  }

  void scale(Vector<uint8_t> &vector){
      int16_t *pt16 = (int16_t *)vector.data();
      for (int j = 0; j < vector.size() / 2; j++) {
        if (abs(pt16[j])<=4095){
          pt16[j] = pt16[j] * 8;
        } else if(pt16[j]<0){
          pt16[j] = -32767;
        } else if(pt16[j]>0){
          pt16[j] = 32767;
        }
      }
  }

  void fromBigEndian(Vector<uint8_t> &vector){
    int size = vector.size() / 2;
    int16_t *data16 = (int16_t*) vector.data();
    for (int i=0; i<size; i++){
      data16[i] = ntohs(data16[i]);
    }
  }


};

/**
 * @brief Encoder for GSM - Depends on
 * https://github.com/pschatzmann/arduino-libgsm.
 * Inspired by gsmenc.c
 * @ingroup codecs
 * @ingroup encoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class GSMEncoder : public AudioEncoder {
 public:
  GSMEncoder(bool scaling=true) {
    cfg.sample_rate = 8000;
    cfg.channels = 1;
    scaling_active = scaling;
  }

  void begin(AudioInfo bi) {
    setAudioInfo(bi);
    begin();
  }

  void begin() {
    TRACEI();

    if (cfg.sample_rate != 8000) {
      LOGW("Sample rate is supposed to be 8000 - it was %d", cfg.sample_rate);
    }
    if (cfg.channels != 1) {
      LOGW("channels is supposed to be 1 - it was %d", cfg.channels);
    }

    v_gsm = gsm_create();
    // 160 13-bit samples
    input_buffer.resize(160 * sizeof(int16_t));
    // gsm_frame of 33 bytes
    result_buffer.resize(33);
    is_active = true;
  }

  virtual void end() {
    TRACEI();
    gsm_destroy(v_gsm);
    is_active = false;
  }

  virtual const char *mime() { return "audio/gsm"; }

  virtual void setAudioInfo(AudioInfo cfg) { this->cfg = cfg; }

  virtual void setOutput(Print &out_stream) { p_print = &out_stream; }

  operator bool() { return is_active; }

  virtual size_t write(const void *in_ptr, size_t in_size) {
    LOGD("write: %d", in_size);
    if (!is_active) {
      LOGE("inactive");
      return 0;
    }
    // encode bytes
    uint8_t *p_byte = (uint8_t *) in_ptr;
    for (int j = 0; j < in_size; j++) {
      processByte(p_byte[j]);
    }
    return in_size;
  }

 protected:
  AudioInfo cfg;
  Print *p_print = nullptr;
  gsm v_gsm;
  bool is_active = false;
  int buffer_pos = 0;
  bool scaling_active;
  Vector<uint8_t> input_buffer;
  Vector<uint8_t> result_buffer;

  // add byte to decoding buffer and decode if buffer is full
  void processByte(uint8_t byte) {
    input_buffer[buffer_pos++] = byte;
    if (buffer_pos >= input_buffer.size()) {
      scaleValues(input_buffer);
      // toBigEndian(input_buffer);
      // encode
      gsm_encode(v_gsm, (gsm_signal*)input_buffer.data(), result_buffer.data());
      size_t written = p_print->write(result_buffer.data(), result_buffer.size());
      assert(written == result_buffer.size());
      buffer_pos = 0;
    }
  }

  void toBigEndian(Vector<uint8_t> &vector){
    int size = vector.size() / 2;
    int16_t *data16 = (int16_t*) vector.data();
    for (int i=0; i<size; i++){
      data16[i] = htons(data16[i]);
    }
  }


  void scaleValues(Vector<uint8_t> &vector) {
    int16_t *pt16 = (int16_t *)vector.data();
    int size = vector.size() / 2;
    if (scaling_active){
      // scale to 16 to 13-bit samples
      for (int j = 0; j < size; j++) {
        pt16[j] = pt16[j] / 8;
      }
    } else {
      // clip value to 13-bits
      for (int j = 0; j < size; j++) {
        if ( pt16[j]>4095){
          pt16[j] = 4095;
        }
        if ( pt16[j]<-4095){
          pt16[j] = -4095;
        }
      }
    }
  }
};

}  // namespace audio_tools