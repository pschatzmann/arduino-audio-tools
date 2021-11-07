#pragma once

#include "MetaDataCommon.h"

namespace audio_tools {

/**
 * Icecast/Shoutcast Metadata Handling
 * Output Class which splits the data into audio and metadata.
 * see https://www.codeproject.com/Articles/11308/SHOUTcast-Stream-Ripper
 */ 
class MetaDataICY : public MetaDataCommon {
    public:
        MetaDataICY() = default;

        /// We just process the Metadata and ignore the audio info
        MetaDataICY(int metaint){
            mp3_blocksize = metaint;
        }

        ~MetaDataICY(){
            if (metaData!=nullptr) delete[]metaData;
        }
        
        /// Defines the ICE metaint value which is provided by the web call!
        void setIcyMetaInt(int value){
            this->mp3_blocksize = value;
        }

        // Resets all counters and restarts the prcessing
        void begin() override {
            clear();
        }

        void end() override {
            clear();
        }

         virtual size_t write(const uint8_t *buffer, size_t len) override {
            int open = len;
            // if the buffer contains a remainder of metadata we need to process it
            if (metaDataOpen>0){
                memcpy(metaData+metaDataAvailable, buffer, metaDataOpen);
                open -= metaDataOpen;
                total = 0;
                processMetaData((uint8_t* )metaData, metaDataLen);
            }

            if (total+open<=mp3_blocksize) {
                // all data is sound data
                total+=open;
            } else {
                // we have some metadata in the buffer
                int currentBlockLen = mp3_blocksize - total;
                open-=currentBlockLen;

                int pos = currentBlockLen;
                metaDataLen = metaSize(buffer[pos]);
                open--;
                pos++;

                if (metaDataLen<open){
                    // we have the full metadata available
                    processMetaData((uint8_t* )buffer+pos, metaDataLen);
                    open -= metaDataLen;
                    pos+=metaDataLen;
                    total = 0;

                } else {
                    // we have only part of the metadata available
                    setupMetaData(metaDataLen); 
                    metaDataAvailable = open;               
                    metaDataOpen = metaDataLen-open;
                    memcpy(metaData, buffer+pos, open);
                }
            }
        }

        void setCallback(void (*fn)(MetaDataType info, const char* str, int len)) {
            callback = fn;
        }

    protected:
        int mp3_blocksize = 0;
        size_t total = 0;
        void (*callback)(MetaDataType info, const char* str, int len);
        uint8_t* metaData=nullptr;
        int metaDataMaxLen=0;
        int metaDataLen=0;
        int metaDataOpen=0;
        int metaDataAvailable=0;
        
        void clear() {
            total = 0;
            metaDataLen=0;
            metaDataOpen=0;
            metaDataAvailable=0;
        }

        /// determines the meta data size from the size byte
        int metaSize(uint8_t metaSize){
            return metaSize*16;
        }

        /// e.g. StreamTitle=' House Bulldogs - But your love (Radio Edit)';StreamUrl='';
        void processMetaData( uint8_t* metaData, int len) {
            LOGI("%s", metaData);
        }

        /// allocates the memory to store the metadata
        void setupMetaData(int meta_size) {
            if (metaData==nullptr){
                metaData = new uint8_t[meta_size+1];
                metaDataMaxLen = meta_size;
            } else {
                if (meta_size>metaDataMaxLen){
                    delete []metaData; 
                    metaData = new uint8_t[meta_size+1];
                    metaDataMaxLen = meta_size;
                }
            }
        }

};

}