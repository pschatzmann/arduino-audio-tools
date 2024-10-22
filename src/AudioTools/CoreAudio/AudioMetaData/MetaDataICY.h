#pragma once
#include "AudioConfig.h"
#ifdef USE_URL_ARDUINO

#include "AbstractMetaData.h"
#include "AudioTools/CoreAudio/AudioBasic/StrView.h"
#include "AudioTools/CoreAudio/AudioHttp/HttpRequest.h"

namespace audio_tools {

/** 
 * @defgroup metadata-icy ICY
 * @ingroup metadata
 * @brief Icecast/Shoutcast Metadata
**/


/**
 * @brief Icecast/Shoutcast Metadata Handling.
 * Metadata class which splits the data into audio and metadata. The result is provided via callback methods.
 * see https://www.codeproject.com/Articles/11308/SHOUTcast-Stream-Ripper
 * @ingroup metadata-icy
 * @author Phil Schatzmann
 * @copyright GPLv3
 */ 
class MetaDataICY : public AbstractMetaData {

    enum Status {ProcessData, ProcessMetaData, SetupSize};

    public:
        MetaDataICY() = default;

        /// We just process the Metadata and ignore the audio info
        MetaDataICY(int metaint){
            setIcyMetaInt(metaint);
        }

        ~MetaDataICY(){
            if (metaData!=nullptr) delete[]metaData;
        }
        
        /// Defines the ICE metaint value which is provided by the web call!
        virtual void setIcyMetaInt(int value) override {
            this->mp3_blocksize = value;
        }

        /// Defines the metadata callback function
        virtual void setCallback(void (*fn)(MetaDataType info, const char* str, int len))  override {
            callback = fn;
        }

        /// Defines the audio callback function
        virtual void setAudioDataCallback(void (*fn)(const uint8_t* str, int len), int bufferLen=1024)  {
            dataBuffer = new uint8_t[bufferLen];
            dataCallback = fn;
            dataLen = 0;
            dataPos = 0;
        }

        /// Resets all counters and restarts the prcessing
        virtual void begin() override {
            clear();
            LOGI("mp3_blocksize: %d", mp3_blocksize);
        }

        /// Resets all counters and restarts the prcessing
        virtual void end() override {
            clear();
        }

        /// Writes the data in order to retrieve the metadata and perform the corresponding callbacks 
        virtual size_t write(const uint8_t *data, size_t len) override {
            if (callback!=nullptr){
                for (size_t j=0;j<len;j++){
                    processChar((char)data[j]);
                }
            }
            return len;
        }

        /// Returns the actual status of the state engine for the current byte
        virtual Status status() {
            return currentStatus;
        }

        /// returns true if the actual bytes is an audio data byte (e.g.mp3)
        virtual bool isData(){
            return currentStatus==ProcessData;
        }

        /// Returns true if the ICY stream contains metadata
        virtual bool hasMetaData() {
            return this->mp3_blocksize>0;
        }

        /// provides the metaint
        virtual int metaInt() {
            return mp3_blocksize;
        }

        /// character based state engine
        virtual void processChar(char ch){
            switch(nextStatus){
                case ProcessData:
                    currentStatus = ProcessData;
                    processData(ch);

                    // increment data counter and determine next status
                    ++totalData; 
                    if (totalData>=mp3_blocksize){
                        LOGI("Data ended")
                        totalData = 0;
                        nextStatus = SetupSize;
                    }   
                    break;

                case SetupSize:
                    currentStatus = SetupSize;
                    totalData = 0;
                    metaDataPos = 0;
                    metaDataLen = metaSize(ch);
                    LOGI("metaDataLen: %d", metaDataLen);
                    if (metaDataLen>0){
                        if (metaDataLen>200){
                            LOGI("Unexpected metaDataLen -> processed as data");
                            nextStatus = ProcessData;
                        } else {
                            LOGI("Metadata found");
                            setupMetaData(metaDataLen);
                            nextStatus = ProcessMetaData;
                        }
                    } else {
                        LOGI("Data found");
                        nextStatus = ProcessData;
                    }
                    break;

                case ProcessMetaData:
                    currentStatus = ProcessMetaData;
                    metaData[metaDataPos++]=ch;
                    if (metaDataPos>=metaDataLen){
                        processMetaData(metaData, metaDataLen);
                        LOGI("Metadata ended")
                        nextStatus = ProcessData;
                    }
                    break;
            }
        }


