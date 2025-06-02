#pragma once

#include <cstdint>
#include <cstring>
#include <functional>
#include <string>

#include "AudioTools/CoreAudio/Buffers.h"
#ifdef ARDUINO
#include "Serial.h"
#endif

namespace audio_tools {

/**
 * @brief MP4Parser is a class that parses MP4 files and extracts boxes (atoms).
 * It provides a callback mechanism to process each box as it is parsed. You can
 * define specific callbacks for individual box types or use a generic callback
 * for the undefined boxes: By default is just prints the box information to
 * Serial.
 * If a container box contains data, it will be processed recursively and if it
 * contains data itself, it will be reported in a second callback call.
 * @Note This parser expects that the buffer size is largen then the biggest
 * box!
 * @ingroup codecs
 * @author Phil Schatzmann
 */

class MP4Parser {
 public:
  /// An individual box in the MP4 file
  struct Box {
    friend class MP4Parser;     // Allow MP4Parser to access private members
    friend class MP4ParserExt;  // Allow MP4Parser to access private members
    size_t id = 0;
    char type[5];         // 4-character box type
    const uint8_t* data;  // Pointer to box payload (not including header)
    size_t data_size;
    size_t size;      // Size of payload (not including header)
    int level;        // Nesting depth
    uint64_t offset;  // File offset where box starts
    bool is_complete = false;
    bool is_container = false;

   private:
    // SingleBuffer<uint8_t> data_buffer;  // Buffer to hold box data if needed
  };

  using BoxCallback = std::function<void(const Box&, void* ref)>;

  /// Type specific callbacks
  struct CallbackEntry {
    char type[5];    // 4-character box type
    BoxCallback cb;  // Callback function
  };

  /// Defines an optional reference. By default it is the parser itself
  void setReference(void* ref) { this->ref = ref; }

  /// Defines the generic callback for all boxes
  void setCallback(BoxCallback cb) { callback = cb; }

  /// Defines a specific callback for a box type
  void setCallback(const char* type, BoxCallback cb) {
    CallbackEntry entry;
    strncpy(entry.type, type, 4);
    entry.type[4] = '\0';  // Ensure null-termination
    entry.cb = cb;
    callbacks.push_back(entry);
  };

  /// Defines a specific buffer size
  bool resize(size_t size) {
    buffer.resize(size);
    return buffer.size() == size;
  }

  /// Initializes the parser
  bool begin() {
    buffer.clear();
    if (buffer.size() == 0) buffer.resize(1024);
    parseOffset = 0;
    fileOffset = 0;
    levelStack.clear();
    box.is_complete = true;  // Start with no open box
    // box.data_buffer.reset();
    box.data = nullptr;
    box.size = 0;
    box.level = 0;
    box.offset = 0;
    box.id = 0;
    return true;
  }

  /// Provide the data to the parser (in chunks if needed)
  size_t write(const uint8_t* data, size_t len) {
    if (is_error) return len;  // If an error occurred, skip writing
    size_t result = buffer.writeArray(data, len);
    parse();
    return result;
  }

  /// Provide the data to the parser (in chunks if needed)
  size_t write(const char* data, size_t len) {
    return write(reinterpret_cast<const uint8_t*>(data), len);
  }

  int availableForWrite() { return buffer.availableForWrite(); }

 protected:
  BoxCallback callback = defaultCallback;
  Vector<CallbackEntry> callbacks;
  SingleBuffer<uint8_t> buffer;
  Vector<size_t> levelStack;
  size_t parseOffset = 0;
  uint64_t fileOffset = 0;
  void* ref = this;
  Box box;
  bool is_error = false;

  /// Returns the current file offset (absolute position in file)
  uint64_t currentFileOffset() { return fileOffset + parseOffset; }

  // Default callback that prints box information to Serial
  static void defaultCallback(const Box& box, void* ref) {
    char space[box.level * 2 + 1];
    memset(space, ' ', box.level * 2);
    space[box.level * 2] = '\0';  // Null-terminate the string
    char str_buffer[64];
    snprintf(str_buffer, sizeof(str_buffer),
             "- #%u %s, Offset: %u, Size: %u, Data Size: %u", (unsigned)box.id,
             box.type, (unsigned)box.offset, (unsigned)box.size,
             (unsigned)box.data_size);
#ifdef ARDUINO
    Serial.print(space);
    Serial.println(str_buffer);
#else
    printf("%s%s\n", space, str_buffer);
#endif
  }

