#pragma once
#include <new>
#include "AudioTools/AudioCodecs/AudioCodecsBase.h"
#include "AudioTools/CoreAudio/AudioBasic/StrView.h"
#include "AudioTools/CoreAudio/AudioMetaData/MimeDetector.h"
#include "AudioTools/CoreAudio/AudioOutput.h"
#include "AudioTools/CoreAudio/BaseStream.h"

namespace audio_tools {

/**
 * @brief A Streaming Decoder where we provide both the input and output
 * as streams.
 *
 * This is the base class for all streaming decoders that process audio data
 * by reading from an input stream and writing decoded PCM data to an output
 * stream. Unlike AudioDecoder which uses a write-based interface,
 * StreamingDecoder uses a pull-based approach where you call copy() to process
 * data.
 *
 * @note This is more efficient than the write-based AudioDecoder interface
 * for streaming scenarios where you have direct access to input and output
 * streams.
 *
 * @ingroup codecs
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class StreamingDecoder : public AudioInfoSource, public AudioInfoSupport {
 public:

  virtual ~StreamingDecoder() = default;

  /**
   * @brief Starts the processing
   *
   * Initializes the decoder and prepares it for processing audio data.
   * Must be called before any copy() operations.
   *
   * @return true if initialization was successful, false otherwise
   */
  virtual bool begin() = 0;

  /**
   * @brief Releases the reserved memory
   *
   * Cleans up any resources allocated by the decoder and stops processing.
   */
  virtual void end() = 0;

  /**
   * @brief Defines the output Stream
   *
   * Sets where the decoded PCM audio data will be written to.
   *
   * @param out_stream The Print stream to write decoded audio data to
   */
  virtual void setOutput(Print& out_stream) { p_print = &out_stream; }

  /**
   * @brief Defines the output streams and register to be notified
   *
   * Sets the output stream and registers for audio info change notifications.
   *
   * @param out_stream The AudioStream to write decoded audio data to
   */
  virtual void setOutput(AudioStream& out_stream) {
    Print* p_print = &out_stream;
    setOutput(*p_print);
    addNotifyAudioChange(out_stream);
  }

  /**
   * @brief Defines the output streams and register to be notified
   *
   * Sets the output stream and registers for audio info change notifications.
   *
   * @param out_stream The AudioOutput to write decoded audio data to
   */
  virtual void setOutput(AudioOutput& out_stream) {
    Print* p_print = &out_stream;
    setOutput(*p_print);
    addNotifyAudioChange(out_stream);
  }

  /**
   * @brief Stream Interface: Decode directly by taking data from the stream
   *
   * This is more efficient than feeding the decoder with write: just call
   * copy() in the loop to process data from the input stream.
   *
   * @param inStream The input stream containing encoded audio data
   */
  void setInput(Stream& inStream) { this->p_input = &inStream; }

  /**
   * @brief Provides the audio information for the current stream
   *
   * Returns audio format information such as sample rate, channels, and
   * bits per sample that was determined from the decoded audio stream.
   *
   * @return AudioInfo structure containing format information
   */
  virtual AudioInfo audioInfo() = 0;

  /**
   * @brief Checks if the class is active
   *
   * @return true if the decoder is ready and active, false otherwise
   */
  virtual operator bool() = 0;

  /**
   * @brief Process a single read operation - to be called in the loop
   *
   * Reads a chunk of data from the input stream, decodes it, and writes
   * the decoded PCM data to the output stream.
   *
   * @return true if data was processed successfully, false if no more data
   *         is available or an error occurred
   */
  virtual bool copy() = 0;

  /**
   * @brief Process all available data
   *
   * Convenience method that calls copy() repeatedly until all available
   * data has been processed.
   *
   * @return true if any data was processed, false if no data was available
   */
  bool copyAll() {
    bool result = false;
    while (copy()) {
      result = true;
    }
    return result;
  }

  /**
   * @brief Provides the MIME type of the audio format handled by this decoder
   *
   * @return C-string containing the MIME type (e.g., "audio/mpeg",
   * "audio/flac")
   */
  virtual const char* mime() = 0;

 protected:
  /**
   * @brief Reads bytes from the input stream
   *
   * Derived classes must implement this to read data from their input source.
   *
   * @param data Buffer to store the read data
   * @param len Maximum number of bytes to read
   * @return Number of bytes actually read
   */
  virtual size_t readBytes(uint8_t* data, size_t len) = 0;

