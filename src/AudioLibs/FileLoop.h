#pragma once

#include "AudioTools/AudioStreams.h"
#ifdef ARDUINO
#  include "FS.h"
#  define READTYPE char
#else 
#  define READTYPE uint8_t
#endif
namespace audio_tools {

/**
 * @brief A simple class which implements a automatic looping file.
 * In order to support different file implementation the file class
 * is a template parameter. The number of loops can be defined by
 * calling setLoopCount().
 * You can also optinally limit the total looping file size by calling 
 * setSize();
 * @ingroup io
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
template <class FileType> class FileLoopT : public AudioStream {
public:
  FileLoopT() = default;
  FileLoopT(FileType file, int count = -1, int rewindPos = 0) {
    setFile(file);
    setLoopCount(count);
    setStartPos(rewindPos);
  }

  // restarts the file from the beginning
  bool begin() override {
    TRACEI();
    current_file.seek(start_pos);
    size_open = total_size;
    return current_file;
  }

  // closes the file
  void end() override { 
    TRACEI();
    current_file.close(); 
  }

  /// defines the file that is used for looping
  void setFile(FileType file) { this->current_file = file; }

  /// Returns the file
  FileType &file(){
    return current_file;
  }

  /// defines the start position after the rewind. E.g. for wav files this should be 44
  void setStartPos(int pos) { start_pos = pos; }

  /// optionally defines the requested playing size in bytes
  void setSize(size_t len) {
    total_size = len;
  }

  /// Returns the (requested) file size
  size_t size() {
    return total_size == -1 ? current_file.size() : total_size;
  }

  /// You can be notified about a rewind 
  void setCallback(void (*cb)(FileLoopT &loop)){
    callback = cb;
  }

  /// count values: 0) do not loop, 1) loop once, n) loop n times, -1) loop
  /// endless
  void setLoopCount(int count) { loop_count = count; }

  int available() override {
    // if we manage the size, we return the open amount
    if (total_size!=-1) return size_open;
    // otherwise we return DEFAULT_BUFFER_SIZE if looping is active
    return isLoopActive() ? DEFAULT_BUFFER_SIZE : current_file.available();
  }

  size_t readBytes(uint8_t *data, size_t len) override {
    LOGD("FileLoopT::readBytes %d at %d", (int)len, (int)current_file.position());
    if (!current_file)
      return 0;
    
    // limit the copy size if necessary
    int copy_len = len;
    if (total_size!=-1){
      copy_len = min((int)len, size_open);
    }

    // read step 1;
    int result1 = current_file.readBytes((READTYPE *)data, copy_len);
    int result2 = 0;
    int open = copy_len - result1;
    if (isLoopActive() && open>0) {
      LOGI("seek 0");
      // looping logic -> rewind to beginning: read step 2
      current_file.seek(start_pos);
      // notify user
      if (callback!=nullptr){
        callback(*this);
      }
      result1 = current_file.readBytes((READTYPE*)data + result1, open);
      if (loop_count>0)
        loop_count--;
    }
    // determine the result size
    int result = result1 + result2;
    // calculate the size_open if necessary
    if (total_size!=-1){
      size_open -= result;
    }
    return result;
  }

  // Returns the bool of the current file
  operator bool() {
    return current_file;
  }

  /// @return true as long as we are looping
  bool isLoopActive() { return loop_count > 0 || loop_count == -1; }

protected:
  int start_pos = 0;
  int loop_count = -1;
  int size_open = -1;
  int total_size = -1;
  void (*callback)(FileLoopT &loop) = nullptr;
  FileType current_file;
};

/**
 * @brief A simple class which implements a automatic looping file.
 * The file needs to be of the class File from FS.h. The number of loops can be
 * defined by calling setLoopCount().
 * You can also optinally limit the total looping file size by calling 
 * setSize();
 * @ingroup io
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class FileLoop : public FileLoopT<File> {
public:
  FileLoop() = default;
  FileLoop(File file, int count = -1, int rewindPos = 0)
      : FileLoopT<File>(file, count, rewindPos) {}
};

} // namespace audio_tools