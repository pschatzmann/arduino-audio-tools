/**
 * @file ContainerBinary.h
 * @author Phil Schatzmann
 * @brief  A lean and efficient container format which provides Header records
 * with audio info, Audio records with the audio, VideoHeader records with
 * video info, Video records with already-encoded video/image frames (e.g.
 * one H.264 access unit or one complete JPEG image - see writeVideo()/
 * setOutputVideo()), and Meta which can contain any additional information.
 * This can be used together with a codec which does not transmit the audio
 * information or has variable frame lengths. We expect that a single
 * write()/writeVideo() call is providing full frames.
 *
 * @version 0.1
 * @date 2022-05-04
 *
 * @copyright Copyright (c) 2022
 *
 */
#pragma once
#include <string.h>

#include "AudioTools/AudioCodecs/AudioCodecsBase.h"
#include "AudioTools/CoreAudio/AudioBasic/StrView.h"
#include "AudioTools/Video/Video.h"

namespace audio_tools {

enum class ContainerType : uint8_t {
  Header = 1,
  Audio = 2,
  Meta = 3,
  VideoHeader = 4,
  Video = 5,
  Undefined = 0
};

struct CommonHeader {
  CommonHeader() = default;
  CommonHeader(ContainerType type, uint16_t len) {
    this->type = type;
    this->len = len;
  }
  char header[2] = {'\r','\n'};
  ContainerType type;
  uint16_t len;
  uint8_t checksum = 0;
};

struct SimpleContainerConfig {
  SimpleContainerConfig() = default;
  CommonHeader common{ContainerType::Header, sizeof(AudioInfo)};
  AudioInfo info;
};

struct SimpleContainerDataHeader {
  CommonHeader common{ContainerType::Audio, 0};
};

struct SimpleContainerMetaDataHeader {
  CommonHeader common{ContainerType::Meta, 0};
};

struct SimpleContainerVideoConfig {
  SimpleContainerVideoConfig() = default;
  CommonHeader common{ContainerType::VideoHeader, sizeof(VideoInfo)};
  VideoInfo info;
};

struct SimpleContainerVideoDataHeader {
  CommonHeader common{ContainerType::Video, 0};
};

// struct ProcessedResult {
//   ContainerType type = ContainerType::Undefined;
//   // total length incl header
//   int total_len = 0;
//   // processed bytes incl header of last step
//   int processed = 0;
//   // still (total) open
//   int open = 0;
// };

/// @brief  Calculates the checksum
static uint8_t checkSum(const uint8_t *data, size_t len) {
  uint8_t result = 0;
  for (int j = 0; j < len; j++) {
    result ^= data[j];
  }
  return result;
}
/// @brief Error types
enum BinaryContainerEncoderError { InvalidHeader, InvalidChecksum, DataMissing};

/**
 * @brief Wraps the encoded data into Config, Data, and Meta segments so that we
 * can recover the audio configuration and orignial segments if this is
 * relevant. We assume that a full segment is written with each call of write();
 * The segments are separated with a new line character.
 * @ingroup codecs
 * @ingroup encoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class BinaryContainerEncoder : public AudioEncoder {
public:
  BinaryContainerEncoder() = default;
  BinaryContainerEncoder(AudioEncoder &encoder) { p_codec = &encoder; }
  BinaryContainerEncoder(AudioEncoder *encoder) { p_codec = encoder; }

  void setEncoder(AudioEncoder *encoder) { p_codec = encoder; }

  void setOutput(Print &outStream) {
    LOGD("BinaryContainerEncoder::setOutput");
    p_out = &outStream;
  }

  bool begin() override {
    TRACED();
    // target.begin();
    bool rc = p_codec->begin();
    p_codec->setAudioInfo(cfg.info);
    is_beginning = true;
    is_video_beginning = true;
    return rc;
  }

  void setAudioInfo(AudioInfo info) override {
    TRACED();
    if (info != audioInfo()) {
      cfg.info = info;
    }
  }

  AudioInfo audioInfo() override { return cfg.info; }

  /// Defines the video track configuration (width/height/format/...) - the
  /// VideoHeader segment is (re-)sent before the next writeVideo() call.
  void setVideoInfo(VideoInfo info) {
    TRACED();
    video_cfg.info = info;
    is_video_beginning = true;
  }

  VideoInfo videoInfo() { return video_cfg.info; }

  /// Adds meta data segment
  size_t writeMeta(const uint8_t *data, size_t len) {
    LOGD("BinaryContainerEncoder::writeMeta: %d", (int)len);
    meta.common.len = len + sizeof(SimpleContainerMetaDataHeader);
    uint8_t tmp_array[meta.common.len];
    memcpy(tmp_array, &meta, sizeof(meta));
    memcpy(tmp_array + sizeof(meta), data, len);
    output(tmp_array, meta.common.len);
    return len;
  }

  /// Add data segment. On first write we also add a AudioInfo header
  size_t write(const uint8_t *data, size_t len) {
    LOGD("BinaryContainerEncoder::write: %d", (int)len);
    if (is_beginning) {
      writeHeader();
      is_beginning = false;
    }
    writeAudio((uint8_t *)data, len);
    return len;
  }

  /// Adds one already-encoded video/image frame (e.g. one H.264 access
  /// unit or one complete JPEG image) as a Video segment - a single call
  /// must provide the full frame. On the first call (or after
  /// setVideoInfo()) a VideoHeader segment is emitted first.
  size_t writeVideo(const uint8_t *data, size_t len) {
    LOGD("BinaryContainerEncoder::writeVideo: %d", (int)len);
    if (is_video_beginning) {
      writeVideoHeader();
      is_video_beginning = false;
    }
    video_dh.common.len = len + sizeof(CommonHeader);
    video_dh.common.checksum = checkSum(data, len);
    output((uint8_t *)&video_dh, sizeof(video_dh));
    output(data, len);
    return len;
  }

  void end() { p_codec->end(); }

  operator bool() { return true; };

  virtual const char *mime() { return "audio/binary"; };

protected:
  uint64_t packet_count = 0;
  bool is_beginning = true;
  bool is_video_beginning = true;
  int repeat_header;
  SimpleContainerConfig cfg;
  SimpleContainerDataHeader dh;
  SimpleContainerMetaDataHeader meta;
  SimpleContainerVideoConfig video_cfg;
  SimpleContainerVideoDataHeader video_dh;
  AudioEncoder *p_codec = nullptr;
  Print *p_out = nullptr;

  void writeAudio(const uint8_t *data, size_t len) {
    LOGD("writeAudio: %d", (int)len);
    // encode data
    SingleBuffer<uint8_t> tmp_buffer{(int)len};
    QueueStream<uint8_t> tmp{tmp_buffer};
    tmp.begin();
    p_codec->setOutput(tmp);
    p_codec->write(data, len);

    // output of audio data header
    dh.common.len = tmp.available() + sizeof(CommonHeader);
    dh.common.checksum = checkSum(tmp_buffer.data(), tmp_buffer.available());
    output((uint8_t *)&dh, sizeof(dh));

    // output of data
    output(tmp_buffer.data(), tmp_buffer.available());
  }

  void writeHeader() {
    LOGD("writeHeader");
    output((uint8_t *)&cfg, sizeof(cfg));
  }

  void writeVideoHeader() {
    LOGD("writeVideoHeader");
    output((uint8_t *)&video_cfg, sizeof(video_cfg));
  }

  size_t output(const uint8_t *data, size_t len) {
    if (p_out != nullptr) {
      int written = p_out->write((uint8_t *)data, len);
      LOGD("output: %d -> %d", (int)len, written);
    } else
      LOGW("output not defined");
    return len;
  }
};

/**
 * @brief Decodes the provided data from the DAT and CFG segments
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class BinaryContainerDecoder : public ContainerDecoder {
public:
  BinaryContainerDecoder() = default;
  BinaryContainerDecoder(AudioDecoder &decoder) { p_codec = &decoder; }
  BinaryContainerDecoder(AudioDecoder *decoder) { p_codec = decoder; }

  void setDecoder(AudioDecoder *decoder){
    p_codec = decoder;
  }

  // Defines the output: this method is called 2 times: first to define
  // output defined in the EnocdedAudioStream and then to define the
  // real output in the output chain.
  void setOutput(Print &outStream) {
    LOGD("BinaryContainerDecoder::setOutput")
    p_out = &outStream;
  }

  void setMetaCallback(void (*callback)(uint8_t*, int, void*)) {
    meta_callback = callback;
  }

  /// Defines the video output - e.g. a VideoOutput implementation
  /// (H264Decoder, JPEGOutputTFT, ...), or any other Print if you want the raw
  /// frame payload as-is.
  void setOutputVideo(Print &out) {
    LOGD("BinaryContainerDecoder::setOutputVideo");
    p_out_video = &out;
  }

  /// Registers a callback invoked once a VideoHeader segment has been
  /// parsed, providing the track's VideoInfo (width/height/format/...).
  void setVideoInfoCallback(void (*callback)(VideoInfo info, void* ref)) {
    video_info_callback = callback;
  }

  /// Common video info (width/height/format/...) parsed from the most
  /// recent VideoHeader segment - VideoInfo::operator bool() is false if
  /// none has been received yet.
  VideoInfo videoInfo() { return video_info; }

  bool begin() {
    TRACED();
    is_first = true;
    return true;
  }

  void end() { TRACED(); }

  size_t write(const uint8_t *data, size_t len) {
    LOGD("write: %d", (int)len);
    uint8_t *data8 = (uint8_t *)data;
    if (buffer.size() < len) {
      buffer.resize(
          std::max(static_cast<int>(DEFAULT_BUFFER_SIZE + header_size),
                   static_cast<int>(len * 4 + header_size)));
    }

    size_t result = buffer.writeArray(data8, len);
    while (parseBuffer())
      ;
    return ignore_write_errors ? len : result;
  }

  operator bool() { return true; };

  void addErrorHandler(void (*error_handler)(BinaryContainerEncoderError error, BinaryContainerDecoder* source, void* ref)){
    this->error_handler = error_handler;
  }

  /// If set to true we do not expect a retry to write the missing data but continue just with the next. (Default is true);
  void setIgnoreWriteErrors(bool flag){
    ignore_write_errors = flag;
  }

  /// Provide additional information for callback
  void setReference(void* ref){
    reference = ref;
  }

protected:
  bool is_first = true;
  CommonHeader header;
  const size_t header_size = sizeof(header);
  AudioDecoder *p_codec = nullptr;
  SingleBuffer<uint8_t> buffer{0};
  Print *p_out = nullptr;
  Print *p_out_video = nullptr;
  VideoInfo video_info;
  void (*meta_callback)(uint8_t* data, int len, void* ref) = nullptr;
  void (*video_info_callback)(VideoInfo info, void* ref) = nullptr;
  void (*error_handler)(BinaryContainerEncoderError error, BinaryContainerDecoder* source, void* ref) = nullptr;
  bool ignore_write_errors = true;
  void * reference = nullptr;


  bool parseBuffer() {
    LOGD("parseBuffer");
    bool result = false;

    StrView str{(const char *)buffer.data()};
    int start = str.indexOf("\r\n");
    LOGD("start: %d", start);
    if (start < 0) {
      return false;
    }
    // get next record
    if (buffer.available() - start > sizeof(header)) {
      // determine header
      memmove((uint8_t *)&header, buffer.data() + start, sizeof(header));

      // check header
      if (!isValidHeader()) {
        LOGW("invalid header: %d", header.type);
        if (error_handler) error_handler(InvalidHeader, this, reference);
        nextRecord();
        return false;
      };

      if (buffer.available() - start >= header.len) {
        // move to start of frame
        buffer.clearArray(start);
        // process frame
        result = processData();
      } else {
        LOGD("not enough data - available %d / req: %d", buffer.available(),
             header.len);
        if (error_handler) error_handler(DataMissing, this, reference);

      }
    } else {
      LOGD("not enough data for header: %d", buffer.available());
      if (error_handler) error_handler(DataMissing, this, reference);
    }
    return result;
  }

  // processes the completed data from the buffer: e.g. writes it
  bool processData() {
    LOGD("processData");
    bool rc = false;
    switch (header.type) {
    case ContainerType::Header: {
      LOGD("Header");
      SimpleContainerConfig config;
      buffer.readArray((uint8_t *)&config, sizeof(config));
      info = config.info;
      notifyAudioChange(info);
      info.logInfo();
      p_codec->setAudioInfo(info);
      p_codec->begin();
      rc = true;
    } break;

    case ContainerType::Audio: {
      LOGD("Audio");
      buffer.clearArray(sizeof(header));
      int data_len = header.len - header_size;
      uint8_t crc = checkSum(buffer.data(), data_len);
      if (header.checksum == crc) {
        // decode
        SingleBuffer<uint8_t> tmp_buffer{data_len * 5};
        QueueStream<uint8_t> tmp{tmp_buffer};
        tmp.begin();
        p_codec->setOutput(tmp);
        p_codec->write(buffer.data(), data_len);

        // output decoded data
        output(tmp_buffer.data(), tmp_buffer.available());
        buffer.clearArray(data_len);
      } else {
        LOGW("invalid checksum");
        if (error_handler) error_handler(InvalidChecksum, this, reference);
        // move to next record
        nextRecord();
        return false;
      }
      rc = true;
    } break;

    case ContainerType::Meta: {
      LOGD("Meta");
      buffer.clearArray(sizeof(header));
      int data_len = header.len - header_size;
      if (meta_callback != nullptr) {
        meta_callback(buffer.data(), data_len, reference);
      }
      buffer.clearArray(data_len);
      rc = true;
    } break;

    case ContainerType::VideoHeader: {
      LOGD("VideoHeader");
      SimpleContainerVideoConfig config;
      buffer.readArray((uint8_t *)&config, sizeof(config));
      video_info = config.info;
      if (video_info_callback) video_info_callback(video_info, reference);
      video_info.logInfo();
      rc = true;
    } break;

    case ContainerType::Video: {
      LOGD("Video");
      buffer.clearArray(sizeof(header));
      int data_len = header.len - header_size;
      uint8_t crc = checkSum(buffer.data(), data_len);
      if (header.checksum == crc) {
        outputVideo(buffer.data(), data_len);
        buffer.clearArray(data_len);
      } else {
        LOGW("invalid video checksum");
        if (error_handler) error_handler(InvalidChecksum, this, reference);
        // move to next record
        nextRecord();
        return false;
      }
      rc = true;
    } break;
    }
    return rc;
  }

  bool isValidHeader() {
    switch (header.type) {
    case ContainerType::Header:
      return header.checksum == 0;
    case ContainerType::Audio:
      return true;
    case ContainerType::Meta:
      return header.checksum == 0;
    case ContainerType::VideoHeader:
      return header.checksum == 0;
    case ContainerType::Video:
      return true;
    }
    return false;
  }

  uint8_t peekBufferValue(){
    uint8_t byte_value=0;
    buffer.peek(byte_value);
    return byte_value;
  }

  void nextRecord() {
    TRACED();
    uint8_t byte_value;
    while (buffer.available() && peekBufferValue() != '\n')
      buffer.read(byte_value);
  }

  // writes the data to the decoder which forwards it to the output; if there
  // is no coded we write to the output instead
  size_t output(uint8_t *data, size_t len) {
    LOGD("output: %d", (int)len);
    if (p_out != nullptr)
      p_out->write((uint8_t *)data, len);
    else
      LOGW("output not defined");

    return len;
  }

  // writes a decoded video frame to setOutputVideo()'s target - no codec
  // is involved (video frames are already-encoded, see writeVideo())
  size_t outputVideo(uint8_t *data, size_t len) {
    LOGD("outputVideo: %d", (int)len);
    if (p_out_video != nullptr)
      p_out_video->write((uint8_t *)data, len);
    else
      LOGW("output video not defined");

    return len;
  }
};

} // namespace audio_tools