  void setAudioInfo(AudioInfo newInfo) override {
    TRACED();
    if (this->info != newInfo) {
      this->info = newInfo;
      notifyAudioChange(info);
    }
  }

  Print* p_print = nullptr;   ///< Output stream for decoded PCM data
  Stream* p_input = nullptr;  ///< Input stream for encoded audio data
  AudioInfo info;
};

/**
 * @brief Converts any AudioDecoder to a StreamingDecoder
 *
 * This adapter class allows you to use any existing AudioDecoder with the
 * StreamingDecoder interface. It handles the conversion between the write-based
 * AudioDecoder API and the stream-based StreamingDecoder API by using an
 * internal buffer.
 *
 * @note The adapter reads data from the input stream into a buffer, then
 * feeds that data to the wrapped AudioDecoder using its write() method.
 *
 * @ingroup codecs
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class StreamingDecoderAdapter : public StreamingDecoder {
 public:
  /**
   * @brief Constructor
   *
   * @param decoder The AudioDecoder to wrap
   * @param mimeStr The MIME type string for this decoder
   * @param copySize Buffer size for data transfer (default:
   * DEFAULT_BUFFER_SIZE)
   */
  StreamingDecoderAdapter(AudioDecoder& decoder, const char* mimeStr,
                          int copySize = DEFAULT_BUFFER_SIZE) {
    p_decoder = &decoder;
    p_decoder->addNotifyAudioChange(*this);
    mime_str = mimeStr;
    if (copySize > 0) resize(copySize);
  }

  /**
   * @brief Starts the processing
   *
   * Initializes the wrapped decoder.
   *
   * @return true if initialization was successful, false otherwise
   */
  bool begin() override { 
    TRACED();
    if (p_decoder == nullptr) return false;
    if (p_input == nullptr) return false;
    return p_decoder->begin(); 
  }

  /**
   * @brief Releases the reserved memory
   *
   * Calls end() on the wrapped decoder to clean up resources.
   */
  void end() override { p_decoder->end(); }

  /**
   * @brief Defines the output Stream
   *
   * Sets the output stream for the wrapped decoder.
   *
   * @param out_stream The output stream for decoded audio data
   */
  void setOutput(Print& out_stream) override {
    p_decoder->setOutput(out_stream);
  }

  /**
   * @brief Provides the audio information
   *
   * Delegates to the wrapped decoder's audioInfo() method.
   *
   * @return AudioInfo from the wrapped decoder
   */
  AudioInfo audioInfo() override { return p_decoder->audioInfo(); }

  /**
   * @brief Checks if the class is active
   *
   * @return true if the wrapped decoder is active, false otherwise
   */
  virtual operator bool() override { return *p_decoder; }

  /**
   * @brief Process a single read operation - to be called in the loop
   *
   * Reads data from the input stream into the internal buffer, then feeds
   * it to the wrapped AudioDecoder for processing.
   *
   * @return true if data was processed successfully, false otherwise
   */
  virtual bool copy() override {
    int read = readBytes(buffer.data(), buffer.size());
    int written = 0;
    if (read > 0) written = p_decoder->write(&buffer[0], read);
    bool rc = written > 0;
    LOGI("copy: %s", rc ? "success" : "failure");
    return rc;
  }

  /**
   * @brief Adjust the buffer size
   *
   * Changes the internal buffer size. The existing content of the buffer is
   * lost!
   *
   * @param bufferSize New buffer size in bytes
   */
  void resize(int bufferSize) { buffer.resize(bufferSize); }

  /**
   * @brief Provides the MIME type
   *
   * Returns the MIME type that was defined in the constructor.
   *
   * @return MIME type string
   */
  const char* mime() override { return mime_str; }

 protected:
  AudioDecoder* p_decoder = nullptr;  ///< Wrapped AudioDecoder instance
  Vector<uint8_t> buffer{0};          ///< Internal buffer for data transfer
  const char* mime_str = nullptr;     ///< MIME type string

  /**
   * @brief Reads bytes from the input stream
   *
   * @param data Buffer to store the read data
   * @param len Maximum number of bytes to read
   * @return Number of bytes actually read
   */
  size_t readBytes(uint8_t* data, size_t len) override {
    if (p_input == nullptr) return 0;
    return p_input->readBytes(data, len);
  }
};

