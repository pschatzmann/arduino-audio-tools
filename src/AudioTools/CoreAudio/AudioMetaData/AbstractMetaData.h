#pragma once

#include <stdint.h>

namespace audio_tools {

/// Enum to filter by type of metadata  @ingroup metadata
enum ID3TypeSelection { SELECT_ID3V1=0b001, SELECT_ID3V2=0b010, SELECT_ID3=0b011, SELECT_ICY=0b100, SELECT_ANY=0b111 };

/// Type of meta info @ingroup metadata
enum MetaDataType { Title, Artist, Album, Genre, Name, Description };

// Description for meta info 
static const char* MetaDataTypeStr[] = {"Title", "Artist", "Album", "Genre","Name", "Description"};

/// Converts the MetaDataType to a string @ingroup metadata
static const char *toStr(MetaDataType t){
    return MetaDataTypeStr[t];
}

/// unfortunatly strnlen or strnlen_s is not available in all implementations
static size_t strnlength (const char* s, size_t n)  { 
    size_t i;
    for (i = 0; i < n && s[i] != '\0'; i++)
        continue;
    return i;
}


/**
 * @brief Common Metadata methods
 * @ingroup metadata
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
        virtual size_t write(const uint8_t *data, size_t len) = 0;
        // select Icecast/Shoutcast Metadata
        virtual void setIcyMetaInt(int value){}

};



}