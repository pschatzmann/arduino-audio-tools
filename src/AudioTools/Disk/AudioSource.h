#pragma once
#include "AudioTools/CoreAudio/AudioBasic/Collections/Vector.h"
#include "AudioTools/CoreAudio/AudioBasic/Str.h"
#include "AudioTools/CoreAudio/AudioMetaData/AbstractMetaData.h"

namespace audio_tools {

/**
 * @brief Abstract Audio Data Source for the AudioPlayer which is used by the
 * Audio Players
 * @ingroup player
 * @author Phil Schatzmann
 * @copyright GPLv3
 *
 */
class AudioSource {
 public:
  /// Reset actual stream and move to root
  virtual bool begin() = 0;

  /// Returns next audio stream
  virtual Stream* nextStream(int offset) = 0;

  /// Returns previous audio stream
  virtual Stream* previousStream(int offset) { return nextStream(-offset); };

  /// Returns audio stream at the indicated index (the index is zero based, so
  /// the first value is 0!)
  virtual Stream* selectStream(int index) {
    LOGE("Not Supported!");
    return nullptr;
  }

  /// same as selectStream - I just prefer this name
  virtual Stream* setIndex(int index) { return selectStream(index); }

  /// Returns the actual index of the stream
  virtual int index() { return -1; }

  /// Returns audio stream by path: The index is not changed!
  virtual Stream* selectStream(const char* path) = 0;

  /// Sets the timeout which is triggering to move to the next stream. - the
  /// default value is 500 ms
  virtual void setTimeoutAutoNext(int millisec) {
    timeout_auto_next_value = millisec;
  }

  /// Provides the timeout which is triggering to move to the next stream.
  virtual int timeoutAutoNext() { return timeout_auto_next_value; }

  // only the ICYStream supports this
  virtual bool setMetadataCallback(void (*fn)(MetaDataType info,
                                              const char* str, int len),
                                   ID3TypeSelection sel = SELECT_ICY) {
    return false;
  }

  /// Sets the timeout of Stream in milliseconds
  virtual void setTimeout(int millisec) {};

  /// Returns default setting go to the next
  virtual bool isAutoNext() { return true; }

  /// access with array syntax
  Stream* operator[](int idx) { return setIndex(idx); }

  /// provides the actual stream (e.g. file) name or url
  virtual const char* toStr() { return nullptr; }

 protected:
  int timeout_auto_next_value = 500;
};

/**
 * @brief Callback Audio Data Source which is used by the Audio Players
 * @ingroup player
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AudioSourceCallback : public AudioSource {
 public:
  AudioSourceCallback() {}

  AudioSourceCallback(Stream* (*nextStreamCallback)(int offset),
                      void (*onStartCallback)() = nullptr) {
    TRACED();
    this->onStartCallback = onStartCallback;
    this->nextStreamCallback = nextStreamCallback;
  }

  /// Reset actual stream and move to root
  virtual bool begin() override {
    TRACED();
    if (onStartCallback != nullptr) onStartCallback();
    return true;
  };

  /// Returns next (with positive index) or previous stream (with negative
  /// index)
  virtual Stream* nextStream(int offset) override {
    TRACED();
    return nextStreamCallback == nullptr ? nullptr : nextStreamCallback(offset);
  }

  /// Returns selected audio stream
  virtual Stream* selectStream(int index) override {
    LOGI("selectStream: %d", index);
    if (indexStreamCallback == nullptr) {
      LOGI("setCallbackSelectStream not provided");
      if (index > 0) {
        begin();
        return nextStream(index);
      } else {
        // nextStream(0) will return the directory but we need a file
        return nextStream(1);
      }
    }
    return indexStreamCallback(index);
  }
  /// Returns audio stream by path
  virtual Stream* selectStream(const char* path) override {
    this->path = path;
    return indexStreamCallback == nullptr ? nullptr : indexStreamCallback(-1);
  };

  void setCallbackOnStart(void (*callback)()) { onStartCallback = callback; }

  void setCallbackNextStream(Stream* (*callback)(int offset)) {
    nextStreamCallback = callback;
  }

  void setCallbackSelectStream(Stream* (*callback)(int idx)) {
    indexStreamCallback = callback;
  }

  virtual bool isAutoNext() override { return auto_next; }

  virtual void setAutoNext(bool a) { auto_next = a; }

  /// Returns the requested path: relevant when provided idx in callback is -1
  virtual const char* getPath() { return path; }

 protected:
  void (*onStartCallback)() = nullptr;
  bool auto_next = true;
  Stream* (*nextStreamCallback)(int offset) = nullptr;
  Stream* (*indexStreamCallback)(int index) = nullptr;
  const char* path = nullptr;
};

/**
 * @brief File entry to minimize RAM usage by using path index and name
 */
struct FileEntry {
  int path_index;  // Index into shared path registry
  Str name;

