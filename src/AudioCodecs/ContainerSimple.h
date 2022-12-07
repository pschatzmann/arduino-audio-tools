/**
 * @file CodecSimpleContainer.h
 * @author Phil Schatzmann
 * @brief  A simple container format which provides CFG records with audio info
 * and DAT records with the audio information. This can be used together with a
 * codec which does not transmit the audio information.
 *
 * @version 0.1
 * @date 2022-05-04
 *
 * @copyright Copyright (c) 2022
 *
 */
#pragma once
#include "AudioCodecs/AudioEncoded.h"
#include "Stream.h"

namespace audio_tools {

struct SimpleContainerConfig {
  char header[4] = "CFG";
  AudioBaseInfo info;
};

struct SimpleContainerDataHeader {
  char header[4] = "DAT";
};

/**
 * @brief Wraps the encoded data into CFG and DAT segments so that we can recover the audio configuration and orignial segments if this is relevant
 * @ingroup codecs
 */
class SimpleContainerEncoder : public AudioEncoder {
 public:
  SimpleContainerEncoder(AudioEncoder &encoder, int repeatHeader = 0) {
    p_codec = &encoder;
    repeat_header = repeatHeader;
  }

  void setOutputStream(Print &outStream) { p_codec->setOutputStream(outStream);}

  void begin(AudioBaseInfo info) {
    TRACED();
    setAudioInfo(info);
    p_codec->begin();
  }

  void begin() {
    p_codec->begin();
  }

  void setAudioInfo(AudioBaseInfo info){
    p_codec->setAudioInfo(info);
    cfg.info = info;
  }

  size_t write(const void *data, size_t len) {
    if (is_beginning) {
      if (packet_count == 0) {
        writeHeader();
      }
      // output of data header
      p_codec->write((uint8_t*)&dh, sizeof(dh));

      // trigger output of info header
      packet_count++;
      if (packet_count >= repeat_header) {
        packet_count = 0;
      }
    }
    // output of data
    p_codec->write((uint8_t*)data, len);
    return len;
  }

  void end() {
    flush();
    p_codec->end();
  }

  /// call to mark end of data packat to start the next one
  void commit() {
    is_beginning = true;
  }

  void flush() { commit(); }

 protected:
  uint64_t packet_count = 0;
  bool is_beginning = true;
  int repeat_header;
  AudioEncoder* p_codec = nullptr;
  SimpleContainerConfig cfg;
  SimpleContainerDataHeader dh;

  void writeHeader() { p_codec->write((uint8_t*) &cfg, sizeof(cfg)); }
};

/**
 * @brief Decodes the provided data from the DAT and CFG segments
 * @ingroup codecs
 */
class SimpleContainerDecoder : public AudioDecoder {
 public:
  SimpleContainerDecoder(AudioDecoder &decoder) { p_codec = &decoder; }

  void setOutputStream(Print &outStream) { p_codec->setOutputStream(outStream);}

  void setNotifyAudioChange(AudioBaseInfoDependent &bi) { p_inform = &bi; }

  void begin() {
    p_codec->begin();
  }

  void end() {
    p_codec->end();
  }

  size_t write(const void *data, size_t len) {
    parsed.clear();
    // parse data and collect relevant positions
    char *pt8 = (char *)data;
    for (int j = 0; j < len - 3; j++) {
      if (strncmp("CFG", pt8 + j, 3)==0) {
        parsed.push_back(pt8 + j);
      } else if (strncmp("DAT", pt8 + j, 3)==0) {
        parsed.push_back(pt8 + j);
      }
    }
    // add end
    parsed.push_back(pt8 + len);

    // process data
    if (parsed.size() > 1) {
      for (int j = 0; j < parsed.size()-1; j++) {
        char *start = parsed[j];
        char *end = parsed[j + 1];
        if (strncmp(start, "CFG", 3)==0) {
          memmove((void*)&cfg,(void*) start, sizeof(cfg));
          if (p_inform != nullptr) {
            p_inform->setAudioInfo(cfg.info);
          }
        } else if (strncmp (start, "DAT", 3)==0) {
          int len = end - start;
          p_codec->write(start + 3, len - 3);
        }
      }
    } else {
      // we have only an end entry -> write all data
      p_codec->write(data, len);
    }
    return len;
  }

  AudioBaseInfo audioInfo() { return cfg.info; }

 protected:
  SimpleContainerConfig cfg;
  AudioBaseInfoDependent *p_inform = nullptr;
  AudioDecoder* p_codec = nullptr;
  Vector<char *> parsed;
};

}  // namespace audio_tools