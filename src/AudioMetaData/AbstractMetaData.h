#pragma once

#include <stdint.h>

namespace audio_tools {

enum ID3TypeSelection { SELECT_ID3V1=0b001, SELECT_ID3V2=0b010, SELECT_ID3=0b011, SELECT_ICY=0b100, SELECT_ANY=0b111 };

/// Type of meta info
enum MetaDataType { Title, Artist, Album, Genre, Name, Description };

/// Test Description for meta info
INLINE_VAR const char* MetaDataTypeStr[] = {"Title", "Artist", "Album", "Genre","Name", "Description"};

/// Converts the MetaDataType to a string
INLINE_VAR const char *toStr(MetaDataType t){
    return MetaDataTypeStr[t];
}

/**
 * @brief Common Metadata methods
 * 
 */
class AbstractMetaData {
    public:
        virtual  ~AbstractMetaData() = default;

        // defines the callback which provides the metadata information
        virtual void setCallback(void (*fn)(MetaDataType info, const char* str, int len)) = 0 ;
        // starts the processing
        virtual void begin() = 0;
        // ends the processing
        virtual void end() = 0;
        // provide audio data which contains the metadata to be extracted
        virtual size_t write(const uint8_t *data, size_t length) = 0;
        // select Icecast/Shoutcast Metadata
        virtual void setIcyMetaInt(int value){}

};

}