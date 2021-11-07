#pragma once

#include "MetaDataCommon.h"
#include "AudioHttp/Str.h"

namespace audio_tools {

/**
 * Icecast/Shoutcast Metadata Handling
 * Output Class which splits the data into audio and metadata.
 * see https://www.codeproject.com/Articles/11308/SHOUTcast-Stream-Ripper
 */ 
class MetaDataICY : public MetaDataCommon {

    enum Status {WriteData, WriteMetaData, SetupSize};

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
        void setIcyMetaInt(int value){
            this->mp3_blocksize = value;
        }

        /// Defines the callback function
        void setCallback(void (*fn)(MetaDataType info, const char* str, int len)) {
            callback = fn;
        }

        // Resets all counters and restarts the prcessing
        void begin() override {
            clear();
            LOGI("mp3_blocksize: %d", mp3_blocksize);
        }

        void end() override {
            clear();
        }

        virtual size_t write(const uint8_t *buffer, size_t len) override {
            if (callback!=nullptr){
                for (int j=0;j<len;j++){
                    writeChar((char)buffer[j]);
                }
            }
            return len;
        }

    protected:
        Status status = WriteData;
        void (*callback)(MetaDataType info, const char* str, int len);
        char* metaData=nullptr;
        int totalData = 0;
        int mp3_blocksize = 0;
        int metaDataMaxLen = 0;
        int metaDataLen = 0;
        int metaDataPos = 0;

        void clear() {
            status = WriteData;
            totalData = 0;
            metaDataLen=0;
            metaDataPos=0;
        }

        // character based state engine
        virtual void writeChar(char ch){
            switch(status){
                case WriteData:
                    // TODO write out data
                    ++totalData;
                    if (totalData>=mp3_blocksize){
                        totalData = 0;
                        status = SetupSize;
                    }   
                    break;

                case SetupSize:
                    totalData = 0;
                    metaDataPos = 0;
                    metaDataLen = metaSize(ch);
                    if (metaDataLen>0 && metaDataLen<70){
                        setupMetaData(metaDataLen);
                        status = WriteMetaData;
                    } else {
                        status = WriteData;
                    }
                    break;

                case WriteMetaData:
                    metaData[metaDataPos++]=ch;
                    if (metaDataPos>=metaDataLen){
                        processMetaData(metaData, metaDataLen);
                        status = WriteData;
                    }
                    break;
            }
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
            if (meta_size>0){
                if (metaData==nullptr){
                    metaData = new prog_char[meta_size+1];
                    metaDataMaxLen = meta_size;
                    LOGI("%d", metaDataMaxLen);
                } else {
                    if (meta_size>metaDataMaxLen){
                        delete []metaData; 
                        metaData = new char[meta_size+1];
                        metaDataMaxLen = meta_size;
                        LOGI("%d", metaDataMaxLen);
                    }
                }
                memset(metaData, 0, meta_size);
            }
        }

        /// e.g. StreamTitle=' House Bulldogs - But your love (Radio Edit)';StreamUrl='';
        virtual void processMetaData( char* metaData, int len) {
            metaData[len]=0;
            if (isAscii(metaData, 12)){
                LOGI("%s", metaData);
                Str meta((char*)metaData);
                int start = meta.indexOf("StreamTitle=");
                if (start>=0){
                    start+=12;
                }
                int end = meta.indexOf("';");
                if (start>=0 && end>start){
                    metaData[end]=0;
                    callback(Title, (const char*)metaData+start+1, end-start);
                }
            } else {
                LOGW("Unexpected Data");
            }
        }


};

}