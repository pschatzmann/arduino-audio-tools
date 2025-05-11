#pragma once
#include "AudioTools/AudioCodecs/AudioCodecs.h"
#include "AudioTools/AudioCodecs/AudioEncoded.h"
#include "AudioTools/AudioCodecs/CodecOpus.h"
#include "AudioTools/CoreAudio/AudioStreamsConverter.h"
#include "OSCData.h"

#define AAO_MAX_BUFFER 70
#define AAO_ADDRESS_BUFFER 128
#define AOO_MAX_MSG_SIZE (1024 * 2)

namespace audio_tools {

/**
 * @brief Audio sink for AOO (Audio Over OSC) which receives audio data
 * via the indicated input stream and writes it to the defined audio output.
 *
 * It implements the following processing chain:
 *
 *                 ->AudioDecoder->FormatConverterStream->
 * IO Stream-copy()->AudioDecoder->FormatConverterStream->OutputMixer->Output
 *                 ->AudioDecoder->FormatConverterStream->
 *
 * Usually a UDPStream is used for receiving OSC messages and sending out
 * ping confirmations, but you can also use any other Stream. UDP is providing
 * the size of the messages. If you dont use UDP, you will need to switch on
 * the has_length_prefix option.
 *
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AOOSink {
 public:
  AOOSink() {
    // add pcm decoder
    addDecoder("pcm",
               []() { return (AudioDecoder *)new DecoderNetworkFormat(); });
  }

  AOOSink(Stream &io, AudioStream &out) : AOOSink() {
    setStream(io);
    setOutput(out);
  }

  AOOSink(Stream &io, AudioOutput &out) : AOOSink() {
    setStream(io);
    setOutput(out);
  }

  ~AOOSink() { end(); };

  /// Defines the communication stream from which we receive and send the AOO
  /// messages
  void setStream(Stream &io) { p_io = &io; }

  /// Defines the audio output
  void setOutput(AudioOutput &out) {
    p_out = &out;
    notify_info = &out;
  }
  /// Defines the audio output
  void setOutput(AudioStream &out) {
    p_out = &out;
    notify_info = &out;
  }
  /// Defines the audio output
  void setOutput(Print &out) { p_out = &out; }

  /// If the input protocol includes a message length prefix, we should read it
  void setLengthPrefixActive(bool active) { has_length_prefix = active; }

  /// Adds a new Decoder
  void addDecoder(const char *id, AudioDecoder *(*cb)()) {
    codec_factory.addDecoder(id, cb);
  }

  /// Set the sink ID
  void setId(int id) { sink_id = id; }

  /// Get the current sink ID
  int id() { return sink_id; }

  /// Starts the processing
  bool begin(AudioInfo info) {
    setAudioInfo(info);
    return begin();
  }

  /// Starts the processing: chain OutputMixer->Print
  bool begin() {
    if (p_io == nullptr) {
      LOGE("Input not set");
      return false;
    }
    if (p_out == nullptr) {
      LOGE("Output not set");
      return false;
    }

    if (aao_out_buffer.size() < AAO_ADDRESS_BUFFER)
      aao_out_buffer.resize(AAO_ADDRESS_BUFFER);
    if (aao_out_buffer.data() == nullptr) {
      LOGE("Not enough memory");
      return false;
    }

    mixer.setOutput(*p_out);
    mixer.setAutoIndex(false);

    is_active = true;
    return true;
  }

  void end() {
    is_active = false;
    // close and delete all decoders
    for (auto &line : sources) {
      line.p_decoder->end();
      delete line.p_decoder;
      line.p_decoder = nullptr;
    }
    sources.clear();
  }

  /// Defines the output audio info (and target for the FormatConverter)
  void setAudioInfo(AudioInfo info) {
    output_info = info;
    // update audio infor for final output
    notify_info->setAudioInfo(info);
    // update the format converters
    for (AAOSourceLine &line : sources) {
      line.format_converter.begin(line.audio_info, output_info);
    }
  }

  /// Read audio data decode and, mix it and finally output mixed audio
  bool copy() {
    if (!is_active) return false;
    // Process any pending messages
    return processMessages();
  }

  /// Checks if sink is active and has received data recently
  bool isActive() { return is_active; }

  /// Request the format from the indicated source id
  bool requestFormatFrom(int source) { return aooSendRequestFormat(source); }

  /// Defines the mixer buffer size
  void setMixerSize(int size) { mixer.resize(size); }

  /// Provides the number of sources that will be mixed
  int getSourceCount() { return sources.size(); }

 protected:
  /**
   * @brief Source line for AOO (Audio Over OSC) which is used to track the
   * audio data for each source.
   * @author Phil Schatzmann
   */
  struct AAOSourceLine {
    AAOSourceLine() = default;
    AAOSourceLine &operator=(AAOSourceLine &other) {
      source_id = other.source_id;
      sink_id = other.sink_id;
      salt = other.salt;
      last_data_time = other.last_data_time;
      is_active = other.is_active;
      last_frame = other.last_frame;
      block_size = other.block_size;
      channel_onset = other.channel_onset;
      audio_info = other.audio_info;
      mixer_idx = other.mixer_idx;
      format_str = other.format_str;
      p_decoder = other.p_decoder;
      return *this;
    }
    int32_t source_id = 0;
    int32_t sink_id = 0;
    int32_t salt = 0;
    uint32_t last_data_time = 0;
    bool is_active = false;
    int32_t last_frame = -1;
    int32_t block_size = 0;
    int32_t channel_onset = 0;
    AudioInfo audio_info{0, 0, 0};
    AudioDecoder *p_decoder = nullptr;
    FormatConverterStream format_converter;  // copy assignment not supported
    Str format_str;
    int mixer_idx = -1;
  };

