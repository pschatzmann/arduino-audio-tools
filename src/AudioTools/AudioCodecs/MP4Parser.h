#pragma once

#include <cstdint>
#include <cstring>
#include <functional>
#include <string>

#include "AudioTools/CoreAudio/Buffers.h"

namespace audio_tools {

/**
 * @brief MP4Parser is a class that parses MP4 container files and extracts
 * boxes (atoms). It provides a callback mechanism to process each box as it is
 * parsed. You can define specific callbacks for individual box types or use a
 * generic callback for the undefined boxes: By default it just prints the box
 * information to Serial. If a container box contains data, it will be processed
 * recursively and if it contains data itself, it might be reported in a second
 * callback call.
 * @note This parser expect the mdat box to be the last box in the file. This
 * can be achieve with the following ffmpeg commands:
 * - ffmpeg -i ../sine.wav -c:a alac  -movflags +faststart alac.m4a
 * - ffmpeg -i ../sine.wav -c:a aac  -movflags +faststart aac.m4a
 *
 * @ingroup codecs
 * @author Phil Schatzmann
 */
class MP4Parser {
 public:
  /**
   * @brief Represents an individual box in the MP4 file.
   */
  struct Box {
    friend class MP4Parser;     ///< Allow MP4Parser to access private members
    friend class MP4ParserExt;  ///< Allow MP4ParserExt to access private
                                ///< members
    size_t id = 0;              ///< Unique box ID
    size_t seq = 0;             ///< Sequence number for the box per id
    char type[5];               ///< 4-character box type (null-terminated)
    const uint8_t* data =
        nullptr;           ///< Pointer to box payload (not including header)
    size_t data_size = 0;  ///< Size of payload (not including header)
    size_t size =
        0;  ///< Size of payload including subboxes (not including header)
    int level = 0;              ///< Nesting depth
    uint64_t file_offset = 0;   ///< File offset where box starts
    int available = 0;         ///< Number of bytes available as data
    bool is_complete = false;   ///< True if the box data is complete
    bool is_incremental = false;  ///< True if the box is being parsed incrementally
    bool is_container = false;  ///< True if the box is a container
  };

  using BoxCallback = std::function<void(Box&, void* ref)>;

  /**
   * @brief Structure for type-specific callbacks.
   */
  struct CallbackEntry {
    char type[5];    ///< 4-character box type
    BoxCallback cb;  ///< Callback function
    bool callGeneric =
        true;  ///< If true, also call the generic callback after this one
  };

  /**
   * @brief Defines an optional reference. By default it is the parser itself.
   * @param ref Pointer to reference object.
   */
  void setReference(void* ref) { this->ref = ref; }

  /**
   * @brief Defines the generic callback for all boxes.
   * @param cb Callback function for all boxes.
   */
  void setCallback(BoxCallback cb) { callback = cb; }

  /**
   * @brief Defines a specific callback for a box type.
   * @param type 4-character box type (e.g. "moov", "mdat").
   * @param cb   Callback function for this box type.
   * @param callGeneric If true, the generic callback will also be called after
   * the type-specific callback.
   */
  void setCallback(const char* type, BoxCallback cb, bool callGeneric = true) {
    CallbackEntry entry;
    strncpy(entry.type, type, 4);
    entry.type[4] = '\0';  // Ensure null-termination
    entry.cb = cb;
    entry.callGeneric = callGeneric;
    callbacks.push_back(entry);
  };

  /**
   * @brief Defines a specific buffer size.
   * @param size Buffer size in bytes.
   * @return true if the buffer was resized successfully.
   */
  bool resize(size_t size) {
    buffer.resize(size);
    return buffer.size() == size;
  }

  /**
   * @brief Initializes the parser.
   * @return true on success.
   */
  bool begin() {
    buffer.clear();
    if (buffer.size() == 0) buffer.resize(2 * 1024);
    parseOffset = 0;
    fileOffset = 0;
    levelStack.clear();
    box.is_complete = true;  // Start with no open box
    box.data = nullptr;
    box.size = 0;
    box.level = 0;
    box.file_offset = 0;
    box.id = 0;
    box.is_incremental = false;
    box.is_complete = true;
    return true;
  }

