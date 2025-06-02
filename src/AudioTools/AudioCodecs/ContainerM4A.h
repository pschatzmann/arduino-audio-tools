#pragma once

#include "AudioTools/AudioCodecs/AudioCodecsBase.h"
#include "AudioTools/AudioCodecs/M4AAudioDemuxer.h"
#include "AudioTools/AudioCodecs/MultiDecoder.h"

namespace audio_tools {

/**
 * @brief M4A Demuxer that extracts audio from M4A/MP4 containers.
 * If you provide a decoder (in the constructor) the audio is decoded into pcm
 * format.
 * @ingroup codecs
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class ContainerM4A : public ContainerDecoder {
 public:
  ContainerM4A() = default;

  ContainerM4A(MultiDecoder& decoder) {
    p_decoder = &decoder;
    p_decoder->addNotifyAudioChange(*this);
  }

  void setOutput(AudioStream& out_stream) override {
    if (p_decoder != nullptr) p_decoder->setOutput(out_stream);
    ContainerDecoder::setOutput(out_stream);
  }

  bool isResultPCM() override { return p_decoder != nullptr ? true : false; }

  bool begin() override {
    demux.begin();
    demux.setReference(this);
    if (p_decoder) p_decoder->begin();
    is_active = true;
    return true;
  }

  void end() override {
    TRACED();
    is_active = false;
    is_magic_cookie_processed = false;
    if (p_decoder) p_decoder->end();
  }

  size_t write(const uint8_t* data, size_t len) override {
    demux.write(data, len);
    return len;
  }

  operator bool() override { return is_active; }

 protected:
  bool is_active = false;
  bool is_magic_cookie_processed = false;
  MultiDecoder* p_decoder = nullptr;
  M4AAudioDemuxer demux{decodeAudio};

  static void decodeAudio(const M4AAudioDemuxer::Frame& frame, void* ref) {
    ContainerM4A* self = static_cast<ContainerM4A*>(ref);
    MultiDecoder& dec = *self->p_decoder;
    // select decoder based on mime type
    dec.selectDecoder(frame.mime);

    // process magic cookie if not done yet
    if (!self->is_magic_cookie_processed) {
      auto& magic_cookie = self->demux.getAlacMagicCookie();
      if (magic_cookie.size() > 0) {
        dec.setCodecConfig(magic_cookie.data(), magic_cookie.size());
      }
      self->is_magic_cookie_processed = true;
    }
    // write encoded data to decoder
    dec.write(frame.data, frame.size);
  }
};

}  // namespace audio_tools