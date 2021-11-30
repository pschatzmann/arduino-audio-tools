#pragma once

#include "AudioConfig.h"
#include "AudioTools/AudioTypes.h"
#include "AudioTools/Buffers.h"
#include "AudioTools/Converter.h"
#include "AudioTools/AudioLogger.h"
#include "AudioTools/AudioStreams.h"
#include "AudioTools/AudioCopy.h"
#include "AudioCodecs/CodecMP3Helix.h"
#include "AudioHttp/AudioHttp.h"
#include "AudioHttp/Str.h"

#ifdef USE_SDFAT
#include <SPI.h>
#include <SdFat.h>
#endif

// SD_FAT_TYPE = 0 for SdFat/File as defined in SdFatConfig.h,
// 1 for FAT16/FAT32, 2 for exFAT, 3 for FAT16/FAT32 and exFAT.
#ifndef SD_FAT_TYPE
#define SD_FAT_TYPE 1
#endif
// Try max SPI clock for an SD. Reduce SPI_CLOCK if errors occur. (40?)
#define SPI_CLOCK SD_SCK_MHZ(50)
// Max file name length including directory path
#define MAX_FILE_LEN 256


namespace audio_tools {

    /**
     * @brief Abstract Audio Data Source which is used by the Audio Players
     * @author Phil Schatzmann
     * @copyright GPLv3
     *
     */
    class AudioSource {
    public:
        /// Reset actual stream and move to root
        virtual void begin() = 0;

        /// Returns next audio stream
        virtual Stream* nextStream(int offset) = 0;

        /// Returns previous audio stream
        virtual Stream* previousStream(int offset) {
            return nextStream(-offset);
        };

        /// Returns audio stream at the indicated index (the index is zero based, so the first value is 0!)
        virtual Stream* selectStream(int index) {
            LOGE("Not Supported!");
            return nullptr;
        }

        /// same as selectStream - I just prefer this name
        virtual Stream* setIndex(int index) {
            return selectStream(index);
        }

        /// Returns audio stream by path
        virtual Stream* selectStream(char* path) {
            return selectStream(path);
        }

        /// Sets the timeout which is triggering to move to the next stream. - the default value is 500 ms
        virtual void setTimeoutAutoNext(int millisec) {
            timeout_auto_next_value = millisec;
        }

        /// Provides the timeout which is triggering to move to the next stream.
        virtual int timeoutAutoNext() {
            return timeout_auto_next_value;
        }

        /// Sets the timeout of Stream in milliseconds
        void setTimeout(int millisec);

        /// Returns default setting go to the next
        virtual bool isAutoNext();

        // only the ICYStream supports this
        virtual bool setMetadataCallback(void (*fn)(MetaDataType info, const char* str, int len)) {
            return false;
        }


    protected:
        int timeout_auto_next_value = 500;
    };

    /**
     * @brief Callback Audio Data Source which is used by the Audio Players
     * @author Phil Schatzmann
     * @copyright GPLv3
     */
    class AudioSourceCallback : public AudioSource {
    public:
        AudioSourceCallback() {
        }

        AudioSourceCallback(Stream* (*nextStreamCallback)(), void (*onStartCallback)() = nullptr) {
            LOGD(LOG_METHOD);
            this->onStartCallback = onStartCallback;
            this->nextStreamCallback = nextStreamCallback;
        }

        /// Reset actual stream and move to root
        virtual void begin() override {
            LOGD(LOG_METHOD);
            if (onStartCallback != nullptr) onStartCallback();
        };

        /// Returns next (with positive index) or previous stream (with negative index)
        virtual Stream* nextStream(int offset) override {
            LOGD(LOG_METHOD);
            return nextStreamCallback == nullptr ? nullptr : nextStreamCallback();
        }

        /// Returns selected audio stream
        virtual Stream* selectStream(int index) override {
            return indexStreamCallback == nullptr ? nullptr : indexStreamCallback(index);
        }

        void setCallbackOnStart(void (*callback)()) {
            onStartCallback = callback;
        }

        void setCallbackNextStream(Stream* (*callback)()) {
            nextStreamCallback = callback;
        }

        void setCallbackSelectStream(Stream* (*callback)(int idx)) {
            indexStreamCallback = callback;
        }

