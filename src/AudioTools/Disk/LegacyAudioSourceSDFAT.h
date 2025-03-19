#pragma once

#include "AudioLogger.h"
#include "AudioTools/Disk/AudioSource.h"
#include "AudioTools/CoreAudio/AudioBasic/Str.h"
#include <SPI.h>
#include <SdFat.h>
namespace audio_tools {

// SD_FAT_TYPE = 0 for SdFat/File as defined in SdFatConfig.h,
// 1 for FAT16/FAT32, 2 for exFAT, 3 for FAT16/FAT32 and exFAT.
#ifndef SD_FAT_TYPE
#define SD_FAT_TYPE 1
#endif
// Try max SPI clock for an SD. Reduce SPI_CLOCK if errors occur. (40?)
#define SPI_CLOCK SD_SCK_MHZ(50)
// Max file name length including directory path
#define MAX_FILE_LEN 256

#if defined(ARDUINO_ARCH_RP2040) && !defined(PICO)
    // only RP2040 from Earle Phil Hower is using the library with a sdfat namespace
    typedef sdfat::SdSpiConfig SdSpiConfig;
    typedef sdfat::SdFs AudioFs;
#else
#if SD_FAT_TYPE == 0
	typedef SdFat AudioFs;
	typedef File AudioFile;
#elif SD_FAT_TYPE == 1
	typedef SdFat32 AudioFs;
	typedef File32 AudioFile;
#elif SD_FAT_TYPE == 2
	typedef SdExFat AudioFs;
	typedef ExFile AudioFile;
#elif SD_FAT_TYPE == 3
	typedef SdFs AudioFs;
	typedef FsFile AudioFile;
#else  // SD_FAT_TYPE
#endif
#endif


/**
 * @brief AudioSource for AudioPlayer using an SD card as data source. 
 * This class is based on https://github.com/greiman/SdFat
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AudioSourceSDFAT : public AudioSource {
public:
    /// Default constructor
    AudioSourceSDFAT(const char* startFilePath = "/", const char* ext = ".mp3", int chipSelect = PIN_CS, int speedMHz = 10) {
        TRACED();
        LOGI("SD chipSelect: %d", chipSelect);
        LOGI("SD speedMHz: %d", speedMHz);
        LOGI("ext: %s", ext);
        p_cfg = new SdSpiConfig(chipSelect, DEDICATED_SPI, SD_SCK_MHZ(speedMHz));
        owns_cfg = true;
        start_path = startFilePath;
        exension = ext;
    }

    /// Costructor with SdSpiConfig
    AudioSourceSDFAT(const char* startFilePath, const char* ext, SdSpiConfig &config) {
        TRACED();
        p_cfg = &config;
        owns_cfg = false;
        start_path = startFilePath;
        exension = ext;
    }

    /// Destructor
    virtual ~AudioSourceSDFAT(){
        TRACED();
        if (p_cfg!=nullptr && owns_cfg){
            delete p_cfg;
        }
    }

    virtual void begin() override {
        TRACED();
        static bool is_sd_setup = false;
        if (!is_sd_setup){
            while (!sd.begin(*p_cfg)) {
                LOGE("SD.begin failed with cs=%d!", p_cfg->csPin);
                sd.initErrorHalt(&Serial);
                delay(500);
            }
            is_sd_setup = true;
        }
        idx_pos = 0;
    }

    virtual Stream* nextStream(int offset) override {
        LOGW("-> nextStream: %d", offset);
        return selectStream(idx_pos+offset);
    }

    virtual Stream* selectStream(int index) override {
        idx_pos = index<0 ? 0 : index;
        file.close();
        file = getFileByPos(start_path, idx_pos);
        file.getName(file_name, MAX_FILE_LEN);
        LOGW("-> selectStream: %d '%s'", idx_pos, file_name);
        return file ? &file : nullptr;
    }

    virtual Stream* selectStream(const char* path) override {
        file.close();
        file = getFileByPath(path);
        LOGW("-> selectStream: %s", path);
        return file ? &file : nullptr;
    }

    /// Defines the regex filter criteria for selecting files. E.g. ".*Bob Dylan.*" 
    void setFileFilter(const char* filter) {
        file_name_pattern = (char*)filter;
    }

    /// Provides the current index position
    int index() {
        return idx_pos;
    }

    /// provides the actual file name
    const char *toStr() {
        return file_name;
    }

    // provides default setting go to the next
    virtual bool isAutoNext() {
        return true;
    };

    /// Allows to "correct" the start path if not defined in the constructor
    virtual void setPath(const char* p) {
        start_path = p;
    }

    virtual void setTimeout(int ms){
        timeout = ms;
    }

protected:
    AudioFile file;
    SdSpiConfig *p_cfg = nullptr;
    bool owns_cfg=false;
    AudioFs sd;
    size_t idx_pos = 0;
    char file_name[MAX_FILE_LEN];
    const char* exension = nullptr;
    const char* start_path = nullptr;
    char* file_name_pattern = (char*) "*";
    int timeout;

    /// checks if the file is a valid audio file
    bool isValidAudioFile(AudioFile& file) {
        if (file.isDir()){
            LOGD("-> isValidAudioFile: '%s': %d", file_name, false);
            return false;
        }
        char file_name[MAX_FILE_LEN];
        file.getName(file_name, MAX_FILE_LEN);
        StrView strFileName(file_name);
        bool result = strFileName.endsWithIgnoreCase(exension) && strFileName.matches(file_name_pattern);
        LOGD("-> isValidAudioFile: '%s': %d", file_name, result);
        return result;
    }

    AudioFile getFileByPath(const char* path) {
        AudioFile dir;
        StrView inPath(path);
        Str strPath;
        Str strfileName;
        int pos = inPath.lastIndexOf("/") + 1;
        strPath.substring(path, 0, pos);
        strfileName.substring(path, pos, inPath.length());
        if (!dir.open(strPath.c_str())) {
            LOGE("directory: %s not open", path);
        } else {
            if (!dir.isDir()) {
                LOGE("directory: %s is not dictory", path);
            } else {
                if (!file.open(&dir, strfileName.c_str(), O_RDWR)) {
                    LOGE("file: %s not open", path);
                } else{
                    LOGD("-> getFileByPath: %s , %s", strPath.c_str(), strfileName.c_str());
                }
            }
        }
        dir.close();
        return file;
    }

    /// Determines the file at the indicated index (starting with 0)
    AudioFile getFileByPos(const char* dirStr, int pos) {
        AudioFile dir;
        AudioFile result;
        if (sd.exists(dirStr)){
            LOGI("directory: '%s'", dirStr);
            if (dir.open(dirStr)){
                if (dir.isDir()) {
                    size_t count = 0;
                    getFileAtIndex(dir, pos, count, result);
                    result.getName(file_name, MAX_FILE_LEN);
                    result.setTimeout(timeout);
                    LOGI("-> getFile: '%s': %d - %s", file_name, pos, result.isOpen() ? "open":"closed");
                } else {
                    LOGE("'%s' is not a directory!", dirStr);
                }
            } else {
                LOGE("Could not open direcotry: '%s'", dirStr);
            }
        } else {
            LOGE("directory: '%s' does not exist", dirStr);
        }
        dir.close();
        
        return result;
    }

    void getFileAtIndex(AudioFile dir, size_t pos, size_t& idx, AudioFile& result) {
        LOGD("%s: %d", LOG_METHOD, idx);
        char file_name_act[MAX_FILE_LEN];
        dir.getName(file_name_act, MAX_FILE_LEN);
        LOGD("-> processing directory: %s ", file_name_act);
        AudioFile file;
        dir.rewind();
        while (!result && file.openNext(&dir, O_RDONLY)) {
            // indent for dir level
            if (!file.isHidden()) {
                file.getName(file_name_act, MAX_FILE_LEN);
                LOGD("-> processing: %s with index %d", file_name_act, idx);

                if (isValidAudioFile(file)){
                    if (idx == pos) {
                        result = file;
                        result.getName(file_name, MAX_FILE_LEN);
                        LOGD("==> found: '%s' at index %d", file_name, idx);
                    }
                    idx++;                        
                }

                if (file.isDir()) {
                    getFileAtIndex(file, pos, idx, result);
                }
            }

            if (file.dirIndex()!=result.dirIndex()){
                LOGD("Close: %s", file_name_act);
                file.close();
            }
        }
        return;
    }
};

}