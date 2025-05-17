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
 * @brief Wraps the encoded data into OSC info and data segments so that the
 * receiver can recover the audio configuration and orignial segments.
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

  void setOutput(Print &outStream) { p_out = &outStream; }

  bool begin() override {
    TRACED();
    if (p_codec == nullptr) return false;
    osc_out.setOutput(*p_out);
    osc_out.begin();
    p_codec->setOutput(osc_out);
    p_codec->setAudioInfo(audioInfo());
    is_active = p_codec->begin();
    writeAudioInfo(audioInfo(), p_codec->mime());
    return is_active;
  }

  void setAudioInfo(AudioInfo info) override {
    TRACED();
    if (is_active) writeAudioInfo(audioInfo(), p_codec->mime());
    AudioWriter::setAudioInfo(info);
  }

  /// Add data segment. On first write we also add a AudioInfo header
  size_t write(const uint8_t *data, size_t len) {
    LOGD("OSCContainerEncoder::write: %d", (int)len);
    if ((repeat_info > 0) && (packet_count % repeat_info == 0)) {
      writeAudioInfo(audioInfo(), p_codec->mime());
    }
    p_codec->write(data, len);
    packet_count++;
    return len;
  }

  void end() {
    p_codec->end();
    is_active = false;
  }

  operator bool() { return is_active; };

  virtual const char *mime() { return "audio/OSC"; };

  /// Activate/deactivate the sending of the audio info
  void setInfoActive(bool flag) { is_send_info_active = flag; }
  /// Automatically resend audio info ever nth write.
  void setRepeatInfoEvery(int packet_count) {
    this->repeat_info = packet_count;
  }

  /// Returns the sequence number of the next packet
  uint64_t getSequenceNumber() { return osc_out.getSequenceNumber(); }

  /// Define a reference object to be provided by the callback
  void setReference(void *ref) { osc_out.setReference(ref); }

  /// Get informed about the encoded packages
  void setEncodedWriteCallback(void (*write_callback)(uint8_t *data, size_t len,
                                                      uint64_t seq,
                                                      void *ref)) {
    osc_out.setEncodedWriteCallback(write_callback);
  }

  /// Resend the encoded data
  size_t resendEncodedData(uint8_t *data, size_t len, uint64_t seq) {
    return osc_out.write(data, len, seq);
  }

 protected:
  uint64_t packet_count = 0;
  int repeat_info = 0;
  bool is_active = false;
  bool is_send_info_active = true;
  AudioEncoder *p_codec = nullptr;
  Print *p_out = nullptr;

  /// Output Encoded Audio via OSC
  class OSCOutput : public AudioOutput {
   public:
    void setReference(void *ref) { this->ref = ref; }
    void setOutput(Print &outStream) { p_out = &outStream; }
    void setEncodedWriteCallback(void (*write_callback)(
        uint8_t *data, size_t len, uint64_t seq, void *ref)) {
      this->encoded_write_callback = write_callback;
    }
    uint64_t getSequenceNumber() { return sequence_number; }
    bool begin() {
      sequence_number = 0;
      return true;
    }
    size_t write(const uint8_t *data, size_t len) override {
      size_t result = write(data, len);
      sequence_number++;
      return result;
    }
    size_t write(const uint8_t *data, size_t len, uint64_t seq) {
      LOGD("writeAudio: %d", (int)len);
      if (encoded_write_callback != nullptr) {
        encoded_write_callback((uint8_t *)data, len, sequence_number, ref);
      }
      uint8_t osc_data[len + 20];  // 20 is guess to cover address & fmt
      OSCData osc{osc_data, sizeof(osc_data)};
      osc.setAddress("/audio/data");
      osc.setFormat("ttb");
      osc.write((uint64_t)millis());
      // we use a uint64_t for a sequence number
      osc.write(sequence_number);
      osc.write(data, len);
      p_out->write(osc_data, osc.size());
      return len;
    }

   protected:
    void (*encoded_write_callback)(uint8_t *data, size_t len, uint64_t seq,
                                   void *ref) = nullptr;
    Print *p_out = nullptr;
    uint64_t sequence_number = 0;
    void *ref = nullptr;
  } osc_out;

  /// OUtput AudioInfo via OSC
  void writeAudioInfo(AudioInfo info, const char *mime) {
    if (is_send_info_active) {
      LOGD("writeAudioInfo");
      uint8_t osc_data[100];
      OSCData osc{osc_data, sizeof(osc_data)};
      osc.setAddress("/audio/info");
      osc.setFormat("iiis");
      osc.write((int32_t)info.sample_rate);
      osc.write((int32_t)info.channels);
      osc.write((int32_t)info.bits_per_sample);
      osc.write(mime);
      p_out->write(osc_data, osc.size());
    }
  }
};

