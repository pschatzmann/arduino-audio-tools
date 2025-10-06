#pragma once

#include <SPI.h>
#include <SdFat.h>

#include "AudioToolsConfig.h"
#include "AudioLogger.h"
#include "AudioTools/Disk/AudioSource.h"

#define USE_SDFAT
#include "AudioTools/Disk/SDIndex.h"

// Try max SPI clock for an SD. Reduce SPI_CLOCK if errors occur. (40?)
#define SPI_CLOCK SD_SCK_MHZ(50)

namespace audio_tools {
/**
 * @brief ESP32 AudioSource for AudioPlayer using an SD card as data source.
 * An index file is used to speed up the access to the audio files by index.
 * This class is based on the Arduino SD implementation
 * For UTF8 Support change SdFatConfig.h #define USE_UTF8_LONG_NAMES 1
 * @param  <SdFat32, File32>, <SdFs, FsFile>, <SdExFat, ExFile>, <SdFat, File>
 * @ingroup player
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
template <typename AudioFs = SdFat32, typename AudioFile = File32>
class AudioSourceIdxSDFAT : public AudioSource {
 public:
  /// Default constructor
  AudioSourceIdxSDFAT(const char *startFilePath = "/", const char *ext = ".mp3",
                      int chipSelect = PIN_CS, int speedMHz = 10,
                      bool setupIndex = true) {
    TRACED();
    LOGI("SD chipSelect: %d", chipSelect);
    LOGI("SD speedMHz: %d", speedMHz);
    LOGI("ext: %s", ext);
    p_cfg = new SdSpiConfig(chipSelect, DEDICATED_SPI, SD_SCK_MHZ(speedMHz));
    owns_cfg = true;
    start_path = startFilePath;
    exension = ext;
    setup_index = setupIndex;
  }

  /// Costructor with SdSpiConfig
  AudioSourceIdxSDFAT(const char *startFilePath, const char *ext,
                      SdSpiConfig &config, bool setupIndex = true) {
    TRACED();
    p_cfg = &config;
    owns_cfg = false;
    start_path = startFilePath;
    exension = ext;
    setup_index = setupIndex;
  }

  /// Constructor for providing an open FS
  AudioSourceIdxSDFAT(AudioFs fs, const char *startFilePath="/", const char *ext="", bool setupIndex = true){
    TRACED();
    sd = fs;
    p_cfg = nullptr;
    owns_cfg = false;
    start_path = startFilePath;
    exension = ext;
    setup_index = setupIndex;
    is_sd_setup = true;
    // since we expect an open fs we do not close it
    is_close_sd = false;
  }

  virtual ~AudioSourceIdxSDFAT() { end(); }

  virtual bool begin() override {
    TRACED();
    if (!is_sd_setup) {
      if (!sd.begin(*p_cfg)) {
        LOGE("sd.begin failed");
        return false;
      }
      is_sd_setup = true;
    }
    idx.begin(start_path, exension, file_name_pattern, setup_index);
    idx_pos = 0;
    return is_sd_setup;
  }

  void end() {
    if (is_sd_setup) {
#ifdef ESP32
      if (is_close_sd) sd.end();
#endif
      if (owns_cfg) delete (p_cfg);
      is_sd_setup = false;
    }
  }

  virtual Stream *nextStream(int offset = 1) override {
    LOGI("nextStream: %d", offset);
    return selectStream(idx_pos + offset);
  }

  virtual Stream *selectStream(int index) override {
    LOGI("selectStream: %d", index);
    idx_pos = index;
    return selectStream(idx[index]);
  }

  virtual Stream *selectStream(const char *path) override {
    file.close();
    if (path == nullptr) {
      LOGE("Filename is null")
      return nullptr;
    }

    // AudioFile new_file;
    if (!file.open(path, O_RDONLY)) {
      LOGE("Open error: '%s'", path);
    }

    LOGI("-> selectStream: %s", path);
    // file = new_file;
    return &file;
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

  /// Provides the number of files (The max index is size()-1)
  long size() { return idx.size(); }

  /// Provides the index of the file with the given name
  int indexOf(const char* filename) {
    return idx.indexOf(filename);
  }

  /// Provides the file name for the indicated index
  const char* name(int pos) {
    return idx[pos];
  }

  /// Defines whether the index should be rebuild on begin
  void setCreateIndex(bool rebuild) {
    setup_index = rebuild;
  }

 protected:
  SdSpiConfig *p_cfg = nullptr;
  AudioFs sd;
  AudioFile file;
  SDIndex<AudioFs, AudioFile> idx{sd};
  size_t idx_pos = 0;
  char file_name[MAX_FILE_LEN];
  const char *exension = nullptr;
  const char *start_path = nullptr;
  const char *file_name_pattern = "*";
  bool setup_index = true;
  bool is_close_sd = true;
  bool is_sd_setup = false;
  int cs;
  bool owns_cfg = false;

  const char *getFileName(AudioFile &file) {
    static char name[MAX_FILE_LEN];
    file.getName(name, MAX_FILE_LEN);
    return name;
  }
};

}  // namespace audio_tools