/**
 * @file ContainerBinary.h
 * @author Phil Schatzmann
 * @brief  A lean and efficient container format which provides Header records
 * with audio info, Audio records with the audio and Meta which
 * can contain any additional information. This can be used together with a
 * codec which does not transmit the audio information or has variable frame
 * lengths. We expect that a single write() is providing full frames.
 *
 * @version 0.1
 * @date 2022-05-04
 *
 * @copyright Copyright (c) 2022
 *
 */
#pragma once
#include <string.h>

#include "AudioCodecs/AudioEncoded.h"

namespace audio_tools {

enum class ContainerType : uint8_t { Header = 1, Audio = 2, Meta = 3, Undefined = 0 };

struct CommonHeader {
  CommonHeader() = default;
  CommonHeader(ContainerType type, uint16_t len) {
    this->type = type;
    this->len = len;
  }
  char header = '\n';
  ContainerType type;
  uint16_t len;
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

struct ProcessedResult {
  ContainerType type = ContainerType::Undefined;
  // total length incl header
  int total_len = 0;
  // processed bytes incl header of last step
  int processed = 0;
  // still (total) open
  int open = 0;
};

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

  void setOutput(Print &outStream) {
    LOGD("BinaryContainerEncoder::setOutput: %d",is_initial_output);
    if (is_initial_output) {
      setupIntialOutputStream(outStream);
    } else {
      p_final_print = &outStream;
    }
  }

  void begin(AudioInfo info) {
    setAudioInfo(info);
    begin();
  }

  void begin() override {
    TRACED();
    target.begin();
    is_beginning = true;
  }

  void setAudioInfo(AudioInfo info) override {
    TRACED();
    if (info != audioInfo()) {
      target.setAudioInfo(info);
      cfg.info = info;
    }
  }

  AudioInfo audioInfo() { return cfg.info; }

  /// Adds meta data segment
  size_t writeMeta(const uint8_t *data, size_t len) {
    LOGD("BinaryContainerEncoder::writeMeta: %d", (int)len);
    meta.common.len = len;
    uint8_t tmp_array[sizeof(meta)+len];
    //output((uint8_t *)&meta, sizeof(meta));
    //output((uint8_t *)&data, len);
    // output in one write
    memcpy(tmp_array, &meta, sizeof(meta));
    memcpy(tmp_array+sizeof(meta), data, len);
    output(tmp_array, sizeof(tmp_array));
    return len;
  }

  /// Add data segment. On first write we also add a AudioInfo header
  size_t write(const void *data, size_t len) {
    LOGD("BinaryContainerEncoder::write: %d", (int)len);
    if (is_beginning) {
      writeHeader();
      is_beginning = false;
    }
    writeAudio((uint8_t *)data, len);
    return len;
  }

  void end() {
    target.end();
    is_initial_output = true;
  }

  operator bool() { return true; };

  virtual const char *mime() { return "audio/binary"; };

 protected:
  uint64_t packet_count = 0;
  bool is_beginning = true;
  int repeat_header;
  SimpleContainerConfig cfg;
  SimpleContainerDataHeader dh;
  SimpleContainerMetaDataHeader meta;
  AudioEncoder *p_codec = nullptr;
  AudioWriter *p_print1 = nullptr;
  AudioWriter *p_print2 = nullptr;
  ContainerTargetPrint target;
  bool is_initial_output = true;
  Print *p_final_print = nullptr;

  void setupIntialOutputStream(Print &outStream) {
    p_print1 = p_codec;
    p_print2 = this;
    if (p_codec!=nullptr)
      target.setupOutput(p_print1, p_print2, outStream);
    else
      target.setupOutput(p_print2, outStream);

    is_initial_output = false;
  }

  void writeAudio(const uint8_t *data, size_t len) {
    TRACED();
    // output of audio data header
    dh.common.len = len;
    output((uint8_t *)&dh, sizeof(dh));

    // output of data
    output(data, len);
  }

  void writeHeader() {
    TRACED();
    output((uint8_t *)&cfg, sizeof(cfg));
  }