    protected:
        Status nextStatus = ProcessData;
        Status currentStatus = ProcessData;
        void (*callback)(MetaDataType info, const char* str, int len) = nullptr;
        char* metaData=nullptr;
        int totalData = 0;
        int mp3_blocksize = 0;
        int metaDataMaxLen = 0;
        int metaDataLen = 0;
        int metaDataPos = 0;
        bool is_data; // indicates if the current byte is a data byte
        // data
        uint8_t *dataBuffer=nullptr;
        void (*dataCallback)(const uint8_t* str, int len) = nullptr;    
        int dataLen = 0;
        int dataPos = 0;

        virtual void clear() {
            nextStatus = ProcessData;
            totalData = 0;
            metaDataLen = 0;
            metaDataPos = 0;
            dataLen = 0;
            dataPos = 0;
        }

        /// determines the meta data size from the size byte
        virtual int metaSize(uint8_t metaSize){
            return metaSize*16;
        }

        /// Make sure that the result is a valid ASCII string
        virtual bool isAscii(char* result, int l){
            // check on first 10 characters
            int m = l < 5 ? l : 10;
            for (int j=0; j<m; j++){
                if (!isascii(result[j])) return false;
            }
            return true;
        }

        /// allocates the memory to store the metadata / we support changing sizes
        virtual void setupMetaData(int meta_size) {
            TRACED();
            if (meta_size>0){
                if (metaData==nullptr){
                    metaData = new char[meta_size+1];
                    metaDataMaxLen = meta_size;
                    LOGD("metaDataMaxLen: %d", metaDataMaxLen);
                } else {
                    if (meta_size>metaDataMaxLen){
                        delete [] metaData; 
                        metaData = new char[meta_size+1];
                        metaDataMaxLen = meta_size;
                        LOGD("metaDataMaxLen: %d", metaDataMaxLen);
                    }
                }
                memset(metaData, 0, meta_size);
            }
        }

        /// e.g. StreamTitle=' House Bulldogs - But your love (Radio Edit)';StreamUrl='';
        virtual void processMetaData( char* metaData, int len) {
            //CHECK_MEMORY();
            TRACED();
            metaData[len]=0;
            if (isAscii(metaData, 12)){
                LOGI("%s", metaData);
                StrView meta(metaData,len+1, len);
                int start = meta.indexOf("StreamTitle=");
                if (start>=0){
                    start+=12;
                }
                int end = meta.indexOf("';");
                if (start>=0 && end>start){
                    metaData[end]=0;
                    if (callback!=nullptr){
                        callback(Title, (const char*)metaData+start+1, end-start);
                    }
                }   
               // CHECK_MEMORY();
            } else {
               // CHECK_MEMORY();
                LOGW("Unexpected Data: %s", metaData);
            }
        }

        /// Collects the data in a buffer and executes the callback when the buffer is full
        virtual void processData(char ch){
            if (dataBuffer!=nullptr){
                dataBuffer[dataPos++] = ch;
                // output data via callback
                if (dataPos>=dataLen){
                    dataCallback(dataBuffer, dataLen);
                    dataPos = 0;
                }
            }
        }
};


/**
 * @brief Resolve icy-metaint from HttpRequest and execute metadata callbacks
 * @ingroup metadata-icy
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class ICYUrlSetup {
    public:
        /// Fills the metaint from the Http Request and executes metadata callbacks on http reply parameters
        int setup(HttpRequest &http ) {
            TRACED();
            p_http = &http;
            const char* iceMetaintStr = http.reply().get("icy-metaint");
            if (iceMetaintStr){
                LOGI("icy-metaint: %s", iceMetaintStr);
            } else {
                LOGE("icy-metaint not defined");
            }
            StrView value(iceMetaintStr);
            int iceMetaint = value.toInt();
            return iceMetaint;
        }

        /// Executes the metadata callbacks with data provided from the http request result parameter
        void executeCallback(void (*callback)(MetaDataType info, const char* str, int len)) {
            TRACEI();
            if (callback==nullptr){
                LOGW("callback not defined")
            }
            if (p_http==nullptr){
                LOGW("http not defined")
            }
            // Callbacks filled from url reply for icy
            if (callback!=nullptr && p_http!=nullptr) {
                // handle icy parameters
                StrView genre(p_http->reply().get("icy-genre"));
                if (!genre.isEmpty()){
                    callback(Genre, genre.c_str(), genre.length());
                }

                StrView descr(p_http->reply().get("icy-description"));
                if (!descr.isEmpty()){
                    callback(Description, descr.c_str(), descr.length());
                }

                StrView name(p_http->reply().get("icy-name"));
                if (!name.isEmpty()){
                    callback(Name, name.c_str(), name.length());
                } 
            }
        }

    protected:
        HttpRequest *p_http = nullptr;

};

}

#endif