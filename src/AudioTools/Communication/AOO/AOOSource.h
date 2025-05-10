#pragma once
#include "AudioTools/AudioCodecs/AudioCodecs.h"
#include "AudioTools/AudioCodecs/AudioEncoded.h"
#include "AudioTools/CoreAudio/AudioOutput.h"
#include "OSCData.h"

#define AAO_MAX_BUFFER 70
#define AAO_ADDRESS_BUFFER 128

namespace audio_tools {

/**
 * @brief Audio source for AOO (Audio Over OSC) which is used to send audio data
 * via the indicated output stream.
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AOOSource : public AudioOutput {
 public:
  AOOSource(Print &output) { setOutput(output); }
  ~AOOSource() { end(); };

  /// Defines the output stream to which we send the AOO data
  void setOutput(Print &output) { p_output = &output; }

  /// If the output protocol does not support the message length, we write it as
  /// a prefix
  void setIsWriteLengthPrefix(bool active) { is_write_length_prefix = active; }

  /// Defines the coder if we do not send PCM data
  void setEncoder(AudioEncoder &encoder) { p_encoder = &encoder; }

  /// Resets the encoder to use PCM data
  void clearEncoder() { p_encoder = &pcm_encoder; }

  void setAudioInfo(const AudioInfo &info) {
    if (p_encoder != nullptr) p_encoder->setAudioInfo(info);
    AudioOutput::setAudioInfo(info);
  }

  /// Starts the processing
  bool begin() {
    if (p_encoder == nullptr) {
      LOGE("Encoder not set");
      return false;
    }
    if (p_output == nullptr) {
      LOGE("Output not set");
      return false;
    }

    // assign ramdom number to salt
    if (salt == 0) {
      randomSeed(millis());
      salt = random(-2147483648, 2147483647);
    }

    if (aao_send_data.size() < AAO_ADDRESS_BUFFER)
      aao_send_data.resize(AAO_ADDRESS_BUFFER);

    /// setup encoder
    encoder_output.source = this;
    p_encoder->setOutput(encoder_output);
    p_encoder->setAudioInfo(audioInfo());
    if (!p_encoder->begin()) {
      LOGE("Encoder failed");
      return false;
    }
    // send initial format information
    if (!aoo_send_info()) {
      LOGE("Failed to send format information");
      return false;
    }

    return AudioOutput::begin();
  }

  /// Ends the processing
  void end() {
    if (p_encoder != nullptr) p_encoder->end();
    AudioOutput::end();
  }

  size_t write(const uint8_t *data, size_t len) override {
    // send ping message every second
    if (!ping()) {
      LOGW("Ping failed");
    }
    block_size = len;
    if (len == 0 || p_encoder == nullptr || p_output == nullptr) return 0;
    if (is_write_length_prefix) {
      int64_t len64 = htonll(static_cast<int64_t>(len));
      p_output->write((uint8_t *)&len64, sizeof(len64));
    }

    // send data
    return p_encoder->write(data, len);
  }

  void setSinkId(int id) { sink_id = id; }

  int sinkId() { return sink_id; }

  /// sends ping every second: this is also automatically called by write()
  bool ping() { return aoo_send_ping(); }

 protected:
  int32_t sink_id = 1;
  uint64_t ping_timeout = 0;
  Print *p_output = nullptr;
  EncoderNetworkFormat pcm_encoder;
  AudioEncoder *p_encoder = &pcm_encoder;
  bool is_write_length_prefix = false;
  int32_t salt = 0;
  int32_t frame = 0;
  int32_t block_size = 1024;
  int32_t channel_onset = 0;
  Vector<uint8_t> aao_send_data;

  /// Write output of encoder to the defined output stream
  class EncoderOutput : public AudioOutput {
   public:
    AOOSource *source = nullptr;
    size_t write(const uint8_t *data, size_t len) override {
      if (!source->aoo_send_data(data, len)) {
        return 0;
      }
      return len;
    }
  } encoder_output;

  /// Determines the codec type
  const char *codecStr() {
    if (p_encoder == nullptr || p_encoder == &pcm_encoder) return "pcm";
    if (StrView(p_encoder->mime()).startsWith("audio"))
      return p_encoder->mime() + 6;
    return "pcm";
  }

  //  notify sinks about format changes: /AoO/sink/<sink>/format ,iiiisb <src>
  //  <salt> <nchannels> <samplerate> <blocksize> <codec> <options>
  bool aoo_send_info() {
    if (p_output == nullptr) return false;
    char address[AAO_MAX_BUFFER];
    // use data on the stack
    OSCData data{aao_send_data.data(), aao_send_data.size()};

    snprintf(address, sizeof(address), "/AoO/sink/%d/format", sink_id);
    data.setAddress(address);
    data.setFormat("iiiisb");
    data.write(salt);
    data.write((int32_t)p_encoder->audioInfo().channels);
    data.write((int32_t)p_encoder->audioInfo().sample_rate);
    data.write(block_size);
    data.write(codecStr());
    data.write((uint8_t *)"", 0);  // options
    size_t written = p_output->write((const uint8_t *)data.data(), data.size());
    if (written != data.size()) {
      return false;
    }
    return true;
  }

  //  ping message from source to sink (usually sent once per second):
  //  ``/AoO/sink/<sink>/ping ,itt <sink> <t1> <t2>``
  bool aoo_send_ping() {
    if (millis() > ping_timeout) {
      ping_timeout = millis() + 1000;
      char address[AAO_MAX_BUFFER];
      // use data on the stack
      OSCData data{aao_send_data.data(), aao_send_data.size()};
      snprintf(address, sizeof(address), "/AoO/sink/%d/ping", sink_id);
      data.setAddress(address);
      data.setFormat("itt");
      data.write(sink_id);
      data.write((int64_t)millis());
      data.write((int64_t)millis());
      size_t written =
          p_output->write((const uint8_t *)data.data(), data.size());
      if (written != data.size()) {
        return false;
      }
      return true;
    }
    return false;
  }

  //  deliver audio data, large blocks are split across several frames:
  //  ``/AoO/sink/<sink>/data ,iiidiiiib <src> <salt> <seq> <samplerate>
  //  <channel_onset> <totalsize> <nframes> <frame> <data>``
  bool aoo_send_data(const uint8_t *audioData, size_t len) {
    if (p_output == nullptr) return false;
    if (aao_send_data.size() < len + AAO_MAX_BUFFER)
      aao_send_data.resize(len + AAO_MAX_BUFFER);
    char address[AAO_MAX_BUFFER];
    snprintf(address, sizeof(address), "/AoO/sink/%d/data", sink_id);
    // use data on the heap
    OSCData data{aao_send_data.data(), aao_send_data.size()};
    data.setAddress(address);
    data.setFormat("iiidiiiib");
    data.write(sink_id);
    data.write(salt);
    data.write(frame++);
    data.write((double)p_encoder->audioInfo().sample_rate);
    data.write(channel_onset);  // channel_onset
    data.write(block_size);
    data.write((int32_t)1);      // nframes
    data.write(audioData, len);  // frame
    size_t written = p_output->write((const uint8_t *)data.data(), data.size());
    if (written != data.size()) {
      return false;
    }
    return true;
  }
};

}  // namespace audio_tools