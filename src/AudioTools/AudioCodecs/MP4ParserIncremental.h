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
  using DataCallback = std::function<void(
      const Box&, const uint8_t* data, size_t len, bool is_final, void* ref)>;

  /// Defines the calback for large incremental data
  void setDataCallback(DataCallback cb) { data_callback = cb; }

 protected:
  DataCallback data_callback = defaultDataCallback;
  bool box_in_progress = false;
  size_t box_bytes_received = 0;
  size_t box_bytes_expected = 0;
  char box_type[5] = {0};
  int box_level = 0;
  uint64_t box_offset = 0;

  /// Just prints the box name and the number of bytes received
  static void defaultDataCallback(const Box& box, const uint8_t* data,
                                  size_t len, bool is_final, void* ref) {
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

  /// Try to start parsing a new box. Returns false if not enough data.
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
    processCallback();

    uint64_t absBoxOffset = fileOffset + parseOffset;
    levelStack.push_back(absBoxOffset + boxSize);
    parseOffset += 8;
  }

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
    processCallback();
  }

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
        processCallback();

        // incremental callback
        box.size = box_bytes_expected;
        box.data_size = available_payload;
        data_callback(box, p + headerSize, available_payload, false, ref);
      }
    }
    fileOffset += (bufferSize - buffer.available());
    buffer.clear();
    parseOffset = 0;
  }

  /// Continue filling an incremental box. Returns false if not enough data.
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
      data_callback(box, buffer.data(), to_read, box.is_complete, ref);
    }
    box_bytes_received += to_read;
    fileOffset += to_read;
    buffer.clearArray(to_read);

    if (box_bytes_received >= box_bytes_expected) {
      box_in_progress = false;
    }
    return true;
  }

  void finalizeParse() {
    if (parseOffset > 0) {
      fileOffset += parseOffset;
      buffer.clearArray(parseOffset);
      parseOffset = 0;
    }
  }
};

}  // namespace audio_tools
