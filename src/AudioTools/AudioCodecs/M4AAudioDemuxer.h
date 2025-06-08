#pragma once

#include "AudioTools/AudioCodecs/M4ACommonDemuxer.h"

namespace audio_tools {

/**
 * @brief A simple M4A audio data demuxer which is providing
 * AAC, MP3 and ALAC frames.
 */
class M4AAudioDemuxer : public M4ACommonDemuxer {
 public:
  /**
   * @brief Constructor. Sets up parser callbacks.
   */
  M4AAudioDemuxer() { setupParser(); }

  /**
   * @brief Defines the callback that returns the audio frames.
   * @param cb Frame callback function.
   */
  void setCallback(FrameCallback cb) override {
    sampleExtractor.setReference(ref);
    sampleExtractor.setCallback(cb);
  }

  /**
   * @brief Initializes the demuxer and resets state.
   */
  bool begin() {
    audio_config.codec = Codec::Unknown;
    audio_config.alacMagicCookie.clear();
    resize(default_size);

    stsz_processed = false;
    stco_processed = false;

    // When codec/sampleSizes/callback/ref change, update the extractor:
    parser.begin();
    sampleExtractor.begin();
    return true;
  }

  /**
   * @brief Writes data to the demuxer for parsing.
   * @param data Pointer to input data.
   * @param len Length of input data.
   */
  void write(const uint8_t* data, size_t len) { parser.write(data, len); }

  /**
   * @brief Returns the available space for writing.
   * @return Number of bytes available for writing.
   */
  int availableForWrite() { return parser.availableForWrite(); }

  /**
   * @brief Returns the ALAC magic cookie (codec config).
   * @return Reference to the ALAC magic cookie vector.
   */
  Vector<uint8_t>& getALACMagicCookie() { return audio_config.alacMagicCookie; }

  /**
   * @brief Sets a reference pointer for callbacks.
   * @param ref Reference pointer.
   */
  void setReference(void* ref) { this->ref = ref; }

  void copyFrom(M4ACommonDemuxer& source) {
    audio_config = source.getM4AAudioConfig();
  }

 protected:
  void* ref = nullptr;  ///< Reference pointer for callbacks.

  /**
   * @brief Setup all parser callbacks
   */
  void setupParser() override {
    // global box data callback to get sizes
    parser.setReference(this);

    // parsing for content of stsd (Sample Description Box)
    parser.setCallback("stsd", [](MP4Parser::Box& box, void* ref) {
      static_cast<M4AAudioDemuxer*>(ref)->onStsd(box);
    });

    // parsing for content of stsd (Sample Description Box)
    parser.setCallback("esds", [](MP4Parser::Box& box, void* ref) {
      static_cast<M4AAudioDemuxer*>(ref)->onEsds(box);
    });
    parser.setCallback("mp4a", [](MP4Parser::Box& box, void* ref) {
      static_cast<M4AAudioDemuxer*>(ref)->onMp4a(box);
    });
    parser.setCallback("alac", [](MP4Parser::Box& box, void* ref) {
      static_cast<M4AAudioDemuxer*>(ref)->onAlac(box);
    });

    // mdat
    parser.setCallback(
        "mdat",
        [](MP4Parser::Box& box, void* ref) {
          M4AAudioDemuxer& self = *static_cast<M4AAudioDemuxer*>(ref);
          // mdat must not be buffered
          LOGI("#%d Box: %s, size: %u of %u bytes", (unsigned) box.seq, box.type,(unsigned) box.available, (unsigned)box.size);
          if (box.seq == 0) self.sampleExtractor.setMaxSize(box.size);
          size_t written = self.sampleExtractor.write(box.data, box.available, box.is_complete);
          assert(written == box.available);
        },
        false);  // 'false' prevents the generic callback from being executed

    // stsz
    parser.setCallback(
        "stsz",
        [](MP4Parser::Box& box, void* ref) {
          M4AAudioDemuxer& self = *static_cast<M4AAudioDemuxer*>(ref);
          self.onStsz(box);
        },
        false);  // 'false' prevents the generic callback from being executed

    // parser.setCallback(
    //     "stco",
    //     [](MP4Parser::Box& box, void* ref) {
    //       M4AAudioDemuxer& self = *static_cast<M4AAudioDemuxer*>(ref);
    //       self.onStco(box);
    //     },
    //     false);  // 'false' prevents the generic callback from being executed
  }
};

}  // namespace audio_tools