  static uint32_t readU32(const uint8_t* p) {
    return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
  }

  static uint64_t readU64(const uint8_t* p) {
    return ((uint64_t)readU32(p) << 32) | readU32(p + 4);
  }
  virtual void parse() {
    while (true) {
      size_t bufferSize = buffer.available();
      // not enough data to parse a box header
      if (parseOffset + 8 > bufferSize) break;

      parseOffset = checkParseOffset();

      const uint8_t* p = buffer.data() + parseOffset;
      uint32_t size32 = readU32(p);
      char type[5];
      strncpy(type, (char*)(p + 4), 4);

      uint64_t boxSize = size32;
      size_t headerSize = 8;
      bool is_container = isContainerBox(type);

      // check data in buffer
      if (!is_container) {
        // not enough data to provide data
        if (boxSize > buffer.size()) {
          LOGE("MP4Parser: '%s' box size %u exceeds buffer size %u", type,
               (unsigned)boxSize, (unsigned)buffer.size());
          is_error = true;
          return;
        }
        // not enough data in buffer
        if (parseOffset + boxSize > bufferSize) break;
      }

      // fill box with data
      // Calculate absolute file offset for this box
      uint64_t absBoxOffset = fileOffset + parseOffset;
      int level = levelStack.size();

      strcpy(box.type, type);
      box.id++;
      box.data = p + headerSize;
      box.size = static_cast<size_t>(boxSize - headerSize);
      box.data_size = box.size;
      box.level = level;
      box.offset = fileOffset + parseOffset;
      box.is_complete = (parseOffset + boxSize <= bufferSize);
      box.is_container = is_container;

      // Special logic for container: no data
      if (box.is_container) {
        box.data = nullptr;
        box.data_size = 0;
        box.is_complete = true;
      }

      // Regular logic for box with complete data
      processCallback();

      // Recurse into container
      if (box.is_container) {
        levelStack.push_back(absBoxOffset + boxSize);
        parseOffset += headerSize;
        continue;
      }

      parseOffset += boxSize;

      popLevels();
    }

    if (parseOffset > 0) {
      fileOffset += parseOffset;
      buffer.clearArray(parseOffset);
      parseOffset = 0;
    }
  }

  void popLevels() {
    // Pop levels if we've passed their bounds (absolute file offset)
    while (!levelStack.empty() &&
           (fileOffset + parseOffset) >= levelStack.back()) {
      levelStack.pop_back();
    }
  }

  void processCallback() {
    bool is_called = false;
    for (const auto& entry : callbacks) {
      if (strncmp(entry.type, box.type, 4) == 0) {
        entry.cb(box, ref);
        is_called = true;
      }
    }
    /// call generic callback
    if (!is_called) callback(box, ref);
  }

  bool isContainerBox(const char* type) const {
    static const char* containers[] = {
        "moov", "trak", "mdia", "minf", "stbl", "edts", "dinf", "udta", "meta",
        "ilst", "moof", "traf", "mfra", "tref", "iprp", "sinf", "schi"};
    for (const char* c : containers)
      if (StrView(type) == c) return true;
    return false;
  }

  bool isPersistedBox(const char* type) const {
    static const char* persisted[] = {"stsz"};
    for (const char* p : persisted)
      if (StrView(type) == p) return true;
    return false;
  }

  size_t checkParseOffset() {
    size_t current = parseOffset;
    const char* type = (char*)(buffer.data() + parseOffset + 4);
    for (int j = 0; j < buffer.available() - parseOffset - 4; j += 4) {
      if (isalpha(type[j]) && isalpha(type[j + 1]) && isalpha(type[j + 2]) &&
          isalpha(type[j + 3])) {
        if (j != 0) {
          // report the data under the last valid box
          box.size = 0;
          box.data_size = j;
          box.level = static_cast<int>(levelStack.size()) + 1;
          ;
          box.data = buffer.data() + parseOffset;
          processCallback();
        }

        return j + parseOffset;
      }
    }
    return parseOffset;
  }
};

}  // namespace audio_tools