  /**
   * @brief Provide the data to the parser (in chunks if needed).
   * @param data Pointer to input data.
   * @param len Length of input data.
   * @return Number of bytes written to the buffer.
   */
  size_t write(const uint8_t* data, size_t len) {
    if (is_error) return len;  // If an error occurred, skip writing
    size_t result = buffer.writeArray(data, len);
    parse();
    return result;
  }

  /**
   * @brief Provide the data to the parser (in chunks if needed).
   * @param data Pointer to input data (char*).
   * @param len Length of input data.
   * @return Number of bytes written to the buffer.
   */
  size_t write(const char* data, size_t len) {
    return write(reinterpret_cast<const uint8_t*>(data), len);
  }

  /**
   * @brief Returns the available space for writing.
   * @return Number of bytes available for writing.
   */
  int availableForWrite() { return buffer.availableForWrite(); }

  /**
   * @brief Adds a box name that will be interpreted as a container.
   * @param name Name of the container box.
   * @param start Offset of child boxes (default 0).
   */
  void addContainer(const char* name, int start = 0) {
    ContainerInfo info;
    info.name = name;
    info.start = start;  // offset of child boxes
  }

  /**
   * @brief Trigger separate parsing (and callbacks) on the indicated string.
   * @param str Pointer to the string data.
   * @param len Length of the string data.
   * @return Number of bytes parsed.
   */
  int parseString(const uint8_t* str, int len, int fileOffset = 0,
                  int level = 0) {
    char type[5];
    int idx = 0;
    Box box;
    while (true) {
      if (!isValidType((const char*)str + idx + 4)) {
        return idx;
      }
      size_t box_size = readU32(str + idx) - 8;
      box.data = str + 8 + idx;
      box.size = box_size;
      box.level = level;
      box.data_size = box.size;
      box.file_offset = fileOffset + idx;
      box.is_complete = true;
      box.is_incremental = false;
      strncpy(box.type, (char*)(str + idx + 4), 4);
      box.type[4] = '\0';
      idx += box.size;
      processCallback(box);
      if (idx >= len) break;  // No more data to parse
    }
    return idx;
  }

  /// find box in box
  bool findBox(const char* name, const uint8_t* data, size_t len, Box& result) {
    for (int j = 0; j < len - 4; j++) {
      if (!isValidType((const char*)data + j + 4)) {
        continue;  // Skip invalid types
      }
      size_t box_size = readU32(data + j) - 8;
      if (box_size < 8) continue;  // Invalid box size
      Box box;
      box.data = data + j + 8;
      box.size = box_size;
      box.data_size = box.size;
      strncpy(box.type, (char*)(data + j + 4), 4);
      box.type[4] = '\0';
      if (StrView(box.type) == name) {
        result = box;
        return true;  // Found the box
      }
    }
    return false;
  }

  /**
   * @brief Default callback that prints box information to Serial.
   * @param box The box being processed.
   * @param ref Optional reference pointer.
   */
  static void defaultCallback(const Box& box, void* ref) {
    char space[box.level * 2 + 1];
    char str_buffer[200];
    memset(space, ' ', box.level * 2);
    space[box.level * 2] = '\0';  // Null-terminate the string
    snprintf(str_buffer, sizeof(str_buffer),
             "%s- #%u %u) %s, Offset: %u, Size: %u, Data Size: %u, Available: %u", space,
             (unsigned)box.id, (unsigned) box.seq, box.type, (unsigned)box.file_offset,
             (unsigned)box.size, (unsigned) box.data_size, (unsigned) box.available);
#ifdef ARDUINO
    Serial.println(str_buffer);
#else
    printf("%s\n", str_buffer);
#endif
  }