  bool has_length_prefix = false;
  bool is_active = false;
  int32_t sink_id = 0;
  Stream *p_io = nullptr;
  OutputMixer<int16_t> mixer;
  CodecFactory codec_factory;
  Vector<uint8_t> aao_out_buffer;
  Vector<uint8_t> aao_in_buffer;
  Print *p_out = nullptr;
  AudioInfoSupport *notify_info = nullptr;
  Vector<AAOSourceLine> sources;
  AudioInfo output_info;

  /// Get the sink ID from the address
  int getSinkIdFromAddress(const char *address) {
    if (StrView(address).startsWith("/AoO/sink/")) {
      return StrView(address + 10).toInt();
    }
    return -1;
  }

  /// Get the source line for the given source id and create a new one if it is
  /// missing
  AAOSourceLine &getSourceLine(int32_t source_id, int32_t sink_id,
                               int32_t salt) {
    TRACED();
    // find existing enty
    for (AAOSourceLine &line : sources) {
      if (line.source_id == source_id && line.sink_id == sink_id &&
          line.salt == salt) {
        return line;
      }
    }

    /// add new entry
    AAOSourceLine result;
    result.source_id = source_id;
    result.sink_id = sink_id;
    result.salt = salt;
    sources.push_back(result);
    return sources[sources.size() - 1];
  }

  /// Process any incoming messages
  bool processMessages() {
    TRACED();
    if (p_io == nullptr || !p_io->available()) return false;

    // Read message size if needed
    size_t msg_size = getMessageSize();

    // Make sure our buffer is large enough
    if (aao_in_buffer.size() < msg_size) aao_in_buffer.resize(msg_size);

    // Read the message
    size_t read = p_io->readBytes(aao_in_buffer.data(), msg_size);

    // Parse OSC message
    OSCData data;
    if (!data.parse(aao_in_buffer.data(), read)) {
      LOGE("Failed to parse OSC message");
      return false;
    }

    // Process based on address pattern
    const char *address = data.getAddress();
    int id = getSinkIdFromAddress(address);
    if (sink_id == 0) {
      sink_id = id;
      LOGI("Setting sink_id: %d", id);
    }

    // Check if the message is for this sink
    if (id != sink_id) {
      LOGI("Message for id %d ignored for id %d", sink_id, id);
      return false;
    }

    // Process the message based on its address
    if (strstr(address, "/format") != nullptr) {
      return processFormatMessage(sink_id, data);
    } else if (strstr(address, "/ping") != nullptr) {
      return processPingMessage(sink_id, data);
    } else if (strstr(address, "/data") != nullptr) {
      return processDataMessage(sink_id, data);
    } else {
      LOGW("Unknown address: %s", address);
    }
    return false;
  }

