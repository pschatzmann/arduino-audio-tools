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
  /**
   * @brief Default constructor. Sets up the demuxer callback.
   */
  ContainerM4A() {
    demux.setReference(this);
    demux.setCallback(decodeAudio);
  };

  /**
   * @brief Constructor with decoder. Sets up the demuxer and decoder notification.
   * @param decoder Reference to a MultiDecoder for PCM output.
   */
  ContainerM4A(MultiDecoder& decoder) : ContainerM4A() {
    p_decoder = &decoder;
    p_decoder->addNotifyAudioChange(*this);
  }

  /**
   * @brief Set the output stream for decoded or raw audio.
   * @param out_stream Output AudioStream.
   */
  void setOutput(AudioStream& out_stream) override {
    if (p_decoder != nullptr) p_decoder->setOutput(out_stream);
    ContainerDecoder::setOutput(out_stream);
  }

  /**
   * @brief Returns true if the result is PCM (decoder is present).
   * @return true if PCM output, false otherwise.
   */
  bool isResultPCM() override { return p_decoder != nullptr ? true : false; }

  /**
   * @brief Initialize the demuxer and decoder.
   * @return true on success.
   */
  bool begin() override {
    demux.begin();
    if (p_decoder) p_decoder->begin();
    is_active = true;
    return true;
  }

  /**
   * @brief End the demuxer and decoder, releasing resources.
   */
  void end() override {
    TRACED();
    is_active = false;
    is_magic_cookie_processed = false;
    if (p_decoder) p_decoder->end();
  }

  /**
   * @brief Feed data to the demuxer for parsing.
   * @param data Pointer to input data.
   * @param len Length of input data.
   * @return Number of bytes processed (always len).
   */
  size_t write(const uint8_t* data, size_t len) override {
    if (is_active == false) return len;
    demux.write(data, len);
    return len;
  }

  /**
   * @brief Returns true if the demuxer is active.
   * @return true if active, false otherwise.
   */
  operator bool() override { return is_active; }

 protected:
  bool is_active = false;                ///< True if demuxer is active.
  bool is_magic_cookie_processed = false;///< True if ALAC magic cookie has been processed.
  MultiDecoder* p_decoder = nullptr;     ///< Pointer to the MultiDecoder.
  M4AAudioDemuxer demux;                 ///< Internal demuxer instance.

  /**
   * @brief Static callback for demuxed audio frames.
   *        Handles decoder selection and magic cookie for ALAC.
   * @param frame The demuxed audio frame.
   * @param ref Reference to the ContainerM4A instance.
   */
  static void decodeAudio(const M4AAudioDemuxer::Frame& frame, void* ref) {
    ContainerM4A* self = static_cast<ContainerM4A*>(ref);
    if (self->p_decoder == nullptr) {
      LOGE("No decoder defined, cannot decode audio frame: %s (%u bytes)", frame.mime, (unsigned) frame.size);
      return;
    }
    MultiDecoder& dec = *(self->p_decoder);
    // select decoder based on mime type
    if (!dec.selectDecoder(frame.mime)) {
      const char* mime = frame.mime ? frame.mime : "(nullptr)";
      LOGE("No decoder found for mime type: %s", mime);
      return;
    }

    // for ALAC only: process magic cookie if not done yet
    if (StrView(frame.mime) == "audio/alac" &&
        !self->is_magic_cookie_processed) {
      auto& magic_cookie = self->demux.getALACMagicCookie();
      if (magic_cookie.size() > 0) {
        if (!dec.setCodecConfig(magic_cookie.data(), magic_cookie.size())){
          LOGE("Failed to set ALAC magic cookie for decoder: %s", dec.selectedMime());
        }
      }
      self->is_magic_cookie_processed = true;
    }
    // write encoded data to decoder
    dec.write(frame.data, frame.size);
  }
};

}  // namespace audio_tools