 protected:
  BoxCallback callback = defaultCallback;  ///< Generic callback for all boxes
  Vector<CallbackEntry> callbacks;         ///< List of type-specific callbacks
  SingleBuffer<uint8_t> buffer;            ///< Buffer for incoming data
  Vector<size_t> levelStack;               ///< Stack for container box levels
  size_t parseOffset = 0;                  ///< Current parse offset in buffer
  uint64_t fileOffset = 0;                 ///< Current file offset
  void* ref = this;                        ///< Reference pointer for callbacks
  Box box;                                 ///< Current box being processed
  bool is_error = false;                   ///< True if an error occurred

  /**
   * @brief Structure for container box information.
   */
  struct ContainerInfo {
    const char* name = nullptr;  ///< Name of the container box
    int start = 0;               ///< Offset of child boxes
  };
  Vector<ContainerInfo> containers;  ///< List of container box info
protected:
  bool box_in_progress =
      false;  ///< True if currently parsing a box incrementally
  size_t box_bytes_received = 0;  ///< Bytes received so far for the current box
  size_t box_bytes_expected = 0;  ///< Total expected bytes for the current box
  char box_type[5] = {0};         ///< Current box type
  int box_level = 0;              ///< Current box level (nesting)
  int box_seq = 0;
  size_t incremental_offset = 0;

  /**
   * @brief Main parsing loop. Handles incremental and complete boxes.
   */
  void parse()  {
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
    box_seq = 0;

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
    box.available = 0;
    box.level = level;
    box.file_offset = fileOffset + parseOffset;
    box.is_incremental = false;
    box.is_complete = true;
    box.is_container = true;
    box.seq = 0;

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
    box.file_offset = fileOffset + parseOffset;
    box.is_complete = true;
    box.is_container = false;
    box.available = payload_size;
    box.is_incremental = false;
    box.seq = 0;

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
    box_seq = 0;

    size_t available_payload = bufferSize - parseOffset - headerSize;
    incremental_offset = fileOffset + parseOffset;
    if (available_payload > 0) {
      box_bytes_received += available_payload;
      strcpy(box.type, box_type);
      box.id = ++this->box.id;
      box.data = p + headerSize;
      box.size = box_bytes_expected;
      box.data_size = box_bytes_expected;
      box.available = available_payload;
      box.level = box_level;
      box.file_offset = incremental_offset;
      box.seq = 0;
      box.is_incremental = true;
      box.is_complete = false;
      box.is_container = false;
      processCallback(box);
    }
    // fileOffset += (bufferSize - buffer.available());
    fileOffset += (parseOffset + payload_size + 8);
    incremental_offset += available_payload;
    buffer.clear();
    parseOffset = 0;
  }