  /// Parse the format message to fill the AAOSourceLine data
  void parseFormatMessage(OSCData data, AAOSourceLine &tmp) {
    // Parse format data
    tmp.source_id = data.readInt32();
    tmp.salt = data.readInt32();
    tmp.audio_info.channels = data.readInt32();
    tmp.audio_info.sample_rate = data.readInt32();
    tmp.audio_info.bits_per_sample = 16;
    tmp.block_size = data.readInt32();
    tmp.format_str = data.readString();
  }

  /// Process format message  /AoO/sink/<sink>/format ,iiiisb <src>
  //  <salt> <nchannels> <samplerate> <blocksize> <codec> <options>

  bool processFormatMessage(int sink_id, OSCData &data) {
    TRACED();
    // Check the format
    const char *format = data.getFormat();
    if (strcmp(format, "iiiisb") != 0) {
      LOGE("Invalid format message format: %s", format);
      return false;
    }

    // Parse format data
    AAOSourceLine tmp;
    parseFormatMessage(data, tmp);

    // Log info
    LOGI("Received format: ch=%d, rate=%d, blocksize=%d, codec=%s",
         tmp.audio_info.channels, tmp.audio_info.sample_rate, tmp.block_size,
         tmp.format_str.c_str());

    // Update information in source list
    AAOSourceLine &info = getSourceLine(tmp.source_id, sink_id, tmp.salt);

    info.source_id = tmp.source_id;
    info.salt = tmp.salt;
    info.audio_info = tmp.audio_info;
    info.block_size = tmp.block_size;
    info.format_str = tmp.format_str;

    // Setup decoder
    AudioDecoder *p_dec = codec_factory.createDecoder(tmp.format_str.c_str());
    if (p_dec == nullptr) {
      LOGE("Decoder not defined for: %s", tmp.format_str.c_str());
      return false;
    }
    info.p_decoder = p_dec;

    // setup chain Audio<Decoder->FormatConverterStream->OutputMixer
    p_dec->setOutput(info.format_converter);
    if (!p_dec->begin()) {
      LOGE("Decoder failed");
      return false;
    }
    info.format_converter.setOutput(mixer);
    if (!info.format_converter.begin(info.audio_info, output_info)) {
      LOGE("Converter failed");
      return false;
    }

    // Set the mixer information
    if (info.mixer_idx == -1) {
      info.mixer_idx = getSourceCount() - 1;
    }
    mixer.setOutputCount(getSourceCount());
    LOGI("Mixer idx: %d for %d inputs", info.mixer_idx, mixer.size());

    return true;
  }

  /// Process ping message
  bool processPingMessage(int sink_id, OSCData &data) {
    TRACED();
    const char *format = data.getFormat();
    if (strcmp(format, "itt") != 0) {
      LOGE("Invalid ping message format: %s", format);
      return false;
    }

    // Read ping data
    int32_t source_id = data.readInt32();
    int64_t t1 = data.readInt64();
    int64_t t2 = data.readInt64();

    // Reply to ping
    aooSendReplyPing(source_id, t1, t2);
    return true;
  }