  FileEntry() : path_index(-1) {}

  FileEntry(int pathIdx, const char* fileName) : path_index(pathIdx) {
    name.set(fileName);
  }
};

/**
 * @brief Interface for classes that can collect file names
 */
class PathNamesRegistry {
 public:
  virtual void addName(const char* nameWithPath) = 0;
};

/**
 * @brief Print class that calls addName for each printed line.
 * Useful for collecting file names from text output (e.g., directory listings).
 */

class NamePrinter : public Print {
 public:
  NamePrinter(PathNamesRegistry& dataSource, const char* prefix = nullptr)
      : dataSource(dataSource), prefix(prefix) {}
  void setPrefix(const char* prefix) { this->prefix = prefix; }
  size_t write(uint8_t ch) override {
    if (ch == '\n' || ch == '\r') {
      // End of line - process the accumulated line
      if (line_buffer.length() > 0) {
        line_buffer.trim();
        if (prefix != nullptr) {
          // Prepend prefix if set
          Str name{prefix};
          name.add("/");
          name.add(line_buffer.c_str());
          LOGD("adding '%s'", name.c_str());
          dataSource.addName(name.c_str());
        } else {
          // Add line as is
          LOGD("adding '%s'", line_buffer.c_str());
          dataSource.addName(line_buffer.c_str());
        }
        line_buffer.clear(false);
      }
      return 1;
    } else {
      // Accumulate characters
      line_buffer.add((char)ch);
      return 1;
    }
  }

  size_t write(const uint8_t* buffer, size_t size) override {
    for (size_t i = 0; i < size; i++) {
      write(buffer[i]);
    }
    return size;
  }

  /// Flush any remaining content in the line buffer
  void flush() override {
    if (line_buffer.length() > 0) {
      dataSource.addName(line_buffer.c_str());
      line_buffer.clear();
    }
  }

