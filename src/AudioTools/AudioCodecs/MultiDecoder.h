
#pragma once

#include "AudioTools/AudioCodecs/AudioCodecsBase.h"
#include "AudioTools/CoreAudio/AudioBasic/StrView.h"
#include "AudioTools/Communication/HTTP/AbstractURLStream.h"
#include "AudioTools/CoreAudio/AudioMetaData/MimeDetector.h"
#include "AudioTools/AudioCodecs/StreamingDecoder.h"

namespace audio_tools {

/**
 * @brief Manage multiple AudioDecoders with automatic format detection
 *
 * This class automatically detects the audio format from incoming data and
 * selects the appropriate decoder from a collection of registered decoders.
 * The format detection is performed using the MimeDetector on the first chunk
 * of data written to the decoder.
 *
 * Key features:
 * - Automatic format detection using MimeDetector
 * - Support for multiple decoder registration
 * - Custom MIME type detection logic support
 * - External MIME source integration (e.g., HTTP headers)
 * - Lazy decoder initialization for memory efficiency
 * - Seamless integration with existing AudioDecoder architecture
 *
 * The actual decoder is only opened when it has been selected, which allows
 * for memory-efficient operation when dealing with multiple possible formats.
 * The relevant decoder is determined dynamically at the first write() call
 * based on the determined MIME type.
 *
 * @note This class uses a write-based interface, unlike StreamingDecoder
 * which uses a pull-based approach. For streaming scenarios with direct
 * access to input/output streams, consider using MultiStreamingDecoder.
 *
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class MultiDecoder : public AudioDecoder {
 public:
  /**
   * @brief Default constructor
   */
  MultiDecoder() = default;
  
  /**
   * @brief Constructor with external MIME source
   *
   * Creates a MultiDecoder that uses an external source for MIME type
   * determination, such as HTTP Content-Type headers. This can be more
   * efficient than automatic detection as it avoids analyzing data content.
   *
   * @param mimeSource Reference to a MimeSource that provides MIME type information
   */
  MultiDecoder(MimeSource& mimeSource) { setMimeSource(mimeSource); }

 #ifdef USE_EXPERIMENTAL 
  /**
   * @brief Destructor
   *
   * Cleans up any internally created DecoderAdapter instances.
   */
  ~MultiDecoder() {
    // Clean up any adapters we created
    for (auto* adapter : adapters) {
      delete adapter;
    }
    adapters.clear();    
  }
#endif

  /**
   * @brief Starts the processing and enables automatic MIME type determination
   *
   * Initializes the MIME detector and prepares the MultiDecoder for format
   * detection. This method must be called before any write() operations.
   *
   * @return true if initialization was successful, false if no output is defined
   */
  bool begin() override {
    mime_detector.begin();
    is_first = true;
    if (p_print == nullptr) {
      LOGE("No output defined");
      return false;
    }
    return true;
  }

  /**
   * @brief Releases resources and closes the active decoder
   *
   * Stops the currently active decoder and resets the MultiDecoder state
   * for potential reuse. After calling end(), begin() must be called again
   * before the decoder can process new data.
   */
  void end() override {
    if (actual_decoder.decoder != nullptr && actual_decoder.is_open) {
      actual_decoder.decoder->end();
    }
    actual_decoder.is_open = false;
    actual_decoder.decoder = nullptr;
    actual_decoder.mime = nullptr;
    is_first = true;
  }

  /**
   * @brief Adds a decoder that will be selected by its MIME type
   *
   * Registers an AudioDecoder that will be automatically selected when
   * the corresponding MIME type is detected in the input data.
   *
   * @param decoder The AudioDecoder to register
   * @param mime The MIME type string to associate with this decoder
   */
  void addDecoder(AudioDecoder& decoder, const char* mime) {
    DecoderInfo info{mime, &decoder};
    decoder.addNotifyAudioChange(*this);
    decoders.push_back(info);
  }

