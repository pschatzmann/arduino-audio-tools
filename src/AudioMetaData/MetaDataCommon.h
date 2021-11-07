#pragma once

#include <stdint.h>

/// Type of meta info
enum MetaDataType { Title, Artist, Album, Genre, Name, Description };

/// Test Description for meta info
const char* MetaDataTypeStr[] = {"Title", "Artist", "Album", "Genre","Name", "Description"};


class MetaDataCommon {
    public:
        virtual void setCallback(void (*fn)(MetaDataType info, const char* str, int len)) = 0 ;
        virtual void begin() = 0;
        virtual void end() = 0;
        virtual size_t write(const uint8_t *data, size_t length) = 0;
        // select Icecast/Shoutcast Metadata
        virtual void setIcyMetaInt(int value){}

};