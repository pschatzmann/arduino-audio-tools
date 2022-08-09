#pragma once
#include "FS.h"

/**
 * @brief We store all the relevant file names in an sequential index
 * file
 */
template<class SD, class File>
class AudioSourceIndex {
  public:
    AudioSourceIndex(SD &sd) {
        p_sd = &sd;
    };
    void begin(bool setupIndex, const char *startDir, const char *extension,
               const char *file_name_pattern) {
      this->ext = extension;
      this->file_name_pattern = file_name_pattern;
      idx_path = String(startDir)+"/idx.txt";
      idx_defpath = String(startDir)+"/idx-def.txt";
      int idx_file_size = indexFileSize();
      LOGI("Index file size: %d", idx_file_size);
      String keyNew = String(startDir) + "|" + extension + "|" + file_name_pattern;
      String keyOld = getIndexDef();
      if (setupIndex && (keyNew != keyOld || idx_file_size==0)) {
        File idxfile = p_sd->open(idx_path.c_str(), FILE_WRITE);
        LOGW("Creating index file");
        listDir(idxfile, startDir);
        LOGI("Indexing completed");
        idxfile.close();
        // update index definition file
        saveIndexDef(keyNew);
      } 
    }

    /// Access file name by index
    const char *operator[](int idx) {
      // return null when inx too big
      if (size>=0 && idx>=size) {
        LOGE("idx %d > size %d", idx, size);
        return nullptr;
      }
      // find record
      File idxfile = p_sd->open(idx_path.c_str());
      int count=0;

      if (idxfile.available()==0){
        LOGE("Index file is empty");
      }

      bool found = false;
      while (idxfile.available()>0 && !found) {
        result = idxfile.readStringUntil('\n');
        LOGD("%d -> %s", count, result.c_str());
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
    String idx_path;
    String idx_defpath;
    SD *p_sd = nullptr;
    
    const char *ext = nullptr;
    const char *file_name_pattern = nullptr;
    long size=-1;

    /// Writes the index file
    void listDir(File &idxfile, const char *dirname) {
      File root = p_sd->open(dirname);
      if (!root) {
        return;
      }
      if (!root.isDirectory()) {
        return;
      }

      File file = root.openNextFile();
      while (file) {
        if (file.isDirectory()) {
          listDir(idxfile, file.name());
        } else {
          if (isValidAudioFile(file)) {
            LOGI("Adding file to index: %s", file.name());
            idxfile.println(file.name());
          }
        }
        file = root.openNextFile();
      }
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
      File idxdef = p_sd->open(idx_defpath.c_str());
      String key1 = idxdef.readString();
      idxdef.close();
      return key1;
    }
    void saveIndexDef(String keyNew){
        File idxdef = p_sd->open(idx_defpath.c_str(), FILE_WRITE);
        idxdef.write((const uint8_t *)keyNew.c_str(), keyNew.length());
        idxdef.close();
    }

    size_t indexFileSize() {
        File idxfile = p_sd->open(idx_path.c_str());
        size_t result = idxfile.size();
        idxfile.close();
        return result;
    }
};