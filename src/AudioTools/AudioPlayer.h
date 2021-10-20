#pragma once

#include "AudioConfig.h"
#include "AudioTools/AudioTypes.h"
#include "AudioTools/Buffers.h"
#include "AudioTools/Converter.h"
#include "AudioTools/AudioLogger.h"
#include "AudioTools/Streams.h"
#include "AudioTools/AudioCopy.h"
#include "AudioCodecs/CodecMP3Helix.h"
#include "AudioHttp/Str.h"
#include "AudioHttp/URLStream.h"

#ifdef USE_SDFAT
#include <SPI.h>
#include <SdFat.h>
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

        /// Provides the timeout which is triggering to move to the next stream
        virtual int timeoutMs() {
            return 500;
        }

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

         AudioSourceCallback(Stream* (*nextStreamCallback)(), void (*onStartCallback)()=nullptr) {
	 		LOGD(LOG_METHOD);
            this->onStartCallback = onStartCallback;
            this->nextStreamCallback = nextStreamCallback;
         }

        /// Reset actual stream and move to root
        virtual void begin() {
	 		LOGD(LOG_METHOD);
            if (onStartCallback!=nullptr) onStartCallback();
        };

        /// Returns next stream
        virtual Stream* nextStream(int offset) {
	 		LOGD(LOG_METHOD);
            return nextStreamCallback==nullptr ? nullptr : nextStreamCallback();
        }

        void setCallbackOnStart(void (*callback)()){
            onStartCallback = callback;
        }

        void setCallbackNextStream(Stream* (*callback)() ){
            nextStreamCallback = callback;
        }

    protected:
        void (*onStartCallback)() = nullptr;
        Stream* (*nextStreamCallback)() = nullptr;

};

#ifdef USE_SDFAT
#ifdef ARDUINO_ARCH_RP2040
// RP2040 is using the library with a sdfat namespace
typedef sdfat::SdSpiConfig SdSpiConfig;
typedef sdfat::FsFile AudioFile;
typedef sdfat::SdFs AudioFs;
#else
typedef FsFile AudioFile;
typedef SdFs AudioFs;
#endif