  /**
   * @brief Continue filling an incremental box. Returns false if not enough
   * data.
   * @return False if more data was processed, true otherwise.
   */
  bool continueIncrementalBox() {
    size_t to_read = std::min((size_t)box_bytes_expected - box_bytes_received,
                              (size_t)buffer.available());
    if (to_read == 0) return true;
    strcpy(box.type, box_type);
    box.id = ++this->box.id;
    box.data = buffer.data();
    box.size = box_bytes_expected;
    box.data_size = box_bytes_expected;
    box.available = to_read;
    box.level = box_level;
    box.file_offset = incremental_offset;
    box.is_complete = (box_bytes_received + to_read == box_bytes_expected);
    box.is_container = false;
    box.is_incremental = true;
    box.seq = ++box_seq;
    processCallback(box);
    box_bytes_received += to_read;
    // fileOffset += to_read;
    buffer.clearArray(to_read);
    incremental_offset += to_read;

    if (box_bytes_received >= box_bytes_expected) {
      box_in_progress = false;
    }
    return false;
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

  /**
   * @brief Returns the current file offset (absolute position in file).
   * @return Current file offset.
   */
  uint64_t currentFileOffset() { return fileOffset + parseOffset; }

  /**
   * @brief Reads a 32-bit big-endian unsigned integer from a buffer.
   * @param p Pointer to buffer.
   * @return 32-bit unsigned integer.
   */
  static uint32_t readU32(const uint8_t* p) {
    return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
  }

  /**
   * @brief Reads a 64-bit big-endian unsigned integer from a buffer.
   * @param p Pointer to buffer.
   * @return 64-bit unsigned integer.
   */
  static uint64_t readU64(const uint8_t* p) {
    return ((uint64_t)readU32(p) << 32) | readU32(p + 4);
  }


  /**
   * @brief Pops levels from the stack if we've passed their bounds.
   */
  void popLevels() {
    // Pop levels if we've passed their bounds (absolute file offset)
    while (!levelStack.empty() &&
           (fileOffset + parseOffset) >= levelStack.back()) {
      levelStack.pop_back();
    }
  }

  /**
   * @brief Processes the callback for a box.
   * Calls the type-specific callback if present, and the generic callback if
   * allowed.
   * @param box The box being processed.
   */
  void processCallback(Box& box) {
    bool is_called = false;
    bool call_generic = true;
    for (const auto& entry : callbacks) {
      if (strncmp(entry.type, box.type, 4) == 0) {
        entry.cb(box, ref);
        is_called = true;
        if (!entry.callGeneric) call_generic = false;
      }
    }
    /// call generic callback if allowed
    if ((!is_called || call_generic) && callback) callback(box, ref);
  }

  /**
   * @brief Checks if a box type is a container box.
   * @param type Box type string.
   * @return true if container box, false otherwise.
   */
  bool isContainerBox(const char* type) {
    // fill with default values if nothing has been defined
    if (containers.empty()) {
      // pure containers
      static const char* containers_str[] = {
          "moov", "trak", "mdia", "minf", "stbl", "edts", "dinf", "udta",
          "ilst", "moof", "traf", "mfra", "tref", "iprp", "sinf", "schi"};
      for (const char* c : containers_str) {
        ContainerInfo info;
        info.name = c;
        info.start = 0;
        containers.push_back(info);
      }
      // container with data
      ContainerInfo info;
      info.name = "meta";
      info.start = 4;  // 4 bytes: version (1 byte) + flags (3 bytes)
      containers.push_back(info);
    }
    // find the container by name
    for (auto& cont : containers) {
      if (StrView(type) == cont.name) return true;
    }
    return false;
  }

  /**
   * @brief Gets the start offset for a subcontainer.
   * @param type Box type string.
   * @return Offset of the subcontainer.
   */
  int getContainerDataLength(const char* type) {
    for (auto& cont : containers) {
      if (StrView(type) == cont.name) return cont.start;
    }
    return 0;
  }

  /**
   * @brief Checks if a type string is a valid 4-character box type.
   * @param type Pointer to type string.
   * @param offset Offset in the string.
   * @return true if valid, false otherwise.
   */
  bool isValidType(const char* type, int offset = 0) const {
    // Check if the type is a valid 4-character string
    return (type != nullptr && isalnum(type[offset]) &&
            isalnum(type[offset + 1]) && isalnum(type[offset + 2]) &&
            isalnum(type[offset + 3]));
  }

  /**
   * @brief Checks and adjusts the parse offset for valid box types.
   * @return Adjusted parse offset.
   */
  size_t checkParseOffset() {
    size_t current = parseOffset;
    const char* type = (char*)(buffer.data() + parseOffset + 4);
    for (int j = 0; j < buffer.available() - parseOffset - 4; j += 4) {
      if (isValidType(type, j)) {
        if (j != 0) {
          // report the data under the last valid box
          box.size = 0;
          box.data_size = j;
          box.level = static_cast<int>(levelStack.size()) + 1;
          box.data = buffer.data() + parseOffset;
          processCallback(box);
        }

        return j + parseOffset;
      }
    }
    return parseOffset;
  }
};

}  // namespace audio_tools