  /**
   * @brief Adds a decoder with custom MIME detection logic
   *
   * Registers an AudioDecoder with a specific MIME type and provides custom
   * logic for detecting that MIME type from raw data. This allows for
   * specialized format detection beyond the standard MimeDetector capabilities.
   *
   * @param decoder The AudioDecoder to register
   * @param mime The MIME type string to associate with this decoder
   * @param check Custom function that analyzes data to detect this MIME type.
   *              Should return true if the data matches this format.
   */
  void addDecoder(AudioDecoder& decoder, const char* mime,
                  bool (*check)(uint8_t* data, size_t len)) {
    addDecoder(decoder, mime);
    mime_detector.setCheck(mime, check);
  }

  /**
   * @brief Sets the output stream for decoded audio data
   *
   * Defines where the decoded PCM audio data will be written to.
   * This output will be automatically configured for the selected decoder.
   *
   * @param out_stream The Print stream to write decoded audio data to
   */
  void setOutput(Print& out_stream) override {
    p_print = &out_stream;
  }

  /**
   * @brief Sets an external MIME source for format detection
   *
   * Provides an alternative to automatic MIME detection by allowing an external
   * source to provide the MIME type information. This is particularly useful
   * when the MIME type is available from HTTP headers or other metadata sources.
   *
   * When a MIME source is set, it takes precedence over automatic detection,
   * making the decoder selection process more efficient.
   *
   * @param mimeSource Reference to a MimeSource that provides MIME type information
   *
   * @note The MimeSource object must remain valid for the lifetime of this
   *       MultiDecoder instance, as only a reference is stored.
   */
  void setMimeSource(MimeSource& mimeSource) { p_mime_source = &mimeSource; }

  /**
   * @brief Selects the actual decoder by MIME type
   *
   * Searches through registered decoders to find one that matches the
   * specified MIME type, then initializes it for use. This method is
   * usually called automatically from the determined MIME type during
   * the first write() operation.
   *
   * @param mime The MIME type string to match against registered decoders
   * @return true if a matching decoder was found and initialized, false otherwise
   */
  bool selectDecoder(const char* mime) {
    bool result = false;
    if (mime == nullptr) return false;
    // do nothing if no change
    if (StrView(mime).equals(actual_decoder.mime)) {
      is_first = false;
      return true;
    }
    // close actual decoder 
    if (actual_decoder.decoder != this) end();

    // find the corresponding decoder
    selected_mime = nullptr;
    for (int j = 0; j < decoders.size(); j++) {
      DecoderInfo info = decoders[j];
      if (StrView(info.mime).equals(mime)) {
        LOGI("Using decoder for %s (%s)", info.mime, mime);
        actual_decoder = info;
        // define output if it has not been defined
        if (p_print != nullptr && actual_decoder.decoder != this 
          && actual_decoder.decoder->getOutput() == nullptr) {
          actual_decoder.decoder->setOutput(*p_print);
        }
        if (!*actual_decoder.decoder) {
          actual_decoder.decoder->begin();
          LOGI("Decoder %s started", actual_decoder.mime);
        }
        result = true;
        selected_mime = mime;
        break;
      }
    }
    is_first = false;
    return result;
  }

  /**
   * @brief Returns the MIME type that was detected and selected
   *
   * @return The MIME type string that was detected and used to select
   *         the current decoder, or nullptr if no decoder has been selected
   */
  const char* selectedMime() { return selected_mime; }

  /**
   * @brief Writes encoded audio data to be decoded
   *
   * On the first call, this method performs MIME type detection to select
   * the appropriate decoder. Subsequent calls delegate to the selected
   * decoder's write() method to process the audio data.
   *
   * The MIME detection process uses either an external MIME source (if set)
   * or analyzes the provided data to determine the audio format.
   *
   * @param data Buffer containing encoded audio data
   * @param len Number of bytes to write
   * @return Number of bytes actually written to the selected decoder
   */
  size_t write(const uint8_t* data, size_t len) override {
    if (is_first) {
      const char* mime = nullptr;
      if (p_mime_source != nullptr) {
        // get content type from http header
        mime = p_mime_source->mime();
        if (mime) LOGI("mime from http request: %s", mime);
      }
      if (mime == nullptr) {
        // use the mime detector
        mime_detector.write((uint8_t*)data, len);
        mime = mime_detector.mime();
        if (mime) LOGI("mime from mime_detector: %s", mime);
      }
      if (mime != nullptr) {
        // select the decoder based on the detemined mime type
        if (!selectDecoder(mime)) {
          LOGE("The decoder could not be found for %s", mime);
          actual_decoder.decoder = &nop;
          actual_decoder.is_open = true;
        }
      }
      is_first = false;
    }
    // check if we have a decoder
    if (actual_decoder.decoder == nullptr) return 0;
    // decode the data
    return actual_decoder.decoder->write(data, len);
  }