  size_t output(const uint8_t *data, size_t len) {
    TRACED();
    if (p_final_print != nullptr)
      p_final_print->write((uint8_t *)data, len);
    else
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
class BinaryContainerDecoder : public AudioDecoder {
 public:
  BinaryContainerDecoder() = default;
  BinaryContainerDecoder(AudioDecoder &decoder) { p_codec = &decoder; }
  BinaryContainerDecoder(AudioDecoder *decoder) { p_codec = decoder; }

  // Defines the output: this method is called 2 times: first to define
  // output defined in the EnocdedAudioStream and then to define the
  // real output in the output chain.
  void setOutput(Print &outStream) {
    LOGD("BinaryContainerDecoder::setOutput: %d",is_initial_output);
    if (is_initial_output) {
      setupIntialOutputStream(outStream);
    } else {
      p_final_print = &outStream;
    }
  }

  void setMetaCallback(void (*callback)(uint8_t *, int)) {
    meta_callback = callback;
  }

  void setNotifyAudioChange(AudioInfoSupport &bi) {}

  void begin() {
    is_first = true;
    target.begin();
  }

  void end() {
    target.end();
    is_initial_output = true;
  }

  size_t write(const void *data, size_t len) {
    uint8_t *data8 = (uint8_t *)data;
    int open = len;
    int processed = 0;

    // on first call we try to synchronize the delimiter; hopefully however
    // the new line is on the first char.
    if (is_first) {
      is_first = false;
      // ignore the data before the first newline
      uint8_t *ptr = (uint8_t *)memchr(data8, '\n', len);
      if (ptr != nullptr) {
        processed = ptr - data8;
        open -= processed;
      }
    } else {
      // process remaining data from last run
      while (result.open > 0) {
        result = processOpen(result, data8 + processed, open);
        open -= result.processed;
        processed += result.processed;
      }
    }

    // process new data startubg with newline
    while (open > 0) {
      result = processData(data8 + processed, open);
      open -= result.processed;
      processed += result.processed;
    }
    return len;
  }

  AudioInfo audioInfo() { return info; }

  operator bool() { return true; };

 protected:
  bool is_first = true;
  ProcessedResult result;
  AudioInfo info;
  CommonHeader header;
  const size_t header_size = sizeof(header);
  AudioDecoder *p_codec = nullptr;
  void (*meta_callback)(uint8_t *, int) = nullptr;
  SingleBuffer<uint8_t> frame{0};
  AudioWriter *p_print1 = nullptr;
  AudioWriter *p_print2 = nullptr;
  Print *p_out = nullptr;
  ContainerTargetPrint target;
  bool is_initial_output = true;
  Print *p_final_print = nullptr;

  void setupIntialOutputStream(Print &outStream) {
    p_out = &outStream;
    if (p_codec != nullptr) {
      p_print1 = p_codec;
      p_print2 = this;
      target.setupOutput(p_print1, p_print2, outStream);
    } else {
      p_print1 = this;
      target.setupOutput(p_print1, outStream);
    }
    is_initial_output = false;
  }

  // loads the data into the buffer and writes it if complete
  ProcessedResult processData(uint8_t *data8, size_t len) {
    TRACED();
    ProcessedResult result = loadData(data8, len);
    writeData(result);
    return result;
  }

  // loads the data
  ProcessedResult loadData(uint8_t *data8, size_t len) {
    TRACED();
    ProcessedResult result;
    if (data8[0] == '\n') {
      memcpy(&header, data8, header_size);
      result.total_len = header_size + header.len;
      LOGD("header.len: %d, result.total_len: %d, len: %d", (int)header.len,
           (int)result.total_len, (int)len);
      if (result.total_len <= len) {
        result.processed = result.total_len;
        result.open = 0;
      } else {
        result.processed = len;
        result.open = result.total_len - len;
      }
      result.type = checkType(header.type);
      if (result.type != ContainerType::Undefined) {
        LOGD("header.len: %d", header.len);
        if (frame.size() < header.len) {
          frame.resize(header.len);
        }
        if (result.processed - header_size > 0)
          frame.writeArray(data8 + header_size, result.processed - header_size);
      }
    } else {
      LOGW("data ignored");
      result.type = ContainerType::Undefined;
    }
    return result;
  }

  // processes the completed data from frame buffer: e.g. writes it to the
  // output
  bool writeData(ProcessedResult result) {
    bool rc = false;
    if (result.open == 0 && frame.available() > 0) {
      TRACED();
      switch (result.type) {
        case ContainerType::Header: {
          LOGD("Header");
          // We expect that the header is never split because it is at the
          // start
          SimpleContainerConfig config;
          assert(result.open == 0);
          memmove(&config, frame.data(), sizeof(config));
          info = config.info;
          if (p_notify) {
            p_notify->setAudioInfo(info);
          }
          info.logInfo();
          frame.clear();
          rc = true;
        } break;

        case ContainerType::Audio: {
          LOGD("Audio");
          output(frame.data(), frame.available());
          frame.clear();
          rc = true;
        } break;

        case ContainerType::Meta: {
          LOGD("Meta");
          if (meta_callback!=nullptr) {
            meta_callback(frame.data(), frame.available());
          }
          frame.clear();
          rc = true;
        } break;
      }
    }
    return rc;
  }

  // processes the reaminder of a split segment
  ProcessedResult processOpen(ProcessedResult in, uint8_t *data8, size_t len) {
    TRACED();
    ProcessedResult result = loadOpen(in, data8, len);
    writeData(result);
    return result;
  }

  // if a segment is split we process the remaining missing part
  ProcessedResult loadOpen(ProcessedResult in, uint8_t *data8, size_t len) {
    TRACED();
    ProcessedResult result = in;
    int to_process;
    if (in.open <= len) {
      result.open = 0;
      result.processed = in.open;
    } else {
      result.open = in.open - len;
      result.processed = len;
    }
    LOGD("in.type: %d, len: %d", in.type, result.processed);
    if (in.type != ContainerType::Undefined) {
      frame.writeArray(data8, result.processed);
    } else {
      LOGW("Unsupported tye");
    }
    return result;
  }

  // checks the type
  ContainerType checkType(ContainerType type) {
    ContainerType result = ContainerType::Undefined;
    switch (header.type) {
      case ContainerType::Header:
        result = ContainerType::Header;
        break;
      case ContainerType::Audio:
        result = ContainerType::Audio;
        break;
      case ContainerType::Meta:
        result = ContainerType::Meta;
        break;
    }
    return result;
  }

  // writes the data to the decoder which forwards it to the output; if there
  // is no coded we write to the output instead
  size_t output(uint8_t *data, size_t len) {
    LOGD("BinaryContainerDecoder::output: %d", (int)len);
    if (p_final_print != nullptr)
      p_final_print->write((uint8_t *)data, len);
    else
      LOGW("output not defined");

    return len;
  }
};

}  // namespace audio_tools