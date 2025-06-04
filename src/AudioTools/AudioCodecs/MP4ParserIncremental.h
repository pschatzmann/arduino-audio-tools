#include "MP4Parser.h"

namespace audio_tools {

/**
 * @brief MP4ParserIncremental is a class that extends MP4Parser to support
 * incremental parsing of MP4 boxes.
 *
 * It allows for processing boxes as they are received, which is useful for
 * large files or streaming scenarios. It provides a callback mechanism to
 * process box data incrementally. The default callback prints the box
 * information to Serial.
 * @ingroup codecs
 */
class MP4ParserIncremental : public MP4Parser {
 public:
  using DataCallback = std::function<void(Box&, const uint8_t* data, size_t len,
                                          bool is_final, void* ref)>;

  /**
   * @brief Defines the callback for all incremental box data.
   * @param cb Callback function for all boxes.
   */
  void setIncrementalDataCallback(DataCallback cb) { data_callback = cb; }

  /**
   * @brief Defines a specific callback for incremental data of a box type.
   * @param type 4-character box type (e.g. "mdat").
   * @param cb   Callback function for this box type.
   * @param callGeneric If true, the generic callback will also be called after the type-specific callback.
   */
  void setIncrementalDataCallback(const char* type, DataCallback cb, bool callGeneric = true) {
    CallbackEntry entry;
    strncpy(entry.type, type, 4);
    entry.type[4] = '\0';  // Ensure null-termination
    entry.cb = cb;
    entry.callGeneric = callGeneric;
    data_callbacks.push_back(entry);
  }

  /**
   * @brief Defines the generic callback for all boxes.
   * @param cb Callback function for all boxes.
   */
  void setCallback(BoxCallback cb) { MP4Parser::setCallback(cb); }

  /**
   * @brief Defines a specific callback for a box type.
   * @param type 4-character box type (e.g. "moov", "mdat").
   * @param cb   Callback function for this box type.
   */
  void setCallback(const char* type, BoxCallback cb) {
    MP4Parser::setCallback(type, cb);
  }

 protected:
  /**
   * @brief Structure for type-specific incremental data callbacks.
   */
  struct CallbackEntry {
    char type[5];     ///< 4-character box type
    DataCallback cb;  ///< Callback function
    bool callGeneric = true; ///< If true, also call the generic callback after this one
  };

  DataCallback data_callback = defaultDataCallback; ///< Generic incremental data callback
  Vector<CallbackEntry> data_callbacks;             ///< List of type-specific incremental data callbacks
  bool box_in_progress = false;                     ///< True if currently parsing a box incrementally
  size_t box_bytes_received = 0;                    ///< Bytes received so far for the current box
  size_t box_bytes_expected = 0;                    ///< Total expected bytes for the current box
  char box_type[5] = {0};                           ///< Current box type
  int box_level = 0;                                ///< Current box level (nesting)
  uint64_t box_offset = 0;                          ///< Offset of the current box

  /**
   * @brief Default incremental data callback. Prints box info.
   * @param box The box being processed.
   * @param data Pointer to box data.
   * @param len Length of data.
   * @param is_final True if this is the last chunk for this box.
   * @param ref Optional reference pointer.
   */
  static void defaultDataCallback(Box& box, const uint8_t* data, size_t len,
                                  bool is_final, void* ref) {
    char space[box.level * 2 + 1];
    memset(space, ' ', box.level * 2);
    space[box.level * 2] = '\0';  // Null-terminate the string

    char str_buffer[200];
    snprintf(str_buffer, sizeof(str_buffer), "%s -> Incremental Data: %s %u ",
             space, box.type, (unsigned)len);

#ifdef ARDUINO
    Serial.println(str_buffer);
#else
    printf("%s\n", str_buffer);
#endif
  }

  /**
   * @brief Main parsing loop. Handles incremental and complete boxes.
   */
  void parse() override {
    while (true) {
      size_t bufferSize = buffer.available();
      if (!box_in_progress) {
        if (!tryStartNewBox(bufferSize)) break;
      } else {
        if (!continueIncrementalBox()) break;
      }
      popLevels();
    }
    finalizeParse();
  }

  /**
   * @brief Try to start parsing a new box. Returns false if not enough data.
   * @param bufferSize Number of bytes available in the buffer.
   * @return True if a box was started, false otherwise.
   */
  bool tryStartNewBox(size_t bufferSize) {
    if (parseOffset + 8 > bufferSize) return false;
    char type[5];

    // get basic box information
    parseOffset = checkParseOffset();
    const uint8_t* p = buffer.data() + parseOffset;
    uint32_t size32 = readU32(p);
    strncpy(type, (char*)(p + 4), 4);
    type[4] = '\0';
    uint64_t boxSize = size32;
    size_t headerSize = 8;

    if (boxSize < headerSize) return false;

    int level = static_cast<int>(levelStack.size());
    bool is_container = isContainerBox(type);

    if (is_container) {
      handleContainerBox(type, boxSize, level);
      return true;
    }

    size_t payload_size = static_cast<size_t>(boxSize - headerSize);
    if (parseOffset + boxSize <= bufferSize) {
      // start with full buffer!
      handleCompleteBox(type, p, headerSize, payload_size, level);
      parseOffset += boxSize;
    } else {
      startIncrementalBox(type, p, headerSize, payload_size, level, bufferSize);
      return false;  // Wait for more data
    }
    return true;
  }