/**
 * @brief Manage multiple StreamingDecoders with automatic format detection
 *
 * This class automatically detects the audio format from incoming streaming
 * data and selects the appropriate decoder from a collection of registered
 * decoders. The format detection is performed using the MimeDetector on the
 * first chunk of data, and the detected data is preserved for the selected
 * decoder using a buffered stream approach.
 *
 * Key features:
 * - Automatic format detection using MimeDetector
 * - Support for multiple decoder registration
 * - Data preservation during format detection
 * - Custom mime type detection logic support
 * - Seamless integration with existing streaming architecture
 *
 * @note The first call to copy() will consume some data for format detection,
 * but this data is preserved and made available to the selected decoder through
 * a BufferedPrefixStream mechanism.
 *
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class MultiStreamingDecoder : public StreamingDecoder {
 public:
  /**
   * @brief Default constructor
   */
  MultiStreamingDecoder() = default;

  /**
   * @brief Destructor
   *
   * Cleans up any internally created StreamingDecoderAdapter instances.
   */
  ~MultiStreamingDecoder() {
    // Clean up any adapters we created
    for (auto* adapter : adapters) {
      delete adapter;
    }
    adapters.clear();    
  }

  /**
   * @brief Starts the processing
   *
   * Initializes the MIME detector and prepares for format detection.
   *
   * @return true if initialization was successful, false if no output is
   * defined
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
   * @brief Releases the reserved memory
   *
   * Stops the currently active decoder and resets the state for next use.
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
   * @brief Defines the output Stream
   *
   * @param out_stream The output stream for decoded audio data
   */
  void setOutput(Print& out_stream) override {
    StreamingDecoder::setOutput(out_stream);
  }

  /**
   * @brief Defines the output streams and register to be notified
   *
   * @param out_stream The AudioStream for decoded audio data
   */
  void setOutput(AudioStream& out_stream) override {
    StreamingDecoder::setOutput(out_stream);
  }

  /**
   * @brief Defines the output streams and register to be notified
   *
   * @param out_stream The AudioOutput for decoded audio data
   */
  void setOutput(AudioOutput& out_stream) override {
    StreamingDecoder::setOutput(out_stream);
  }

  /**
   * @brief Stream Interface: Decode directly by taking data from the stream
   *
   * @param inStream The input stream containing encoded audio data
   */
  void setInput(Stream& inStream) { 
    StreamingDecoder::setInput(inStream);
  }

  /**
   * @brief Adds a decoder that will be selected by its MIME type
   *
   * Registers a StreamingDecoder that will be automatically selected when
   * the corresponding MIME type is detected in the input stream.
   *
   * @param decoder The StreamingDecoder to register
   */
  void addDecoder(StreamingDecoder& decoder) {
    decoder.addNotifyAudioChange(*this);
    const char* mime = decoder.mime();
    if (mime != nullptr) {
      DecoderInfo info{mime, &decoder};
      decoders.push_back(info);
    } else {
      LOGE("Decoder mime() returned nullptr - cannot add decoder");
    }
  }

  /**
   * @brief Adds a decoder with explicit MIME type
   *
   * Registers a StreamingDecoder with a specific MIME type, which may be
   * different from what the decoder's mime() method returns.
   *
   * @param decoder The StreamingDecoder to register
   * @param mime The MIME type string to associate with this decoder
   */
  void addDecoder(StreamingDecoder& decoder, const char* mime) {
    if (mime != nullptr) {
      decoder.addNotifyAudioChange(*this);
      DecoderInfo info{mime, &decoder};
      decoders.push_back(info);
    } else {
      LOGE("Decoder mime() returned nullptr - cannot add decoder");
    }
  }

  /**
   * @brief Adds an AudioDecoder with explicit MIME type
   *
   * Wraps an AudioDecoder in a StreamingDecoderAdapter and registers it with
   * the specified MIME type. This allows using traditional AudioDecoder
   * instances with the MultiStreamingDecoder's automatic format detection.
   *
   * @param decoder The AudioDecoder to wrap and register
   * @param mime The MIME type string to associate with this decoder
   * @param bufferSize Buffer size for the adapter (default:
   * DEFAULT_BUFFER_SIZE)
   *
   * @note The created StreamingDecoderAdapter is stored internally and will be
   * automatically managed by the MultiStreamingDecoder.
   */
  void addDecoder(AudioDecoder& decoder, const char* mime,
                  int bufferSize = DEFAULT_BUFFER_SIZE) {
    if (mime != nullptr) {
      // Create a StreamingDecoderAdapter to wrap the AudioDecoder
      decoder.addNotifyAudioChange(*this);
      auto adapter = new StreamingDecoderAdapter(decoder, mime, bufferSize);
      adapters.push_back(adapter);  // Store for cleanup

      DecoderInfo info{mime, adapter};
      decoders.push_back(info);
    } else {
      LOGE("MIME type is nullptr - cannot add AudioDecoder");
    }
  }

  /**
   * @brief Checks if the class is active
   *
   * @return true if a decoder is selected and active, or if format detection
   *         hasn't been performed yet
   */
  virtual operator bool() override {
    if (actual_decoder.decoder == nullptr) return false;
    return is_first || actual_decoder.is_open;
  }

  /**
   * @brief Process a single read operation - to be called in the loop
   *
   * On the first call, this method reads data for format detection, selects
   * the appropriate decoder, and sets up a buffered stream. Subsequent calls
   * delegate to the selected decoder's copy() method.
   *
   * @return true if data was processed successfully, false if no data is
   *         available or format detection/decoding failed
   */
  virtual bool copy() override {
    if (p_input == nullptr) return false;

    // Automatically select decoder if not already selected
    if (is_first) {
      // determine the mime and select the decoder
      if (!selectDecoder()) {
        return false;
      }
      is_first = false;
    }

    // Check if we have a decoder
    if (actual_decoder.decoder == nullptr) return false;

    // Use the selected decoder to process data
    return actual_decoder.decoder->copy();
  }

  /**
   * @brief Selects the actual decoder by MIME type
   *
   * Searches through registered decoders to find one that matches the
   * detected MIME type, then initializes it for use.
   *
   * @param mime The MIME type string to match
   * @return true if a matching decoder was found and initialized, false
   * otherwise
   */
  bool selectDecoder(const char* mime) {
    TRACEI();
    bool result = false;
    
    // Guard against null MIME type - cannot proceed without valid MIME
    if (mime == nullptr) {
      LOGE("mime is null");
      return false;
    }

    // Optimization: Check if the requested MIME type is already active
    // This avoids unnecessary decoder switching when the same format is detected
    if (StrView(mime).equals(actual_decoder.mime)) {
      is_first = false;  // Mark initialization as complete
      return true;       // Already using the correct decoder
    }

    // Clean shutdown of currently active decoder before switching
    // This ensures proper resource cleanup and state reset
    if (actual_decoder.decoder != nullptr) {
      actual_decoder.decoder->end();
      actual_decoder.is_open = false;  // Mark as inactive
    }

    // Search through all registered decoders to find one that handles this MIME type
    selected_mime = nullptr;  // Clear previous selection
    for (int j = 0; j < decoders.size(); j++) {
      DecoderInfo info = decoders[j];
      
      // Check if this decoder supports the detected MIME type
      if (StrView(info.mime).equals(mime)) {
        LOGI("Using Decoder %s for %s", toStr(info.mime), toStr(mime));
        
        // Switch to the matching decoder
        actual_decoder = info;

        // Configure the decoder's output stream to match our output
        // This ensures decoded audio data flows to the correct destination
        if (p_print != nullptr) {
          actual_decoder.decoder->setOutput(*p_print);
        }

        // Initialize the selected decoder and mark it as active
        LOGI("available: %d", p_data_source->available());
        assert(p_data_source != nullptr);
        actual_decoder.decoder->setInput(*p_data_source);
        actual_decoder.decoder->clearNotifyAudioChange();
        actual_decoder.decoder->addNotifyAudioChange(*this);
        if (actual_decoder.decoder->begin()) {
          actual_decoder.is_open = true;
          LOGI("StreamingDecoder %s started", toStr(actual_decoder.mime));
        } else {
          // Decoder failed to start - this is a critical error
          LOGE("Failed to start StreamingDecoder %s", toStr(actual_decoder.mime));
          return false;
        }

        // Successfully found and initialized a decoder
        result = true;
        selected_mime = mime;  // Store the MIME type that was selected
        break;                 // Stop searching once we find a match
      }
    }
    
    // Mark initialization phase as complete regardless of success/failure
    is_first = false;
    return result;  // true if decoder was found and started, false otherwise
  }

  /**
   * @brief Provides the MIME type of the selected decoder
   * @return MIME type string of the currently active decoder, or nullptr
   *         if no decoder is selected
   */
  const char* mime() override {
    // fallback to actual decoder
    if (actual_decoder.decoder != nullptr) {
      return actual_decoder.decoder->mime();
    }
    return nullptr;
  }

  /**
   * @brief Returns the MIME type that was detected and selected
   *
   * @return The MIME type string that was detected by the MimeDetector
   */
  const char* selectedMime() { return selected_mime; }

  /**
   * @brief Provides the audio information from the selected decoder
   *
   * @return AudioInfo from the currently active decoder, or empty AudioInfo
   *         if no decoder is selected
   */
  AudioInfo audioInfo() override {
    if (actual_decoder.decoder != nullptr) {
      return actual_decoder.decoder->audioInfo();
    }
    AudioInfo empty;
    return empty;
  }

  /**
   * @brief Provides access to the internal MIME detector
   *
   * Returns a reference to the MimeDetector instance used for automatic
   * format detection. This allows access to advanced features such as:
   * - Adding custom MIME type detection logic
   * - Setting custom detection callbacks
   * - Configuring default MIME types
   * - Accessing detection statistics
   *
   * @note This method should typically only be used for advanced configuration
   * before calling begin(). Modifying the detector after format detection
   * has occurred may lead to unexpected behavior.
   *
   * @return Reference to the internal MimeDetector instance
   *
   * @see MimeDetector::setCheck() for adding custom detection logic
   * @see MimeDetector::setMimeCallback() for detection notifications
   */
  MimeDetector& mimeDetector() { return mime_detector; }

  /**
   * @brief Sets an external MIME source for format detection
   *
   * Provides an alternative to automatic MIME detection by allowing an external
   * source to provide the MIME type information. This is particularly useful
   * when the MIME type is already known from other sources such as:
   * - HTTP Content-Type headers
   * - File extensions
   * - Metadata from containers or playlists
   * - User-specified format preferences
   *
   * When a MIME source is set, the automatic detection process (which requires
   * reading and analyzing stream data) is bypassed, making the decoder
   * initialization more efficient and faster.
   *
   * @param mimeSource Reference to a MimeSource object that provides the
   *                   MIME type through its mime() method
   *
   * @note The MimeSource object must remain valid for the lifetime of this
   *       MultiStreamingDecoder instance, as only a reference is stored.
   *
   * @note Setting a MIME source takes precedence over automatic detection.
   *       To revert to automatic detection, the MIME source would need to
   *       return nullptr from its mime() method.
   *
   * @see MimeSource interface for implementing custom MIME providers
   * @see selectDecoder() for how MIME type detection and selection works
   *
   * @since This feature allows integration with external metadata sources
   */
  void setMimeSource(MimeSource& mimeSource) { p_mime_source = &mimeSource; }

 protected:

  /**
   * @brief Information about a registered decoder
   */
  struct DecoderInfo {
    const char* mime = nullptr;           ///< MIME type for this decoder
    StreamingDecoder* decoder = nullptr;  ///< Pointer to the decoder instance
    bool is_open = false;  ///< Whether the decoder is currently active

    /**
     * @brief Default constructor
     */
    DecoderInfo() = default;

    /**
     * @brief Constructor with parameters
     *
     * @param mime MIME type string
     * @param decoder Pointer to StreamingDecoder instance
     */
    DecoderInfo(const char* mime, StreamingDecoder* decoder) {
      this->mime = mime;
      this->decoder = decoder;
    }
  } actual_decoder;  ///< Currently active decoder information

  Vector<DecoderInfo> decoders{0};  ///< Collection of registered decoders
  Vector<StreamingDecoderAdapter*> adapters{
      0};                      ///< Collection of internally created adapters
  MimeDetector mime_detector;  ///< MIME type detection engine
  Vector<uint8_t> detection_buffer{0};  ///< Buffer for format detection data
  bool is_first = true;                 ///< Flag for first copy() call
  const char* selected_mime = nullptr;  ///< MIME type that was selected
  MimeSource* p_mime_source =
      nullptr;  ///< Optional MIME source for custom logic
  Stream *p_data_source = nullptr; ///< effective data source for decoder

  BufferedStream buffered_stream{0};  ///< Buffered stream for data preservation
  const char* toStr(const char* str){
    return str == nullptr ? "" : str;
  }

  /**
   * @brief Automatically detects MIME type and selects appropriate decoder
   *
   * This method performs automatic format detection and decoder selection when
   * no decoder is currently active. It supports two modes of operation:
   * 1. External MIME source - Uses a provided MimeSource for format information
   * 2. Auto-detection - Analyzes stream content to determine the audio format
   *
   * The method reads a small sample of data (80 bytes) from the input stream
   * for format detection, then preserves this data in a buffered stream so it
   * remains available to the selected decoder. This ensures no audio data is
   * lost during the detection process.
   *
   * @note This method is automatically called by copy() on the first invocation.
   * Subsequent calls will return immediately if a decoder is already selected.
   *
   * @note The detection data is preserved using BufferedPrefixStream, allowing
   * the selected decoder to process the complete stream including the bytes
   * used for format identification.
   *
   * @return true if a decoder was successfully selected and initialized, or if
   *         a decoder was already active; false if MIME detection failed or no
   *         matching decoder was found
   *
   * @see selectDecoder(const char* mime) for explicit decoder selection
   * @see setMimeSource() for providing external MIME type information
   * @see MimeDetector for details on automatic format detection
   */
  bool selectDecoder() {
    // Only perform MIME detection and decoder selection if no decoder is active yet
    // This prevents re-detection on subsequent calls during the same stream
    if (actual_decoder.decoder == nullptr) {
      const char* mime = nullptr;
      p_data_source = nullptr;

      // Two methods for MIME type determination: external source or auto-detection
      if (p_mime_source != nullptr) {
        // Option 1: Use externally provided MIME source (e.g., from HTTP headers)
        // This is more efficient as it avoids reading and analyzing stream data
        mime = p_mime_source->mime();
        LOGI("mime from source: %s", toStr(mime));
        assert(p_input != nullptr);
        p_data_source = p_input;
      } else {
        // Option 2: Auto-detect MIME type by analyzing stream content
        // Redirect the decoder to use the buffered stream
        // we use the buffered stream as input
        assert(p_input != nullptr);
        buffered_stream.setStream(*p_input);
        buffered_stream.resize(DEFAULT_BUFFER_SIZE);
        p_data_source = &buffered_stream;

        // This requires reading a sample of data to identify the format
        detection_buffer.resize(160);
        size_t bytesRead = buffered_stream.peekBytes(detection_buffer.data(), detection_buffer.size());        // If no data is available, we cannot proceed with detection
        if (bytesRead == 0) return false;

        // Feed the sample data to the MIME detector for format analysis
        // The detector examines file headers, magic numbers, etc.
        mime_detector.write(detection_buffer.data(), bytesRead);
        mime = mime_detector.mime();
        LOGI("mime from detector: %s", toStr(mime));
        
      }
      
      // Process the detected/provided MIME type
      if (mime != nullptr) {        
        // Delegate to the overloaded selectDecoder(mime) method to find
        // and initialize the appropriate decoder for this MIME type
        if (!selectDecoder(mime)) {
          LOGE("The decoder could not be selected for %s", toStr(mime));
          return false;  // No registered decoder can handle this format
        }
      } else {
        // MIME detection failed - format is unknown or unsupported
        LOGE("Could not determine mime type");
        return false;
      }
    } else {
      LOGI("Decoder already selected: %s", toStr(actual_decoder.mime));
      assert(p_input != nullptr);
      actual_decoder.decoder->setInput(*p_input);
    }
    
    // Success: either decoder was already selected or selection completed successfully
    return true;
  }

  /**
   * @brief Reads bytes from the input stream
   *
   * @param data Buffer to store read data
   * @param len Maximum number of bytes to read
   * @return Number of bytes actually read
   */
  size_t readBytes(uint8_t* data, size_t len) override {
    if (p_input == nullptr) return 0;
    return p_input->readBytes(data, len);
  }
};