  /**
   * @brief Checks if the decoder is active and ready
   *
   * @return true if a decoder is selected and active, or if format detection
   *         hasn't been performed yet; false if no suitable decoder was found
   */
  virtual operator bool() override {
    if (actual_decoder.decoder == &nop) return false;
    return is_first || actual_decoder.is_open;
  };

  /**
   * @brief Sets codec-specific configuration data
   *
   * Forwards codec configuration data to the currently selected decoder.
   * This method can only be called after a decoder has been selected.
   *
   * @param data Buffer containing codec configuration data
   * @param len Length of the configuration data
   * @return true if the configuration was successfully applied, false otherwise
   */
  bool setCodecConfig(const uint8_t* data, size_t len) override {
    if (actual_decoder.decoder == nullptr) {
      LOGE("No decoder defined, cannot set codec config");
      return false;
    }
    return actual_decoder.decoder->setCodecConfig(data, len);
  }

  /**
   * @brief Provides access to the internal MIME detector
   *
   * Returns a reference to the MimeDetector instance used for automatic
   * format detection. This allows direct access to configure custom MIME
   * detection logic or to query detection results.
   *
   * @return Reference to the internal MimeDetector instance
   */
  MimeDetector& mimeDetector() { return mime_detector; }

#ifdef USE_EXPERIMENTAL 
 
  /**
   * @brief Adds a StreamingDecoder that will be selected by its MIME type
   *
   * Registers a StreamingDecoder that will be automatically selected when
   * the corresponding MIME type is detected in the input data. The 
   * StreamingDecoder is wrapped in a DecoderAdapter to provide compatibility
   * with the write-based AudioDecoder interface used by MultiDecoder.
   *
   * @param decoder The StreamingDecoder to register
   * @param mime The MIME type string to associate with this decoder
   * @param bufferSize Buffer size for the adapter (default: 1024 bytes)
   */
  void addDecoder(StreamingDecoder& decoder, const char* mime,
                  int bufferSize = 1024) {
    if (mime != nullptr) {
      // Create a DecoderAdapter to wrap the StreamingDecoder
      decoder.addNotifyAudioChange(*this);
      auto adapter = new DecoderAdapter(decoder, bufferSize);
      adapters.push_back(adapter);  // Store for cleanup
      
      // Add the adapter as a regular AudioDecoder
      addDecoder(*adapter, mime);
    } else {
      LOGE("MIME type is nullptr - cannot add StreamingDecoder");
    }
  }
#endif

 protected:
  /**
   * @brief Information about a registered decoder
   */
  struct DecoderInfo {
    const char* mime = nullptr;           ///< MIME type for this decoder
    AudioDecoder* decoder = nullptr;      ///< Pointer to the decoder instance
    bool is_open = false;                 ///< Whether the decoder is currently active
    
    /**
     * @brief Default constructor
     */
    DecoderInfo() = default;
    
    /**
     * @brief Constructor with parameters
     *
     * @param mime MIME type string
     * @param decoder Pointer to AudioDecoder instance
     */
    DecoderInfo(const char* mime, AudioDecoder* decoder) {
      this->mime = mime;
      this->decoder = decoder;
    }
  } actual_decoder;                       ///< Currently active decoder information
  
  Vector<DecoderInfo> decoders{0};        ///< Collection of registered decoders
#ifdef USE_EXPERIMENTAL 
  Vector<DecoderAdapter*> adapters{0};    ///< Collection of internally created adapters
#endif
  MimeDetector mime_detector;             ///< MIME type detection engine
  CodecNOP nop;                           ///< No-operation codec for unsupported formats
  MimeSource* p_mime_source = nullptr;    ///< Optional external MIME source
  bool is_first = true;                   ///< Flag for first write() call
  const char* selected_mime = nullptr;    ///< MIME type that was selected
};

}  // namespace audio_tools