  /**
   * @brief Handles a container box (box with children).
   * @param type Box type string.
   * @param boxSize Size of the box.
   * @param level Nesting level of the box.
   */
  void handleContainerBox(const char* type, uint64_t boxSize, int level) {
    strcpy(box.type, type);
    box.id = ++this->box.id;
    box.data = nullptr;
    box.size = static_cast<size_t>(boxSize - 8);
    box.data_size = 0;
    box.level = level;
    box.offset = fileOffset + parseOffset;
    box.is_complete = true;
    box.is_container = true;
    processCallback(box);

    uint64_t absBoxOffset = fileOffset + parseOffset;
    levelStack.push_back(absBoxOffset + boxSize);
    parseOffset += 8;
  }

  /**
   * @brief Handles a complete (non-incremental) box.
   * @param type Box type string.
   * @param p Pointer to the start of the box in the buffer.
   * @param headerSize Size of the box header.
   * @param payload_size Size of the box payload.
   * @param level Nesting level of the box.
   */
  void handleCompleteBox(const char* type, const uint8_t* p, size_t headerSize,
                         size_t payload_size, int level) {
    strcpy(box.type, type);
    box.id = ++this->box.id;
    box.data = p + headerSize;
    box.size = payload_size;
    box.data_size = payload_size;
    box.level = level;
    box.offset = fileOffset + parseOffset;
    box.is_complete = true;
    box.is_container = false;
    processCallback(box);
  }

  /**
   * @brief Starts parsing a box incrementally.
   * @param type Box type string.
   * @param p Pointer to the start of the box in the buffer.
   * @param headerSize Size of the box header.
   * @param payload_size Size of the box payload.
   * @param level Nesting level of the box.
   * @param bufferSize Number of bytes available in the buffer.
   */
  void startIncrementalBox(const char* type, const uint8_t* p,
                           size_t headerSize, size_t payload_size, int level,
                           size_t bufferSize) {
    box_in_progress = true;
    box_bytes_received = 0;
    box_bytes_expected = payload_size;
    strncpy(box_type, type, 5);
    box_level = level;
    box_offset = fileOffset + parseOffset;

    size_t available_payload = bufferSize - parseOffset - headerSize;

    if (available_payload > 0) {
      box_bytes_received += available_payload;
      if (data_callback) {
        strcpy(box.type, box_type);
        box.id = ++this->box.id;
        box.data = nullptr;
        box.data_size = available_payload;
        box.level = box_level;
        box.offset = box_offset;
        box.is_complete = false;
        box.is_container = false;

        // regular callback from parent
        box.size = payload_size;
        box.data_size = 0;
        processCallback(box);

        // incremental callback
        box.size = box_bytes_expected;
        box.data_size = available_payload;
        processDataCallback(box, p + headerSize, available_payload, false, ref);
      }
    }
    fileOffset += (bufferSize - buffer.available());
    buffer.clear();
    parseOffset = 0;
  }

  /**
   * @brief Continue filling an incremental box. Returns false if not enough data.
   * @return True if more data was processed, false otherwise.
   */
  bool continueIncrementalBox() {
    size_t to_read = std::min((size_t)box_bytes_expected - box_bytes_received,
                              (size_t)buffer.available());
    if (to_read == 0) return false;
    if (data_callback) {
      strcpy(box.type, box_type);
      box.id = ++this->box.id;
      box.data = nullptr;
      box.size = box_bytes_expected;
      box.data_size = to_read;
      box.level = box_level;
      box.offset = box_offset + box_bytes_received;
      box.is_complete = (box_bytes_received + to_read == box_bytes_expected);
      box.is_container = false;
      processDataCallback(box, buffer.data(), to_read, box.is_complete, ref);
    }
    box_bytes_received += to_read;
    fileOffset += to_read;
    buffer.clearArray(to_read);

    if (box_bytes_received >= box_bytes_expected) {
      box_in_progress = false;
    }
    return true;
  }

  /**
   * @brief Processes the incremental data callback for a box.
   * Calls the type-specific callback if present, and the generic callback if allowed.
   * @param box The box being processed.
   * @param data Pointer to box data.
   * @param len Length of data.
   * @param is_final True if this is the last chunk for this box.
   * @param ref Optional reference pointer.
   */
  void processDataCallback(Box& box, const uint8_t* data, size_t len,
                           bool is_final, void* ref) {
    bool is_called = false;
    bool call_generic = true;
    for (auto& entry : data_callbacks) {
      if (StrView(entry.type) == box.type) {
        entry.cb(box, data, len, is_final, ref);
        is_called = true;
        if (!entry.callGeneric) call_generic = false;
        break;
      }
    }
    if ((!is_called || call_generic) && data_callback) {
      data_callback(box, data, len, is_final, ref);
    }
  }

  /**
   * @brief Finalizes parsing, updating file offset and clearing buffer.
   */
  void finalizeParse() {
    if (parseOffset > 0) {
      fileOffset += parseOffset;
      buffer.clearArray(parseOffset);
      parseOffset = 0;
    }
  }
};

}  // namespace audio_tools