/**
 * @brief AudioSource using an SD card as data source. This class is based on https://github.com/greiman/SdFat 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AudioSourceSdFat : public AudioSource {
    public:
        AudioSourceSdFat(const char *startFilePath="/", const char* ext=".mp3", int chipSelect=PIN_CS, int speedMHz=2){
	 		LOGD(LOG_METHOD);
            LOGI("SD chipSelect: %d", chipSelect);
            LOGI("SD speedMHz: %d", speedMHz);
            if (!sd.begin(SdSpiConfig(chipSelect, DEDICATED_SPI, SD_SCK_MHZ(speedMHz)))){
                LOGE("SD.begin failed");
            }
            start_path = startFilePath;
            exension = ext;
        }

        virtual void begin() {
	 		LOGD(LOG_METHOD);
            pos = 0;
        }

        virtual Stream* nextStream(int offset){
	 		LOGD(LOG_METHOD);
            // move to next file
            pos += offset;
            file.close();
            file = getFile(start_path, pos);
            file.getName(file_name, MAX_FILE_LEN);
            LOGI("-> nextStream: '%s'", file_name);
            return file ? &file : nullptr;
        }

        /// Defines the regex filter criteria for selecting files. E.g. ".*Bob Dylan.*" 
        void setFileFilter(const char* filter){
            file_name_pattern = filter;
        }

    protected:
        AudioFile file;
        AudioFs sd;
        size_t pos = 0;
        char file_name[MAX_FILE_LEN];
        const char *exension = nullptr;
        const char* start_path = nullptr;
        const char* file_name_pattern = "*";

        /// checks if the file is a valid audio file
        bool isValidAudioFile(AudioFile &file){
            file.getName(file_name, MAX_FILE_LEN);
            Str strFileName(file_name);
            bool result =  strFileName.endsWith(exension) && strFileName.matches(file_name_pattern);
            LOGI("-> isValidAudioFile: '%s': %d", file_name, result);
            return result;
        }

        /// Determines the file at the indicated index (starting with 0)
        AudioFile getFile(const char *dirStr, int pos) {
            AudioFile dir; 
            AudioFile result;
            dir.open(dirStr);
            size_t count = 0;
            getFileAtIndex(dir, pos, count, result);
            result.getName(file_name, MAX_FILE_LEN);
            LOGI("-> getFile: '%s': %d", file_name, pos);
            dir.close();
            return result;
        }

        /// Recursively walk the directory tree to find the file at the indicated pos.
        void getFileAtIndex(AudioFile dir, size_t pos, size_t &idx, AudioFile &result) {
	 		LOGD("%s: %d", LOG_METHOD, idx);
            AudioFile file;
            if (idx>pos) return;
            
            while (file.openNext(&dir, O_READ)) {
                if (idx>pos) return;
                if (!file.isHidden()) {
                    if (!file.isDir()) {
                        if (isValidAudioFile(file)){
                            idx++;
                            if (idx-1==pos){
                                LOGI("-> get: '%s'", file_name);
                                result = file;
                                result.getName(file_name, MAX_FILE_LEN);
                                //return;
                            }
                        }
                    } else {
                        getFileAtIndex(file, pos, idx, result);
                    }

                    // close all files except result
                    char file_name_act[MAX_FILE_LEN];
                    file.getName(file_name_act, MAX_FILE_LEN);
                    //LOGI("-> %s <-> %s", file_name_act, file_name);
                    if (!Str(file_name_act).equals(file_name)){
                        file.getName(file_name, MAX_FILE_LEN);
                        file.close();
                        LOGI("-> close: '%s'", file_name);
                    } else {
                        return;
                    }
                }
            }
        }
};

#endif


#if defined(ESP32) || defined(ESP8266) || defined(USE_URL_ARDUINO) 

/**
 * @brief Audio Source which provides the data via the network from an URL
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AudioSourceURL : public AudioSource {
    public:
        AudioSourceURL(URLStream &urlSstream, const char *urlArray[],const char* mime, int arraySize=0, int startPos=0) {
	 		LOGD(LOG_METHOD);
            this->actual_stream = &urlSstream;
            this->mime = mime;
            this->urlArray = urlArray;
            this->max = arraySize;
            this->pos = startPos-1;
        }

        /// Setup Wifi URL
        virtual void begin() {
	 		LOGD(LOG_METHOD);
            setPos(-1);
        }

        /// Defines the actual position
        void setPos(int newPos) {
            pos = newPos;
            if (pos>=max || pos<0){
                pos = -1;
            } 
        }

        /// Opens the next url from the array
        Stream* nextStream(int offset){
            pos += offset;
            if (pos<0){
                pos=0;
            }
            LOGI("nextStream: %s", urlArray[pos]);
            if (offset!=0 || actual_stream==nullptr){
                actual_stream->end();
                actual_stream->begin(urlArray[pos], mime);
                pos += offset;
                if (pos>=max){
                    pos = 0;
                } else if (pos<0){
                    pos = max -1;
                }
            }
            return actual_stream;
        }

        void setTimeoutMs(int millisec){
            timeout = millisec;   
        }

        int timeoutMs() {
            return timeout;
        }

    protected:
        URLStream *actual_stream=nullptr;
        const char **urlArray;
        int pos=0;
        int max=0;
        const char *mime=nullptr;
        int timeout = 60000;

};

#endif

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
class AudioPlayer: public AudioBaseInfoDependent {

    public:
        /**
         * @brief Construct a new Audio Player object
         * 
         * @param decoder 
         * @param output 
         * @param chipSelect 
         */
        AudioPlayer(AudioSource &source, Print &output, AudioDecoder &decoder ) : scaler(1.0, 0, 32767){
	 		LOGD(LOG_METHOD);
            this->source = &source;
            this->out = new EncodedAudioStream(&output, &decoder); 
            // notification for audio configuration
            decoder.setNotifyAudioChange(*this);            
            audioInfoTarget = (AudioBaseInfoDependent*) &output;
        }

        /// (Re)Starts the playing of the music (from the beginning)
        bool begin(bool isActive = true) {
	 		LOGD(LOG_METHOD);
            bool result = false;
            this->out->begin();
            // setup input
            this->source->begin();
            // get first streem
            input_stream = nextStream(1);
            if (input_stream!=nullptr) {
                meta_out.begin();
                copier.setCallbackOnWrite(onWrite, this);
                copier.begin(*out, *input_stream);
                timeout = millis() + source->timeoutMs();
                active = isActive;
                result = true;
            } else {
                LOGI("-> begin: no data found");
            }
            return result;
        }

        virtual void setAudioInfo(AudioBaseInfo info) {
            if (audioInfoTarget!=nullptr){
                LOGD(LOG_METHOD);
                LOGI("sample_rate: %d", info.sample_rate);
                LOGI("bits_per_sample: %d", info.bits_per_sample);
                LOGI("channels: %d", info.channels);
                audioInfoTarget->setAudioInfo(info);
                begin(true);
            }
        };


        /// starts / resumes the playing of a matching song
        void play(){
	 		LOGD(LOG_METHOD);
            active = true;
        }

        /// halts the playing
        void stop(){
	 		LOGD(LOG_METHOD);
            active = false;
        }

        /// moves to next file
        void next(){
	 		LOGD(LOG_METHOD);
            active = false;

            input_stream = nextStream(+1);
            meta_out.begin();

            if (input_stream!=nullptr) {
                copier.begin(*out, *input_stream);
                active = true;
                timeout = millis() + source->timeoutMs();
            }    
        }

        /// determines if the player is active
        bool isActive() {
            return active;
        }

        /// sets the volume - values need to be between 0.0 and 1.0
        void setVolume(float volume){
            if (volume>=0 && volume<=1.0) {
                if (volume!=currentVolume){
                    LOGI("setVolume(%f)", volume);
                    scaler.setFactor(volume);
                    currentVolume = volume;
                }
            } else {
                LOGE("setVolume value '%f' out of range (0.0 -1.0)", volume);
            }
        }

        /// Determines the actual volume
        float volume() {
            return scaler.factor();
        }

        /// Call this method in the loop. 
        void copy(){
            if (active){
    	 		LOGD(LOG_METHOD);
                // handle sound
                if (copier.copy(scaler) || timeout==0){
                    // reset timeout
                    timeout = millis() + source->timeoutMs();
                }

                // move to next stream after timeout
                if (millis()>timeout){
        	 		LOGW("-> timeout");
                    // open next stream
                    input_stream = nextStream(+1);
                    if (input_stream!=nullptr){
                        LOGD("open next stream");
                        meta_out.begin();
                        copier.begin(*out, *input_stream);
                    } else {
                        LOGD("stream is null");
                    }
                }
            }
        }

        /// Defines the medatadata callback
        void setCallbackMetadata(void (*callback)(MetaDataType type, const char* str, int len)) {
    	 	LOGD(LOG_METHOD);
            meta_active = true;
            meta_out.setCallback(callback);
        }

    protected:

        bool active = false;
        AudioSource *source = nullptr;
        AudioDecoder *decoder = nullptr;
        EncodedAudioStream *out = nullptr; // Decoding stream
        Stream* input_stream = nullptr;
        MetaDataID3 meta_out;
        bool meta_active = false;
        StreamCopy copier; // copies sound into i2s
        ConverterScaler<int16_t> scaler; // volume control
        AudioBaseInfoDependent *audioInfoTarget=nullptr;
        uint32_t timeout = 0;
        float currentVolume=-1; // illegal value which will trigger an update


        /// Callback implementation which writes to metadata
        static void onWrite(void* obj, void* data, size_t len){
    	 	LOGD(LOG_METHOD);
            AudioPlayer *p = (AudioPlayer*)obj;
            if (p->meta_active){
                p->meta_out.write((const uint8_t*)data, len);
            }
        }

        /// Returns next stream
        virtual Stream* nextStream(int offset) {
    	 	LOGD(LOG_METHOD);
            return source->nextStream(offset);
        };

};

}