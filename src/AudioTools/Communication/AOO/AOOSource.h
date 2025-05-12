#pragma once
#include "AOOBuffers.h"
#include "AudioTools/AudioCodecs/AudioCodecs.h"
#include "AudioTools/AudioCodecs/AudioEncoded.h"
#include "AudioTools/CoreAudio/AudioOutput.h"
#include "OSCData.h"

#define AAO_MAX_BUFFER 70
#define AAO_ADDRESS_BUFFER 128

namespace audio_tools {

/**
 * @brief Audio source for AOO (Audio Over OSC) which is used to send audio data
 * via the indicated output stream (usually a UDPStream). We provide a simple
 * implementation which is purely based on the AudioTools library w/o any
 * external dependencies. The call to write() will send the data to the output
 * stream and receive ping and resend requests. If you pause to call write(),
 * make sure that you continue to call receive() to keep the ping alive.
 *
 * By default we tramsmit PCM data, but you can also use any other encoder. If
 * you decide to use a CopyEnoder to write already encoded data, you need to
 * make sure that the writes are throttled by the decoded data rate, otherwise
 * the AAOSink will run into buffer overflows.
 *
 * @attention Currently we do not support the split into multiple frames, so for
 * each seq no we send only one full frame. In order to support the reseding of
 * missing frames we need to define the timeout which defines how long we keep
 * the sent data in a buffer.
 *
 * @ingroup communications
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AOOSource : public AudioOutput {
 public:
  AOOSource(Stream &output, uint16_t bufferTimeMs) {
    setStream(output);
    setBufferTimeout(bufferTimeMs);
  }
  ~AOOSource() { end(); };

  /// Defines the output stream to which we send the AOO data
  void setStream(Stream &output) { p_stream = &output; }

  /// If the output protocol does not support the message length, we write it as
  /// a prefix
  void setLengthPrefixActive(bool active) { is_write_length_prefix = active; }

  /// Defines the encoder if we do not send PCM data
  void setEncoder(const char *format, AudioEncoder &encoder) {
    p_encoder = &encoder;
    encoder_format = format;
  }

  /// Resets the encoder to use PCM data
  void clearEncoder() { p_encoder = &pcm_encoder; }

  void setAudioInfo(const AudioInfo info) override {
    if (p_encoder != nullptr) p_encoder->setAudioInfo(info);
    AudioOutput::setAudioInfo(info);
  }

  /// Starts the processing
  bool begin(AudioInfo info) override { return AudioOutput::begin(info); }

  /// Starts the processing
  bool begin() override {
    if (p_encoder == nullptr) {
      LOGE("Encoder not set");
      return false;
    }
    if (p_stream == nullptr) {
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
    if (!receive()) {
      LOGW("Ping failed");
    }
    block_size = len;
    if (len == 0 || p_encoder == nullptr || p_stream == nullptr) return 0;
    if (is_write_length_prefix) {
      uint64_t len64 = htonll(static_cast<uint64_t>(len));
      p_stream->write((uint8_t *)&len64, sizeof(len64));
    }

    // send data
    return p_encoder->write(data, len);
  }

  void setSinkId(int id) { sink_id = id; }

  int sinkId() { return sink_id; }

  /// sends ping every second: this is also automatically called by write()
  bool receive() {
    aoo_receive();
    return aoo_send_ping();
  }

  /// Defines the time a resend buffer entry is kept in the buffer
  void setBufferTimeout(int timeMs) { aao_out_buffer.setTimeout(timeMs); }

 protected:
  int32_t sink_id = 1;
  uint64_t ping_timeout = 0;
  Stream *p_stream = nullptr;
  const char *encoder_format = "pcm";
  EncoderNetworkFormat pcm_encoder;
  AudioEncoder *p_encoder = &pcm_encoder;
  bool is_write_length_prefix = false;
  int32_t salt = 0;
  int32_t frame = 0;
  int32_t block_size = 1024;
  int32_t channel_onset = 0;
  Vector<uint8_t> aao_send_data;
  AOOSourceBuffer aao_out_buffer;

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
  const char *codecStr() { return encoder_format; }

  //  notify sinks about format changes: /AoO/sink/<sink>/format ,iiiisb <src>
  //  <salt> <nchannels> <samplerate> <blocksize> <codec> <options>
  bool aoo_send_info() {
    if (p_stream == nullptr) return false;
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
    size_t written = p_stream->write((const uint8_t *)data.data(), data.size());
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
      data.write(millis());
      data.write(millis());
      size_t written =
          p_stream->write((const uint8_t *)data.data(), data.size());
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
    if (p_stream == nullptr) return false;
    if (aao_send_data.size() < len + AAO_MAX_BUFFER)
      aao_send_data.resize(len + AAO_MAX_BUFFER);

    LOGI("aoo_send_data: %d", len);
    aao_out_buffer.writeArray(frame, audioData, len);
    char address[AAO_MAX_BUFFER];
    snprintf(address, sizeof(address), "/AoO/sink/%d/data", sink_id);
    // use data on the heap
    OSCData data{aao_send_data.data(), aao_send_data.size()};
    data.setAddress(address);
    data.setFormat("iiidiiiib");
    data.write(sink_id);
    data.write(salt);
    data.write(frame++);  // seq
    data.write((double)p_encoder->audioInfo().sample_rate);
    data.write(channel_onset);  // channel_onset
    data.write(block_size);     // total size
    data.write((int32_t)1);     // nframes
    data.write(audioData, 0);   // frame
    size_t written = p_stream->write((const uint8_t *)data.data(), data.size());
    if (written != data.size()) {
      return false;
    }
    return true;
  }

  /// Determines the message size for parsing the ping confirmation message
  size_t getMessageSize() {
    TRACED();
    size_t msg_size = AAO_MAX_BUFFER;
    if (is_write_length_prefix) {
      p_stream->setTimeout(5);
      if (p_stream->available() < sizeof(uint64_t)) {
        LOGW("Not enough data for message size");
        return false;
      }
      uint64_t size64;
      if (p_stream->readBytes((uint8_t *)&size64, sizeof(size64)) !=
          sizeof(size64)) {
        LOGE("Failed to read message size");
        return false;
      }
      msg_size = (size_t)ntohll(size64);
    }
    LOGI("msg_size: %d", msg_size);
    return msg_size;
  }

  bool aoo_receive() {
    TRACED();
    // Read message size if needed
    size_t msg_size = getMessageSize();
    uint8_t buffer[msg_size];
    // Make sure our buffer is large enough
    // Read the message
    size_t read = p_stream->readBytes(buffer, msg_size);

    // Stop if there is no data
    if (read == 0) {
      return true;
    }

    // Parse OSC message
    OSCData data;
    if (!data.parse(buffer, read)) {
      LOGE("Failed to parse OSC message");
      return false;
    }

    // Process the ping reply
    const char *address = data.getAddress();
    if (StrView(address).contains("/ping")) {
      return processPingReply(data);
    } else if (StrView(address).contains("/data")) {
      return processResendRequest(data);
    } else {
      LOGW("Unknown address: %s", address);
    }
    return true;
  }

  /// Process ping message
  bool processPingReply(OSCData &data) {
    TRACED();
    const char *format = data.getFormat();
    if (strcmp(format, "ittt") != 0) {
      LOGE("Invalid ping message format: %s", format);
      return false;
    }

    // Read ping data
    int32_t source_id = data.readInt32();
    uint64_t t1 = data.readUint64();
    uint64_t t2 = data.readUint64();
    uint64_t t3 = data.readUint64();

    LOGI("ping reply: %ld %ld %ld", t1, t2, t3);

    // Reply to ping
    return true;
  }

  /// request dropped packets: /AoO/src/<src>/data ,ii[ii]* <sink> <salt> [<seq>
  /// <frame>]*
  bool processResendRequest(OSCData &data) {
    TRACED();
    const char *format = data.getFormat();
    StrView formatStr(format);
    int32_t sink = data.readInt32();
    int32_t salt = data.readInt32();

    int n = (formatStr.length() - 2) / 2;
    for (int j = 0; j < n; j++) {
      int32_t seq = data.readInt32();
      int32_t frame = data.readInt32();
      LOGI("Resend - seq: %d frame: %d", seq, frame);
      SingleBuffer<uint8_t> *buffer = aao_out_buffer.getBuffer(seq);
      if (buffer == nullptr) {
        LOGE("Resend: Buffer not found");
        return false;
      }
      aoo_send_data(buffer->data(), buffer->available());
    }
    return true;
  }
};

}  // namespace audio_tools