/**
 * @brief Adapter class which allows the AudioDecoder API on a StreamingDecoder
 *
 * This adapter provides the reverse functionality of StreamingDecoderAdapter:
 * it allows you to use a StreamingDecoder with the write-based AudioDecoder
 * API. It uses a ring buffer and queue to convert write() calls into a stream
 * that the StreamingDecoder can read from.
 *
 * @note This is useful when you have a StreamingDecoder but need to integrate
 * it into code that expects the AudioDecoder write-based interface.
 *
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class DecoderAdapter : public AudioDecoder {
 public:
  /**
   * @brief Constructor
   *
   * @param dec The StreamingDecoder to wrap
   * @param bufferSize Size of the internal ring buffer for data transfer
   */
  DecoderAdapter(StreamingDecoder& dec, int bufferSize) {
    TRACED();
    p_dec = &dec;
    p_dec->setInput(queue);
    resize(bufferSize);
  }

  /**
   * @brief Defines the output Stream
   *
   * Sets the output stream for the wrapped StreamingDecoder.
   *
   * @param out The output stream for decoded audio data
   */
  void setOutput(Print& out) override { p_dec->setOutput(out); }

  /**
   * @brief Sets the input stream for the wrapped decoder
   *
   * @param in The input stream containing encoded audio data
   */
  void setInput(Stream& in) { p_dec->setInput(in); }

  /**
   * @brief Starts the processing
   *
   * Initializes the wrapped StreamingDecoder and marks this adapter as active.
   *
   * @return true if the StreamingDecoder was started successfully
   */
  bool begin() override {
    TRACED();
    active = true;
    bool rc = p_dec->begin();
    return rc;
  }

  /**
   * @brief Stops the processing
   *
   * Marks this adapter as inactive. The wrapped StreamingDecoder is not
   * explicitly stopped to allow continued use.
   */
  void end() override {
    TRACED();
    active = false;
  }

  /**
   * @brief Resizes the internal buffer
   *
   * Changes the size of the ring buffer used for data transfer.
   * The buffer is only allocated when first needed (lazy setup).
   *
   * @param size New buffer size in bytes
   */
  void resize(int size) {
    buffer_size = size;
    // setup the buffer only if needed
    if (is_setup) rbuffer.resize(size);
  }

  /**
   * @brief Writes encoded audio data to be decoded
   *
   * The data is written to an internal queue, which is then processed
   * by calling copy() on the wrapped StreamingDecoder.
   *
   * @param data Buffer containing encoded audio data
   * @param len Number of bytes to write
   * @return Number of bytes actually written
   */
  size_t write(const uint8_t* data, size_t len) override {
    TRACED();
    setupLazy();
    size_t result = queue.write((uint8_t*)data, len);
    // Trigger processing - process all available data
    while (p_dec->copy());

    return result;
  }

  /**
   * @brief Gets the wrapped StreamingDecoder
   *
   * Provides direct access to the underlying StreamingDecoder for
   * advanced use cases.
   *
   * @return Pointer to the wrapped StreamingDecoder
   */
  StreamingDecoder* getStreamingDecoder() { return p_dec; }

  /**
   * @brief Checks if the adapter is active
   *
   * @return true if the adapter is active, false otherwise
   */
  operator bool() override { return active; }

 protected:
  bool active = false;                ///< Whether the adapter is active
  bool is_setup = false;              ///< Whether lazy setup has been performed
  int buffer_size;                    ///< Size of the ring buffer
  StreamingDecoder* p_dec = nullptr;  ///< Wrapped StreamingDecoder instance
  RingBuffer<uint8_t> rbuffer{0};     ///< Ring buffer for data storage
  QueueStream<uint8_t> queue{rbuffer};  ///< Stream interface to the ring buffer

  /**
   * @brief Performs lazy initialization of the ring buffer
   *
   * The ring buffer is only allocated when first needed to save memory.
   */
  void setupLazy() {
    if (!is_setup) {
      rbuffer.resize(buffer_size);
      queue.begin();
      is_setup = true;
    }
  }
};

/**
 * @brief Type alias for DecoderAdapter
 *
 * Provides an alternative name for backward compatibility.
 */
using DecoderFromStreaming = DecoderAdapter;

}  // namespace audio_tools
