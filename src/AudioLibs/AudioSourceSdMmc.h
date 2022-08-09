#pragma once

#include "AudioBasic/StrExt.h"
#include "AudioLogger.h"
#include "AudioTools/AudioSource.h"
#include "FS.h"
#include "SD_MMC.h"

#define MAX_FILE_LEN 256

namespace audio_tools {

/**
 * @brief ESP32 AudioSource for AudioPlayer using an SD card as data source.
 * This class is based on the Arduino SD_MMC implementation
 * Connect the SD card to the following pins:
 *
 * SD Card | ESP32
 *    D2       12
 *    D3       13
 *    CMD      15
 *    VSS      GND
 *    VDD      3.3V
 *    CLK      14
 *    VSS      GND
 *    D0       2  (add 1K pull up after flashing)
 *    D1       4
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AudioSourceSdMmc : public AudioSource {
protected:
  /**
   * @brief We store all the relevant file names in an sequential index
   * file
   */
  class MMCFileIndex {
  public:
    MMCFileIndex() = default;
    void begin(bool setupIndex, const char *startDir, const char *extension,
               const char *file_name_pattern) {
      this->ext = extension;
      this->file_name_pattern = file_name_pattern;
      Stirng path = String(startDir)+"/idx.txt";
      idxfile = SD_MMC.open(path.c_str(), FILE_WRITE);

      String keyNew = String(startDir) + "|" + extension + "|" + file_name_pattern;
      String keyOld = getIndexDef();
      if (setupIndex && keyNew != keyOld) {
        LOGW("Creating index file");
        listDir(startDir);
        // update index definition file
        saveIndexDef(keyNew);
      } 
    }

    /// Access file name by index
    const char *operator[](int idx) {
      // return null when inx too big
      if (count>=0 && idx>=count) return nullptr;

      // find record
      File idxfile = SD_MMC.open("/idx.txt");
      int count=0;
      bool found = false;
      while (idxfile.available()&&!found) {
        result = idxfile.readStringUntil('\n');
        if (count==idx) {
          found=true;
        }
        count++;
      }
      if (!found){
        size = count;
      }
      idxfile.close();

      return found ? result.c_str(): nullptr;
    }

  protected:
    String result;
    File idxfile;
    const char *ext = nullptr;
    const char *file_name_pattern = nullptr;
    size_t size=-1;

    /// Writes the index file
    void listDir(const char *dirname) {
      File root = SD_MMC.open(dirname);
      if (!root) {
        return;
      }
      if (!root.isDirectory()) {
        return;
      }

      File file = root.openNextFile();
      while (file) {
        if (file.isDirectory()) {
          listDir(file.name());
        } else {
          if (isValidAudioFile(file)) {
            LOGI("Adding file to index: %s", file.name());
            idxfile.println(file.name());
          }
        }
        file = root.openNextFile();
      }
      idxfile.close();
    }

    /// checks if the file is a valid audio file
    bool isValidAudioFile(File &file) {
      const char *file_name = file.name();
      if (file.isDirectory()) {
        LOGD("-> isValidAudioFile: '%s': %d", file_name, false);
        return false;
      }
      Str strFileName(file_name);
      bool result = strFileName.endsWithIgnoreCase(ext) &&
                    strFileName.matches(file_name_pattern);
      LOGD("-> isValidAudioFile: '%s': %d", file_name, result);
      return result;
    }

    String getIndexDef() {
      File idxdef = SD_MMC.open("/idx-def.txt");
      String key1 = idxdef.readString();
      idxdef.close();
      return key1;
    }
    void saveIndexDef(String keyNew){
        File idxdef = SD_MMC.open("/idx-def.txt", FILE_WRITE);
        idxdef.write((const uint8_t *)keyNew.c_str(), keyNew.length());
        idxdef.close();
    }
  };

public:
  /// Default constructor
  AudioSourceSdMmc(const char *startFilePath = "/", const char *ext = ".mp3", bool setupIndex=true) {
    start_path = startFilePath;
    exension = ext;
    setup_index = setupIndex;
  }

  virtual void begin() override {
    LOGD(LOG_METHOD);
    static bool is_sd_setup = false;
    if (!is_sd_setup) {
      if (!SD_MMC.begin()) {
        LOGE("SD_MMC.begin failed");
        return;
      }
      is_sd_setup = true;
    }
    idx.begin(setup_index, start_path, exension, file_name_pattern);
    idx_pos = 0;
  }

  virtual Stream *nextStream(int offset = 1) override {
    LOGW("nextStream: %d", offset);
    return selectStream(idx_pos + offset);
  }

  virtual Stream *selectStream(int index) override {
    LOGW("selectStream: %d", index);
    idx_pos = index;
    file_name = idx[index];
    if (file_name==nullptr) return nullptr;
    LOGI("Using file %s", file_name);
    file = SD_MMC.open(file_name);
    return file ? &file : nullptr;
  }

  virtual Stream *selectStream(const char *path) override {
    file.close();
    file = SD_MMC.open(path);
    file_name = file.name();
    LOGW("-> selectStream: %s", path);
    return file ? &file : nullptr;
  }

  /// Defines the regex filter criteria for selecting files. E.g. ".*Bob
  /// Dylan.*"
  void setFileFilter(const char *filter) { file_name_pattern = filter; }

  /// Provides the current index position
  int index() { return idx_pos; }

  /// provides the actual file name
  const char *toStr() { return file_name; }

  // provides default setting go to the next
  virtual bool isAutoNext() { return true; }

  /// Allows to "correct" the start path if not defined in the constructor
  virtual void setPath(const char *p) { start_path = p; }

protected:
  File file;
  MMCFileIndex idx;
  size_t idx_pos = 0;
  const char *file_name;
  const char *exension = nullptr;
  const char *start_path = nullptr;
  const char *file_name_pattern = "*";
  bool setup_index = true;
};

} // namespace audio_tools