  /// Determines the message size from the source, if relevant
  size_t getMessageSize() {
    TRACED();
    size_t msg_size = AOO_MAX_MSG_SIZE;
    if (has_length_prefix) {
      if (p_io->available() < sizeof(int64_t)) {
        LOGW("Not enough data for message size");
        return false;
      }
      int64_t size64;
      if (p_io->readBytes((uint8_t *)&size64, sizeof(size64)) !=
          sizeof(size64)) {
        LOGE("Failed to read message size");
        return false;
      }
      msg_size = (size_t)ntohll(size64);
    }
    LOGI("msg_size: %d", msg_size);
    return msg_size;
  }

  /// Process data message
  bool processDataMessage(int sink_id, OSCData &data) {
    TRACED();
    const char *format = data.getFormat();
    if (strcmp(format, "iiidiiiib") != 0) {
      LOGE("Invalid data message format: %s", format);
      return false;
    }
    // Read data message info
    int32_t source_id = data.readInt32();
    int32_t salt = data.readInt32();
    int32_t seq = data.readInt32();
    double sample_rate = data.readDouble();
    int32_t chan_onset = data.readInt32();
    int32_t total_size = data.readInt32();
    int32_t nframes = data.readInt32();
    int32_t frame = data.readInt32();
    BinaryData audio_data = data.readData();

    AAOSourceLine &info = getSourceLine(sink_id, source_id, salt);

    // Check decoder
    if (info.p_decoder == nullptr) {
      LOGE("Decoder is null");
      return false;
    }

    // Check for dropped frames
    if (info.last_frame >= 0 && seq > info.last_frame + 1) {
      LOGW("Dropped frames: last=%d, current=%d", info.last_frame, seq);
      aooSendRequestData(info, info.last_frame + 1, seq - 1);
    }
    info.last_frame = seq;
    info.last_data_time = millis();

    /// Define the mixer output id;
    mixer.setIndex(info.mixer_idx);
    LOGI("Writing %d to mixer %d", audio_data.len, info.mixer_idx);

    // Pass audio data to decoder
    size_t written = info.p_decoder->write(audio_data.data, audio_data.len);
    if (written != audio_data.len) {
      LOGW("Write incomplete");
    }

    // Process output
    mixer.flushMixer();

    return true;
  }

  ///  request the format from source:  /AoO/src/<src>/format ,i <sink>
  bool aooSendRequestFormat(int source_id) {
    TRACED();
    if (p_io == nullptr) return false;

    char address[AAO_MAX_BUFFER];
    // use data on the stack
    OSCData data{aao_out_buffer.data(), aao_out_buffer.size()};

    snprintf(address, sizeof(address), "/AoO/src/%d/format", source_id);
    data.setAddress(address);
    data.setFormat("i");
    data.write(sink_id);

    Stream *output = (Stream *)p_io;  // Assuming input has write capability
    size_t written = output->write(data.data(), data.size());
    return written == data.size();
  }

  /// request dropped packets: /AoO/src/<src>/data ,ii[ii]* <sink> <salt> [<seq>
  /// <frame>]*
  bool aooSendRequestData(AAOSourceLine &info, int32_t start_seq,
                          int32_t end_seq) {
    LOGE("Requesting data from %d to %d not implemented", start_seq, end_seq);
    return false;
  }

  /// ping message reply from sink to source: /AoO/src/<src>/ping ,ittt sink
  /// <t1> <t2> <t3>
  bool aooSendReplyPing(int source_id, int64_t t1 = 0, int64_t t2 = 0) {
    TRACED();
    if (p_io == nullptr) return false;

    char address[AAO_MAX_BUFFER];
    // use data on the stack
    OSCData data{aao_out_buffer.data(), aao_out_buffer.size()};

    snprintf(address, sizeof(address), "/AoO/src/%d/ping", source_id);
    data.setAddress(address);
    data.setFormat("ittt");
    data.write(sink_id);
    data.write(t1);
    data.write(t2);
    data.write((int64_t)millis());

    Stream *output = (Stream *)p_io;  // Assuming input has write capability
    size_t written = output->write(data.data(), data.size());
    return written == data.size();
  }
};

}  // namespace audio_tools