    protected:
        void (*onStartCallback)() = nullptr;
        Stream* (*nextStreamCallback)() = nullptr;
        Stream* (*indexStreamCallback)(int index) = nullptr;
    };

#ifdef USE_SDFAT
#if defined(ARDUINO_ARCH_RP2040) && !defined(PICO)
    // only RP2040 from Earle Phil Hower is using the library with a sdfat namespace
    typedef sdfat::SdSpiConfig SdSpiConfig;
    typedef sdfat::FsFile AudioFile;
    typedef sdfat::SdFs AudioFs;
#else
#if SD_FAT_TYPE == 0
	typedef SdFat AudioFs;
	typedef File AudioDir;
	typedef File AudioFile;
#elif SD_FAT_TYPE == 1
	typedef SdFat32 AudioFs;
	//typedef File32 AudioDir;
	typedef File32 AudioFile;
#elif SD_FAT_TYPE == 2
	typedef SdExFat AudioFs;
	typedef ExFile AudioDir;
	typedef ExFile AudioFile;
#elif SD_FAT_TYPE == 3
	typedef SdFs AudioFs;
	typedef FsFile AudioDir;
	typedef FsFile AudioFile;
#else  // SD_FAT_TYPE
#endif
#endif


    /**
     * @brief AudioSource using an SD card as data source. This class is based on https://github.com/greiman/SdFat
     * @author Phil Schatzmann
     * @copyright GPLv3
     */
    class AudioSourceSdFat : public AudioSource {
    public:
        /// Default constructor
        AudioSourceSdFat(const char* startFilePath = "/", const char* ext = ".mp3", int chipSelect = PIN_CS, int speedMHz = 2) {
            LOGD(LOG_METHOD);
            LOGI("SD chipSelect: %d", chipSelect);
            LOGI("SD speedMHz: %d", speedMHz);
            if (!sd.begin(SdSpiConfig(chipSelect, DEDICATED_SPI, SD_SCK_MHZ(speedMHz)))) {
                LOGE("SD.begin failed");
            }
            start_path = startFilePath;
            exension = ext;
        }

