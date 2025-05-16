/**
 * @file ContainerOSC.h
 * @author Phil Schatzmann
 * @brief  A simple container format which uses OSC messages to
 * tramsmit Header records with audio info and Audio records with the audio
 * data.
 *
 * @version 0.1
 * @date 2025-05-20
 *
 * @copyright Copyright (c) 2022
 *
 */
#pragma once
#include <string.h>

#include "AudioTools/AudioCodecs/AudioCodecsBase.h"
#include "AudioTools/AudioCodecs/MultiDecoder.h"
#include "AudioTools/Communication/OSCData.h"
#include "AudioTools/CoreAudio/AudioBasic/StrView.h"

namespace audio_tools {

/**
 * @brief Wraps the encoded data into OSC info and daata segments so that we
 * can recover the audio configuration and orignial segments.  We assume that a
 * full segment is written with each call of write();
 * @ingroup codecs
 * @ingroup encoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class OSCContainerEncoder : public AudioEncoder {
 public:
  OSCContainerEncoder() = default;
  OSCContainerEncoder(AudioEncoder &encoder) { p_codec = &encoder; }

  void setEncoder(AudioEncoder *encoder) { p_codec = encoder; }

  void setOutput(Print &outStream) {
    LOGD("OSCContainerEncoder::setOutput");
    p_out = &outStream;
  }

  bool begin() override {
    TRACED();
    if (p_codec == nullptr) return false;
    // target.begin();
    is_active = p_codec->begin();
    p_codec->setAudioInfo(audioInfo());
    writeAudioInfo();
    return is_active;
  }

  void setAudioInfo(AudioInfo info) override {
    TRACED();
    writeAudioInfo();
    AudioWriter::setAudioInfo(info);
  }

  /// Add data segment. On first write we also add a AudioInfo header
  size_t write(const uint8_t *data, size_t len) {
    LOGD("OSCContainerEncoder::write: %d", (int)len);
    if ((repeat_info > 0) && (packet_count % repeat_info == 0)) {
      writeAudioInfo();
    }
    writeAudio(data, len);
    packet_count++;
    return len;
  }

  void end() {
    p_codec->end();
    is_active = false;
  }

  operator bool() { return is_active; };

  virtual const char *mime() { return "audio/OSC"; };

  void setRepeatInfoEvery(int packet_count) {
    this->repeat_info = packet_count;
  }

 protected:
  uint64_t packet_count = 0;
  int repeat_info = 0;
  bool is_active = false;
  AudioEncoder *p_codec = nullptr;
  Print *p_out = nullptr;

  void writeAudio(const uint8_t *data, size_t len) {
    LOGD("writeAudio: %d", (int)len);
    uint8_t osc_data[len + 20];  // 20 is guess to cover address & fmt
    OSCData osc{osc_data, sizeof(osc_data)};
    osc.setAddress("/audio/data");
    osc.setFormat("ttb");
    osc.write((uint64_t)millis());
    // we use a uint64_t for a sequence number
    osc.write(packet_count);
    osc.write(data, len);
    p_out->write(osc_data, osc.size());
  }

  void writeAudioInfo() {
    LOGD("writeAudioInfo");
    uint8_t osc_data[100];
    OSCData osc{osc_data, sizeof(osc_data)};
    osc.setAddress("/audio/info");
    osc.setFormat("iiis");
    osc.write((int32_t)audioInfo().sample_rate);
    osc.write((int32_t)audioInfo().channels);
    osc.write((int32_t)audioInfo().bits_per_sample);
    osc.write(p_codec->mime());
    p_out->write(osc_data, osc.size());
  }
};

/**
 * @brief Decodes the provided data from the OSC segments. I recommend to assign
 * a MultiDecoder so that we can support muiltiple audio types.
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class OSCContainerDecoder : public ContainerDecoder {
 public:
  OSCContainerDecoder() = default;
  OSCContainerDecoder(AudioDecoder &decoder) { setDecoder(decoder); }
  OSCContainerDecoder(MultiDecoder &decoder) { setDecoder(decoder); }

  void setDecoder(AudioDecoder &decoder) { p_codec = &decoder; }

  void setDecoder(MultiDecoder &decoder) {
    p_codec = &decoder;
    p_multi_decoder = &decoder;
  }

  void setOutput(Print &outStream) {
    LOGD("OSCContainerDecoder::setOutput")
    p_out = &outStream;
  }

  bool begin() {
    TRACED();
    if (p_codec == nullptr) return false;
    osc.setReference(this);
    osc.addCallback("/audio/info", parseInfo, OSCCompare::StartsWith);
    osc.addCallback("/audio/data", parseData, OSCCompare::StartsWith);
    is_active = true;
    return true;
  }

  void end() { is_active = false; }

  size_t write(const uint8_t *data, size_t len) {
    LOGD("write: %d", (int)len);
    if (!osc.parse((uint8_t *)data, len)) {
      return 0;
    }
    return len;
  }

  operator bool() { return is_active; };

  /// Provides the mime type from the encoder
  const char *mime() { return mime_str.c_str(); };

  /// Replace the write to the decoder with a callback
  void setWriteCallback(
      bool (*write_callback)(uint64_t time, uint64_t seq, uint8_t *data,
                             size_t len, void *ref)) {
    this->write_callback = write_callback;
  }

  /// Provide a reference object to the callback
  void setReference(void *ref) { this->ref = ref; }

 protected:
  bool is_active = false;
  AudioDecoder *p_codec = nullptr;
  MultiDecoder *p_multi_decoder = nullptr;
  SingleBuffer<uint8_t> buffer{0};
  Print *p_out = nullptr;
  OSCData osc;
  Str mime_str;
  bool (*write_callback)(uint64_t time, uint64_t seq, uint8_t *data, size_t len, void* ref) = nullptr;
  void *ref = nullptr;

  static bool parseData(OSCData &osc, void *ref) {
    uint64_t time = osc.readTime();
    uint64_t seq = osc.readTime();
    OSCBinaryData data = osc.readData();
    OSCContainerDecoder *self = static_cast<OSCContainerDecoder *>(ref);
    // call write callbak if defined
    if (self->write_callback != nullptr) {
      return self->write_callback(time, seq, data.data, data.len, ref);
    }
    // output to decoder
    if (self->p_codec != nullptr) {
      self->p_codec->write(data.data, data.len);
    }
    return true;
  }

  static bool parseInfo(OSCData &osc, void *ref) {
    AudioInfo info;
    info.sample_rate = osc.readInt32();
    info.channels = osc.readInt32();
    info.bits_per_sample = osc.readInt32();
    const char *mime = osc.readString();

    OSCContainerDecoder *self = static_cast<OSCContainerDecoder *>(ref);
    if (self != nullptr) {
      self->setAudioInfo(info);
      self->mime_str = mime;
      LOGI("mime: %s", mime);
      // select the right decoder based on the mime type
      if (self->p_multi_decoder != nullptr)
        self->p_multi_decoder->selectDecoder(mime);
    }

    return true;
  }
};

}  // namespace audio_tools