 private:
  PathNamesRegistry& dataSource;
  Str line_buffer{200};  // Buffer to accumulate characters for each line
  const char* prefix = nullptr;
};

/**
 * @brief Flexible Audio Data Source using a Vector of (file) names with minimal RAM
 * usage. Files are stored with separated path index and name to minimize memory
 * consumption. Identical paths are stored only once in a shared path registry.
 * This class is a template to support multiple SD libraries and other Streams.
 *
 * @note A mandatory callback function must be provided to convert a file path (and optionally an old file instance) into a stream or file object. This callback is set via the constructor or setNameToStreamCallback().
 *
 * Example callback signature:
 *   FileType* callback(const char* path, FileType& oldFile);
 *
 * Without this callback, the class cannot open or access files for playback. Also 
 * don't forget to close the old file to prevent any memory leaks.
 *
 * @ingroup player
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
template <typename FileType>
class AudioSourceVector : public AudioSource, public PathNamesRegistry {
 public:
  typedef FileType* (*FileToStreamCallback)(const char* path,
                                            FileType& oldFile);

  AudioSourceVector() = default;

  /// Constructor with callback for file to stream conversion
  AudioSourceVector(FileToStreamCallback callback)
      : nameToStreamCallback(callback) {}

  /// Reset actual stream and move to root
  bool begin() override {
    TRACED();
    current_index = 0;
    current_stream = nullptr;
    return true;
  }

  /// Returns next audio stream
  virtual FileType* nextStream(int offset) override {
    TRACED();
    if (files.empty()) return nullptr;

    current_index += offset;
    // Wrap around if out of bounds
    if (current_index < 0) {
      current_index = files.size() - 1;
    } else if (current_index >= (int)files.size()) {
      current_index = 0;
    }

    return selectStream(current_index);
  }

  /// Returns audio stream at the indicated index
  virtual FileType* selectStream(int index) override {
    TRACED();
    if (files.empty() || index < 0 || index >= (int)files.size()) {
      LOGE("Invalid index: %d (size: %d)", index, files.size());
      return nullptr;
    }

    current_index = index;
    Str fullPath = getFullPath(index);
    LOGI("selectStream: %d -> %s", index, fullPath.c_str());

    if (nameToStreamCallback) {
      current_stream = nameToStreamCallback(fullPath.c_str(), getCurrentFile());
      return current_stream;
    }

    LOGE("No file to stream callback set!");
    return nullptr;
  }

  /// Returns audio stream by path
  virtual FileType* selectStream(const char* path) override {
    TRACED();
    if (path == nullptr) return nullptr;

    int idx = indexOf(path);
    if (idx >= 0) {
      return selectStream(idx);
    }

    LOGE("File not found: %s", path);
    return nullptr;
  }

  /// Returns the actual index of the stream
  virtual int index() override { return current_index; }

  /// Find index of file by path
  int indexOf(const char* path) {
    if (path == nullptr) return -1;

    // Find the file by path
    for (int i = 0; i < (int)files.size(); i++) {
      Str fullPath = getFullPath(i);
      if (fullPath.equals(path)) {
        return i;
      }
    }
    return -1;
  }

  /// Add a file with full path (path and name will be separated automatically)
  void addName(const char* nameWithPath) override {
    TRACED();
    if (nameWithPath == nullptr) return;
    LOGI("addName: '%s'", nameWithPath);

    // Split path and name
    StrView nameWithPathStr(nameWithPath);
    int lastSlashPos = nameWithPathStr.lastIndexOf("/");
    int len = nameWithPathStr.length();

    Str pathStr;
    Str nameStr;
    if (lastSlashPos < 0) {
      // No path, just name
      pathStr.set("");
      nameStr.set(nameWithPath);
    } else {
      // Split into path and name
      pathStr.substring(nameWithPath, 0, lastSlashPos);
      nameStr.substring(nameWithPath, lastSlashPos + 1, len);
    }

    // Find or add path to registry
    int pathIndex = findOrAddPath(pathStr.c_str());

    // Create file entry with path index
    FileEntry entry{pathIndex, nameStr.c_str()};
    files.push_back(entry);

  }

  /// Remove a file by full path
  bool deleteName(const char* nameWithPath) {
    TRACED();
    if (nameWithPath == nullptr) return false;
    
    int idx = indexOf(nameWithPath);
    if (idx >= 0) {
      LOGI("deleteName: '%s' at index %d", nameWithPath, idx);
      return deleteIndex(idx);
    }
    
    LOGW("deleteName: File not found: '%s'", nameWithPath);
    return false;
  }

  /// Remove a file by index
  bool deleteIndex(size_t idx) {
    TRACED();
    if (idx >= files.size()) {
      LOGW("deleteIndex: Invalid index: %d (size: %d)", (int)idx, files.size());
      return false;
    }
    
    LOGI("deleteIndex: Removing file at index %d", (int)idx);
    files.erase(files.begin() + idx);
    
    // Adjust current_index if necessary
    if (current_index >= (int)idx) {
      current_index--;
      if (current_index < 0 && !files.empty()) {
        current_index = 0;
      }
    }
    
    return true;
  }

  /// Add multiple files at once
  template <typename T, size_t N>
  void addNames(T (&nameArray)[N]) {
    for (size_t i = 0; i < N; i++) {
      addName(nameArray[i]);
    }
  }

  /// Clear all files and path registry
  void clear() {
    files.clear();
    path_registry.clear();
    current_index = 0;
    current_stream = nullptr;
  }

  /// Get the number of files
  int size() { return files.size(); }

  /// Check if empty
  bool isEmpty() { return files.empty(); }

  /// Set the callback for converting file path to stream
  void setNameToStreamCallback(FileToStreamCallback callback) {
    nameToStreamCallback = callback;
  }

  /// Get the current file reference for use in callback
  FileType& getCurrentFile() {
    if (current_stream) {
      return *current_stream;
    }
    static FileType empty;
    return empty;
  }

  /// provides the actual stream (e.g. file) name or url
  virtual const char* toStr() override {
    if (current_index >= 0 && current_index < (int)files.size()) {
      current_path = getFullPath(current_index);
      return current_path.c_str();
    }
    return nullptr;
  }

  /// provides the name at the given index
  const char* name(int index) { return getFullPath(index).c_str(); }

 protected:
  Vector<FileEntry> files;    // List of all files
  Vector<Str> path_registry;  // Shared registry of unique paths
  int current_index = 0;
  FileType* current_stream = nullptr;
  FileToStreamCallback nameToStreamCallback = nullptr;
  Str current_path;  // Cache for toStr() method

  /// Find existing path in registry or add new one, returns index
  int findOrAddPath(const char* path) {
    if (path == nullptr) path = "";

    // Search for existing path
    for (int i = 0; i < (int)path_registry.size(); i++) {
      if (strcmp(path_registry[i].c_str(), path) == 0) {
        return i;  // Found existing path
      }
    }

    // Path not found, add new one
    Str newPath;
    newPath.set(path);
    path_registry.push_back(newPath);
    return path_registry.size() - 1;
  }

  /// Get file entry at index
  FileEntry& getFileEntry(int index) const { return files[index]; }

  /// Get full path of file at index
  Str getFullPath(int index) {
    if (index < 0 || index >= (int)files.size()) {
      return Str();
    }

    FileEntry& entry = getFileEntry(index);

    // Get path
    Str result;
    if (entry.path_index >= 0 && entry.path_index < (int)path_registry.size()) {
      result = path_registry[entry.path_index].c_str();
    }
    // delimite with /
    if (!result.endsWith("/") && !entry.name.startsWith("/")) {
      result.add("/");
    }
    // add name
    result.add(entry.name.c_str());

    return result;
  }

  /// Get path from registry at index
  const char* getPath(int pathIndex) {
    if (pathIndex >= 0 && pathIndex < (int)path_registry.size()) {
      return path_registry[pathIndex].c_str();
    }
    return "";
  }
};

/**
 * @brief Audio Data Source managing a static array of file names (const char*).
 * Designed for PROGMEM storage on Arduino platforms with static file lists.
 * This class is a template to support multiple SD libraries and other Streams.
 *
 * @note A mandatory callback function must be provided to convert a file path (and optionally an old file instance) into a stream or file object. This callback is set via the constructor or setNameToStreamCallback().
 *
 * Example callback signature:
 *   FileType* callback(const char* path, FileType& oldFile);
 *
 * Without this callback, the class cannot open or access files for playback. Also 
 * don't forget to close the old file to prevent any memory leaks.
 *
 * @ingroup player
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
template <typename FileType>
class AudioSourceArray : public AudioSource {
 public:
  typedef FileType* (*FileToStreamCallback)(const char* path, FileType& file);

  AudioSourceArray() = default;

  /// Constructor with array and callback
  template <size_t N>
  AudioSourceArray(const char* (&nameArray)[N], FileToStreamCallback callback)
      : file_array(nameArray), array_size(N), nameToStreamCallback(callback) {}

  /// Constructor with array pointer, size and callback
  AudioSourceArray(const char* const* nameArray, size_t size,
                   FileToStreamCallback callback)
      : file_array(nameArray),
        array_size(size),
        nameToStreamCallback(callback) {}

  /// Reset actual stream and move to root
  virtual bool begin() override {
    TRACED();
    current_index = 0;
    current_stream = nullptr;
    return true;
  }

  /// Returns next audio stream
  virtual FileType* nextStream(int offset) override {
    TRACED();
    if (array_size == 0) return nullptr;

    current_index += offset;
    // Wrap around if out of bounds
    if (current_index < 0) {
      current_index = array_size - 1;
    } else if (current_index >= (int)array_size) {
      current_index = 0;
    }

    return selectStream(current_index);
  }

  /// Returns audio stream at the indicated index
  virtual FileType* selectStream(int index) override {
    TRACED();
    if (array_size == 0 || index < 0 || index >= (int)array_size) {
      LOGE("Invalid index: %d (size: %d)", index, array_size);
      return nullptr;
    }

    current_index = index;
    const char* filePath = file_array[index];
    LOGI("selectStream: %d -> %s", index, filePath);

    if (nameToStreamCallback && filePath) {
      current_stream = nameToStreamCallback(filePath, getCurrentFile());
      return current_stream;
    }

    LOGE("No file to stream callback set or invalid file path!");
    return nullptr;
  }

  /// Returns audio stream by path
  virtual FileType* selectStream(const char* path) override {
    TRACED();
    if (path == nullptr) return nullptr;

    int idx = indexOf(path);
    if (idx >= 0) {
      return selectStream(idx);
    }

    LOGE("File not found: %s", path);
    return nullptr;
  }

  /// Returns the actual index of the stream
  virtual int index() override { return current_index; }

  /// Find index of file by path
  int indexOf(const char* path) {
    if (path == nullptr) return -1;

    // Find the file by path
    for (int i = 0; i < (int)array_size; i++) {
      const char* filePath = file_array[i];
      if (filePath && StrView(path).equals(filePath)) {
        return i;
      }
    }
    return -1;
  }

  /// Set the array of file names
  template <size_t N>
  void setArray(const char* (&nameArray)[N]) {
    file_array = nameArray;
    array_size = N;
    current_index = 0;
  }

  /// Set the array with pointer and size
  void setArray(const char* const* nameArray, size_t size) {
    file_array = nameArray;
    array_size = size;
    current_index = 0;
  }

  /// Get the number of files
  int size() const { return array_size; }

  /// Check if empty
  bool isEmpty() const { return array_size == 0; }

  /// Set the callback for converting file path to stream
  void setNameToStreamCallback(FileToStreamCallback callback) {
    nameToStreamCallback = callback;
  }

  FileType& getCurrentFile() {
    if (current_stream) {
      return *current_stream;
    }
    static FileType empty;
    return empty;
  }

  /// Get file path at index
  const char* getFilePath(int index) const {
    if (index >= 0 && index < (int)array_size && file_array) {
      return file_array[index];
    }
    return nullptr;
  }

  /// provides the actual stream (e.g. file) name or url
  virtual const char* toStr() override {
    if (current_index >= 0 && current_index < (int)array_size && file_array) {
      return file_array[current_index];
    }
    return nullptr;
  }

  /// provides the name at the given index
  const char* name(int index) { return getFilePath(index); }

 protected:
  const char* const* file_array = nullptr;  // Pointer to array of const char*
  size_t array_size = 0;
  int current_index = 0;
  FileType* current_stream = nullptr;
  FileToStreamCallback nameToStreamCallback = nullptr;
};

}  // namespace audio_tools