/**
 * @brief Decodes the provided data from the OSC segments. I recommend to
 * assign a MultiDecoder so that we can support muiltiple audio types.
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class OSCContainerDecoder : public ContainerDecoder {
 public:
  OSCContainerDecoder() = default;
  OSCContainerDecoder(AudioDecoder &decoder) {
    setDecoder(decoder);
  }
  OSCContainerDecoder(MultiDecoder &decoder) {
    setDecoder(decoder);
  }

  /// Defines the decoder to be used
  void setDecoder(AudioDecoder &decoder) { p_codec = &decoder; }

  /// Defines the decoder to be used: special logic for multidecoder
  void setDecoder(MultiDecoder &decoder) {
    p_codec = &decoder;
    is_multi_decoder = true;
  }

  /// Optionally define you own OSCData object
  void setOSCData(OSCData &osc) { p_osc = &osc; }

  void setOutput(Print &outStream) {
    LOGD("OSCContainerDecoder::setOutput")
    p_out = &outStream;
  }

  bool begin() {
    TRACED();
    if (p_codec == nullptr || p_osc == nullptr) return false;
    p_osc->setReference(this);
    p_osc->addCallback("/audio/info", parseInfo, OSCCompare::StartsWith);
    p_osc->addCallback("/audio/data", parseData, OSCCompare::StartsWith);
    is_active = true;
    return true;
  }

  void end() { is_active = false; }

  size_t write(const uint8_t *data, size_t len) {
    if (!is_active) return 0;
    LOGD("write: %d", (int)len);
    if (!p_osc->parse((uint8_t *)data, len)) {
      return 0;
    }
    return len;
  }

  operator bool() { return is_active; };

  /// Provides the mime type from the encoder
  const char *mime() { return mime_str.c_str(); };

  /// Provides the sequence number of the last packet
  uint64_t getSequenceNumber() { return seq_no; }

  /// Adds an new parser callback for a specific address matching string
  bool addParserCallback(const char *address,
                         bool (*callback)(OSCData &data, void *ref),
                         OSCCompare compare = OSCCompare::Matches) {
    if (p_osc == nullptr) return false;
    p_osc->addCallback(address, callback, compare);
    return true;
  }

  /// Replace the write to the decoder with a callback:
  void setWriteCallback(bool (*write_callback)(uint64_t time, uint64_t seq,
                                               uint8_t *data, size_t len,
                                               void *ref)) {
    this->write_callback = write_callback;
  }

  /// Callback to be called when data is missing
  void setMissingDataCallback(void (*missing_data_callback)(uint64_t from_seq,
                                                            uint64_t to_seq,
                                                            void *ref)) {
    this->missing_data_callback = missing_data_callback;
  }

  /// Provide a reference object to the callback
  void setReference(void *ref) { this->ref = ref; }

 protected:
  bool is_active = false;
  bool is_multi_decoder = false;
  AudioDecoder *p_codec = nullptr;
  SingleBuffer<uint8_t> buffer{0};
  Print *p_out = nullptr;
  OSCData osc_default;
  OSCData *p_osc = &osc_default;
  Str mime_str;
  uint64_t seq_no = 0;
  /// Return false to complete the processing w/o writing to the decoder
  bool (*write_callback)(uint64_t time, uint64_t seq, uint8_t *data, size_t len,
                         void *ref) = nullptr;
  void (*missing_data_callback)(uint64_t from_seq, uint64_t to_seq,
                                void *ref) = missingDataCallback;
  void *ref = nullptr;

  /// Default callback for missing data: just log the missing range
  static void missingDataCallback(uint64_t from_seq, uint64_t to_seq,
                                  void *ref) {
    LOGW("Missing sequence numbers %d - %d", from_seq, to_seq);
  }

  static bool parseData(OSCData &osc, void *ref) {
    uint64_t time = osc.readTime();
    uint64_t seq = osc.readTime();
    OSCBinaryData data = osc.readData();
    OSCContainerDecoder *self = static_cast<OSCContainerDecoder *>(ref);
    // Check for missing sequence numbers
    if (self->seq_no + 1 != seq) {
      self->missing_data_callback(self->seq_no + 1, seq - 1, self->ref);
    }
    // store the actual sequence number
    self->seq_no = seq;
    // call write callbak if defined
    if (self->write_callback != nullptr) {
      bool ok = self->write_callback(time, seq, data.data, data.len, ref);
      if (!ok) return true;
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
      if (self->is_multi_decoder)
        static_cast<MultiDecoder*>(self->p_codec)->selectDecoder(mime);
    }

    return true;
  }
};

}  // namespace audio_tools