        /// Costructor with SdSpiConfig
        AudioSourceSdFat(const char* startFilePath, const char* ext, SdSpiConfig &config) {
            LOGD(LOG_METHOD);
            if (!sd.begin(config)) {
                LOGE("SD.begin failed");
            }
            start_path = startFilePath;
            exension = ext;
        }
        virtual void begin() override {
            LOGD(LOG_METHOD);
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

        virtual Stream* selectStream(char* path) override {
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

    protected:
        AudioFile file;
        AudioFs sd;
        size_t idx_pos = 0;
        char file_name[MAX_FILE_LEN];
        const char* exension = nullptr;
        const char* start_path = nullptr;
        char* file_name_pattern = (char*) "*";

        /// checks if the file is a valid audio file
        bool isValidAudioFile(AudioFile& file) {
            char file_name[MAX_FILE_LEN];
            file.getName(file_name, MAX_FILE_LEN);
            Str strFileName(file_name);
            bool result = strFileName.endsWithIgnoreCase(exension) && strFileName.matches(file_name_pattern);
            LOGI("-> isValidAudioFile: '%s': %d", file_name, result);
            return result;
        }

        /// Determines the file at the indicated index (starting with 0)
        AudioFile getFileByPos(const char* dirStr, int pos) {
            AudioFile dir;
            AudioFile result;
            if (sd.exists(dirStr)){
                LOGI("directory: %s", dirStr);
            } else {
                LOGE("directory: %s does not exist", dirStr);
                stop();
            }

            dir.open(dirStr);
            size_t count = 0;
            getFileAtIndex(dir, pos, count, result);
            result.getName(file_name, MAX_FILE_LEN);
            LOGD("-> getFile: '%s': %d", file_name, pos);
            dir.close();
            return result;
        }

        AudioFile getFileByPath(char* path) {
            AudioFile dir;
            Str inPath(path);
            StrExt strPath;
            StrExt strfileName;
            int pos = inPath.lastIndexOf("/") + 1;
            strPath.substring(path, 0, pos);
            strfileName.substring(path, pos, inPath.length());
            if (!dir.open(strPath.c_str())) {
                LOGE("directory: %s not open", path);
            }
            else
            {
                if (!dir.isDir()) {
                    LOGE("directory: %s is not dictory", path);
                }
                else
                {
                    if (!file.open(&dir, strfileName.c_str(), O_RDWR)) {
                        LOGE("file: %s not open", path);
                    }
                    else
                    {
                        LOGD("-> getFileByPath: %s , %s", strPath.c_str(), strfileName.c_str());
                    }
                }
            }
            dir.close();
            return file;
        }

        /// Recursively walk the directory tree to find the file at the indicated pos.
        void getFileAtIndex(AudioFile dir, size_t pos, size_t& idx, AudioFile& result) {
            LOGD("%s: %d", LOG_METHOD, idx);
            AudioFile file;
            if (idx > pos) return;

            bool found = false;
            while (!found && file.openNext(&dir, O_RDONLY )) {
                //if (idx > pos) return;

                // close all files except result
                char file_name_act[MAX_FILE_LEN];
                file.getName(file_name_act, MAX_FILE_LEN);
                LOGD("-> processing: %s with index %d", file_name_act, idx);
                if (!file.isHidden()) {
                    if (!file.isDir()) {
                        if (isValidAudioFile(file)) {
                            if (idx == pos) {
                                found = true;
                                result = file;
                                result.getName(file_name, MAX_FILE_LEN);
                                LOGI("==> found: '%s' at index %d", file_name, idx);
                            }
                            idx++;
                        }
                    } else {
                        // process subdirectory
                        getFileAtIndex(file, pos, idx, result);
                    }

                    if (!Str(file_name_act).equals(file_name)) {
                        file.getName(file_name, MAX_FILE_LEN);
                        file.close();
                        LOGD("-> close: '%s'", file_name);
                    }  
                }
            }
        }

    };

#endif


#if defined(USE_URL_ARDUINO) && ( defined(ESP32) || defined(ESP8266) )

    /**
     * @brief Audio Source which provides the data via the network from an URL
     * @author Phil Schatzmann
     * @copyright GPLv3
     */
    class AudioSourceURL : public AudioSource {
    public:
        template<typename T, size_t N>
        AudioSourceURL(AbstractURLStream& urlStream, T(&urlArray)[N], const char* mime, int startPos = 0) {
            LOGD(LOG_METHOD);
            this->actual_stream = &urlStream;
            this->mime = mime;
            this->urlArray = urlArray;
            this->max = N;
            this->pos = startPos - 1;
            this->timeout_auto_next_value = 20000;
        }

        /// Setup Wifi URL
        virtual void begin() override {
            LOGD(LOG_METHOD);
            this->pos = 0;
        }

        /// Opens the selected url from the array
        Stream* selectStream(int idx) override {
            pos = idx;
            if (pos < 0) {
                pos = 0;
                LOGI("url array out of limits: %d -> %d", idx, pos);
            }
            if (pos >= max) {
                pos = max-1;
                LOGI("url array out of limits: %d -> %d", idx, pos);
            }
            LOGI("selectStream: %d/%d -> %s", pos, max-1, urlArray[pos]);
            if (started) actual_stream->end();
            actual_stream->begin(urlArray[pos], mime);
            started = true;
            return actual_stream;
        }

        /// Opens the next url from the array
        Stream* nextStream(int offset) override {
            pos += offset;
            if (pos < 0 || pos >= max) {
                pos = 0;
            }
            LOGI("nextStream: %d/%d -> %s", pos, max-1, urlArray[pos]);
            return selectStream(pos);
        }

        /// Opens the Previous url from the array
        Stream* previousStream(int offset) override {
            pos -= offset;
            if (pos < 0 || pos >= max) {
                pos = max-1;
            }
            LOGI("previousStream: %d/%d -> %s", pos, max-1, urlArray[pos]);
            return selectStream(pos);
        }

        /// Opens the selected url
        Stream* selectStream(char* path) override {
            LOGI("selectStream: %s", path);
            if (started) actual_stream->end();
            actual_stream->begin(path, mime);
            started = true;
            return actual_stream;
        }

        int index() {
            return pos;
        }

        const char *toStr() {
            return urlArray[pos];
        }

        /// Sets the timeout of the URL Stream in milliseconds
        void setTimeout(int millisec){
            actual_stream->setTimeout(millisec);
        }

        // provides go not to the next on error
        virtual bool isAutoNext() {
            return true;
        };

        // only the ICYStream supports this
        bool setMetadataCallback(void (*fn)(MetaDataType info, const char* str, int len)) {
            LOGI(LOG_METHOD);
            return actual_stream->setMetadataCallback(fn);
        }


    protected:
        AbstractURLStream* actual_stream = nullptr;
        const char** urlArray;
        int pos = 0;
        int max = 0;
        const char* mime = nullptr;
        bool started = false;
    };

#endif

    /**
     * @brief Helper class to debounce user input from a push button
     *
     */
    class Debouncer {

    public:
        Debouncer(uint16_t timeoutMs = 5000, void* ref = nullptr) {
            setDebounceTimeout(timeoutMs);
            p_ref = ref;
        }

        void setDebounceTimeout(uint16_t timeoutMs) {
            ms = timeoutMs;
        }

        /// Prevents that the same method is executed multiple times within the indicated time limit
        bool debounce(void(*cb)(void* ref) = nullptr) {
            bool result = false;
            if (millis() > debounce_ms) {
                LOGI("accpted");
                if (cb != nullptr) cb(p_ref);
                // new time limit
                debounce_ms = millis() + ms;
                result = true;
            }
            else {
                LOGI("rejected");
            }
            return result;
        }

    protected:
        unsigned long debounce_ms = 0; // Debounce sensitive touch
        uint16_t ms;
        void* p_ref = nullptr;

    };

    /**
     * @brief Implements a simple audio player which supports the following commands:
     * - begin
     * - play
     * - stop
     * - next
     * - setVolume
     * @author Phil Schatzmann
     * @copyright GPLv3
     */
    class AudioPlayer : public AudioBaseInfoDependent {

    public:
        /**
         * @brief Construct a new Audio Player object. The processing chain is
         * AudioSource -> Stream -copy> EncodedAudioStream -> VolumeOutput -> Print
         *
         * @param source
         * @param output
         * @param decoder
         */
        AudioPlayer(AudioSource& source, AudioPrint& output, AudioDecoder& decoder) {
            LOGD(LOG_METHOD);
            this->p_source = &source;
            this->volume_out.begin(output);
            this->p_out_decoding = new EncodedAudioStream(volume_out, decoder);
            this->p_final_print = &output;

            // notification for audio configuration
            decoder.setNotifyAudioChange(*this);
        }

        /**
         * @brief Construct a new Audio Player object. The processing chain is
         * AudioSource -> Stream -copy> EncodedAudioStream -> VolumeOutput -> Print
         *
         * @param source
         * @param output
         * @param decoder
         * @param notify
         */
        AudioPlayer(AudioSource& source, Print& output, AudioDecoder& decoder, AudioBaseInfoDependent* notify = nullptr) {
            LOGD(LOG_METHOD);
            this->p_source = &source;
            this->volume_out.begin(output);
            this->p_out_decoding = new EncodedAudioStream(volume_out, decoder);
            this->p_final_notify = notify;

            // notification for audio configuration
            decoder.setNotifyAudioChange(*this);
        }

        /**
         * @brief Construct a new Audio Player object. The processing chain is
         * AudioSource -> Stream -copy> EncodedAudioStream -> VolumeOutput -> Print
         *
         * @param source
         * @param output
         * @param decoder
         */
        AudioPlayer(AudioSource& source, AudioStream& output, AudioDecoder& decoder) {
            LOGD(LOG_METHOD);
            this->p_source = &source;
            this->volume_out.begin(output);
            this->p_out_decoding = new EncodedAudioStream(volume_out, decoder);
            this->p_final_stream = &output;

            // notification for audio configuration
            decoder.setNotifyAudioChange(*this);
        }

        /// Default destructor
        virtual ~AudioPlayer() {
            if (p_out_decoding != nullptr) {
                delete p_out_decoding;
            }
        }

        /// (Re)Starts the playing of the music (from the beginning)
        virtual bool begin(int index=0, bool isActive = true) {
            LOGD(LOG_METHOD);
            bool result = false;

            // navigation supoort
            autonext = p_source->isAutoNext();

            // start dependent objects
            p_out_decoding->begin();
            p_source->begin();
            meta_out.begin();
            
            if (index >= 0) {
                p_input_stream = p_source->selectStream(index);
                if (p_input_stream != nullptr) {
                    if (meta_active) {
                        copier.setCallbackOnWrite(decodeMetaData, this);
                    }
                    copier.begin(*p_out_decoding, *p_input_stream);
                    timeout = millis() + p_source->timeoutAutoNext();
                    active = isActive;
                    result = true;
                }
                else {
                    LOGW("-> begin: no data found");
                    active = isActive;
                    result = false;
                }
            }
            else {
                LOGW("-> begin: no stream selected");
                active = isActive;
                result = false;
            }
            return result;
        }

        virtual void end() {
            LOGD(LOG_METHOD);
            p_out_decoding->end();
            meta_out.end();
        }

        /// Updates the audio info in the related objects
        virtual void setAudioInfo(AudioBaseInfo info) {
            LOGD(LOG_METHOD);
            LOGI("sample_rate: %d", info.sample_rate);
            LOGI("bits_per_sample: %d", info.bits_per_sample);
            LOGI("channels: %d", info.channels);
            // notifiy volume
            volume_out.setAudioInfo(info);
            // notifiy final ouput: e.g. i2s
            if (p_final_print != nullptr) p_final_print->setAudioInfo(info);
            if (p_final_stream != nullptr) p_final_stream->setAudioInfo(info);
            if (p_final_notify != nullptr) p_final_notify->setAudioInfo(info);
        };

        /// starts / resumes the playing of a matching song
        virtual void play() {
            LOGD(LOG_METHOD);
            active = true;
        }

        /// halts the playing
        virtual void stop() {
            LOGD(LOG_METHOD);
            active = false;
        }

        /// moves to next file
        virtual bool next(int offset=1) {
            LOGD(LOG_METHOD);
            previous_stream = false;
            active = setStream(p_source->nextStream(offset));
            return active;
        }

        /// moves to selected file
        virtual bool setIndex(int idx) {
            LOGD(LOG_METHOD);
            previous_stream = false;
            active = setStream(p_source->selectStream(idx));
            return active;
        }

        /// moves to selected file
        virtual bool setPath(char* path) {
            LOGD(LOG_METHOD);
            previous_stream = false;
            active = setStream(p_source->selectStream(path));
            return active;
        }

        /// moves to previous file
        virtual bool previous(int offset=1) {
            LOGD(LOG_METHOD);
            previous_stream = true;
            active = setStream(p_source->previousStream(offset));
            return active;
        }

        /// start selected input stream
        virtual bool setStream(Stream *input) {
            end();
            p_out_decoding->begin();
            p_input_stream = input;
            if (p_input_stream != nullptr) {
                LOGD("open selected stream");
                meta_out.begin();
                copier.begin(*p_out_decoding, *p_input_stream);
            }
            return p_input_stream != nullptr;
        }

        /// determines if the player is active
        virtual bool isActive() {
            return active;
        }

        /// sets the volume - values need to be between 0.0 and 1.0
        virtual void setVolume(float volume) {
            if (volume >= 0 && volume <= 1.0) {
                if (abs(volume - current_volume) > 0.01) {
                    LOGI("setVolume(%f)", volume);
                    volume_out.setVolume(volume);
                    current_volume = volume;
                }
            }
            else {
                LOGE("setVolume value '%f' out of range (0.0 -1.0)", volume);
            }
        }

        /// Determines the actual volume
        virtual float volume() {
            return volume_out.volume();
        }

        /// Set move to next
        virtual void setAutoNext(bool next) {
            autonext = next;
        }

        /// Call this method in the loop. 
        virtual void copy() {
            if (active) {
                LOGD(LOG_METHOD);
                // handle sound
                if (copier.copy() || timeout == 0) {
                    // reset timeout
                    timeout = millis() + p_source->timeoutAutoNext();
                }
                // move to next stream after timeout
                if (p_input_stream == nullptr || millis() > timeout) {
                    if (autonext) {
                        if (previous_stream == false) {
                            LOGW("-> timeout - moving to next stream");
                            // open next stream
                            if (!next(1)) {
                                LOGD("stream is null");
                            }
                        }
                        else {
                            LOGW("-> timeout - moving to previous stream");
                            // open previous stream
                            if (!previous(1)) {
                                LOGD("stream is null");
                            }
                        }
                    }
                    else
                    {
                        active = false;
                    }
                    timeout = millis() + p_source->timeoutAutoNext();
                }
            }
        }

        /// Defines the medatadata callback
        virtual void setMetadataCallback(void (*callback)(MetaDataType type, const char* str, int len)) {
            LOGI(LOG_METHOD);
            // setup metadata. 
            if (p_source->setMetadataCallback(callback)){
                // metadata is handled by source
                LOGI("Using ICY Metadata");
                meta_active = false;
            } else {
                // metadata is handled here
                meta_out.setCallback(callback);
                meta_active = true;
            }
        }

        /// Change the VolumeControl implementation
        virtual void setVolumeControl(VolumeControl& vc) {
            volume_out.setVolumeControl(vc);
        }

    protected:
        bool active = false;
        bool autonext = false;
        AudioSource* p_source = nullptr;
        VolumeOutput volume_out; // Volume control
        MetaDataID3 meta_out; // Metadata parser
        EncodedAudioStream* p_out_decoding = nullptr; // Decoding stream
        Stream* p_input_stream = nullptr;
        AudioPrint* p_final_print = nullptr;
        AudioStream* p_final_stream = nullptr;
        AudioBaseInfoDependent* p_final_notify = nullptr;
        StreamCopy copier; // copies sound into i2s
        bool meta_active = false;
        uint32_t timeout = 0;
        bool previous_stream = false;
        float current_volume = -1; // illegal value which will trigger an update

        /// Callback implementation which writes to metadata
        static void decodeMetaData(void* obj, void* data, size_t len) {
            LOGD("%s, %zu", LOG_METHOD, len);
            AudioPlayer* p = (AudioPlayer*)obj;
            if (p->meta_active) {
                p->meta_out.write((const uint8_t*)data, len);
            }
        }

    };

}