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
    codec = Codec::Unknown;
    alacMagicCookie.clear();
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
  Vector<uint8_t>& getALACMagicCookie() { return alacMagicCookie; }

  /**
   * @brief Sets a reference pointer for callbacks.
   * @param ref Reference pointer.
   */
  void setReference(void* ref) { this->ref = ref; }

  /**
   * @brief Resizes the internal buffer.
   * @param size New buffer size.
   */
  void resize(int size) {
    default_size = size;
    if (buffer.size() < size) {
      buffer.resize(size);
    }
  }

 protected:
  SingleBuffer<uint8_t> buffer;     ///< Buffer for incremental data.
  void* ref = nullptr;              ///< Reference pointer for callbacks.
  size_t default_size = 2 * 1024;   ///< Default buffer size.

  /**
   * @brief Setup all parser callbacks
   */
  virtual void setupParser() {
    // global box data callback to get sizes
    parser.setReference(this);
    parser.setCallback(boxDataSetupCallback);

    // incremental data callback
    parser.setIncrementalDataCallback(incrementalBoxDataCallback);

    // Register a specific incremental data callback for mdat
    parser.setIncrementalDataCallback(
        "mdat",
        [](MP4Parser::Box& box, const uint8_t* data, size_t len, bool is_final,
           void* ref) {
          auto* self = static_cast<M4AAudioDemuxer*>(ref);
          LOGI("*Box: %s, size: %u bytes", box.type, (unsigned)len);
          self->sampleExtractor.write(data, len, is_final);
        },
        false);

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
    parser.setCallback(
        "mdat",
        [](MP4Parser::Box& box, void* ref) {
          M4AAudioDemuxer& self = *static_cast<M4AAudioDemuxer*>(ref);
          // mdat must not be buffered
          LOGI("Box: %s, size: %u bytes", box.type, (unsigned)box.size);
          self.sampleExtractor.setMaxSize(box.size);
        },
        false);  // 'false' prevents the generic callback from being executed
  }

  /**
   * @brief Checks if a box type is relevant for audio demuxing.
   * @param type Box type string.
   * @return True if relevant, false otherwise.
   */
  static bool isRelevantBox(const char* type) {
    // Check if the box is relevant for audio demuxing
    return (StrView(type) == "stsd" || StrView(type) == "stsz" ||
            StrView(type) == "stco");
  }

  /**
   * @brief Callback for box data setup. If we contain data we add
   * it to the buffer. If there is no data we set up the buffer to
   * receive incremental data.
   * @param box MP4 box.
   * @param ref Reference pointer.
   */
  static void boxDataSetupCallback(MP4Parser::Box& box, void* ref) {
    M4AAudioDemuxer& self = *static_cast<M4AAudioDemuxer*>(ref);

    bool is_relevant = isRelevantBox(box.type);
    if (is_relevant) {
      LOGI("Box: %s, size: %u bytes", box.type, (unsigned)box.size);
      if (box.data_size == 0) {
        // setup for incremental processing
        self.resize(box.size);
        self.buffer.clear();
      } else {
        // we have the complete box data
        self.processBox(box);
      }
    }
  }

  /**
   * @brief Callback for incremental box data.
   * @param box MP4 box.
   * @param data Pointer to data.
   * @param len Length of data.
   * @param is_final True if this is the last chunk.
   * @param ref Reference pointer.
   */
  static void incrementalBoxDataCallback(MP4Parser::Box& box,
                                         const uint8_t* data, size_t len,
                                         bool is_final, void* ref) {
    M4AAudioDemuxer& self = *static_cast<M4AAudioDemuxer*>(ref);

    // mdat is now handled by its specific incremental callback, so remove logic
    // here

    // only process relevant boxes
    if (!isRelevantBox(box.type)) return;

    LOGI("*Box: %s, size: %u bytes", box.type, (unsigned)len);

    // others fill buffer incrementally
    if (len > 0) {
      size_t written = self.buffer.writeArray(data, len);
      if (written != len) {
        LOGE("Failed to write all data to buffer, written: %zu, expected: %zu",
             written, len);
      }
    }

    // on last chunk, call the specific box handler
    if (is_final) {
      MP4Parser::Box complete_box = box;
      complete_box.data = self.buffer.data();
      complete_box.data_size = self.buffer.size();
      self.processBox(complete_box);
      // The buffer might be quite large, so we resize it to the default size
      self.resize(self.default_size);
    }
  }

  /**
   * @brief Processes a parsed MP4 box.
   * @param box MP4 box.
   */
  void processBox(MP4Parser::Box& box) {
    if (StrView(box.type) == "stsd") {
      onStsd(box);
    } else if (StrView(box.type) == "stsz") {
      onStsz(box);
    } else if (StrView(box.type) == "stco") {
      // onStco(box); // currently not supported
    }
  }

};

}  // namespace audio_tools