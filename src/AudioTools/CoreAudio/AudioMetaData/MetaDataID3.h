
/**
 * @brief Parser for MP3 ID3 Meta Data: The goal is to implement a simple API which provides the title, artist, albmum and the Genre
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#pragma once
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include "AbstractMetaData.h"


/** 
 * @defgroup metadata-id3 ID3 
 * @ingroup metadata
 * @brief ID3 Metadata
 **/


namespace audio_tools {

// String array with genres 
static const char *genres[] = { "Classic Rock", "Country", "Dance", "Disco", "Funk", "Grunge", "Hip-Hop", "Jazz", "Metal", "New Age", "Oldies", "Other", "Pop", "R&B", "Rap", "Reggae", "Rock", "Techno", "Industrial", "Alternative", "Ska", "Death Metal", "Pranks", "Soundtrack", "Euro-Techno", "Ambient", "Trip-Hop", "Vocal", "Jazz+Funk", "Fusion", "Trance", "Classical", "Instrumental", "Acid", "House", "Game", "Sound Clip", "Gospel", "Noise", "Alternative Rock", "Bass", "Soul", "Punk", "Space", "Meditative", "Instrumental Pop", "Instrumental Rock", "Ethnic", "Gothic", "Darkwave", "Techno-Insdustiral", "Electronic", "Pop-Folk", "Eurodance", "Dream", "Southern Rock", "Comedy", "Cult", "Gangsta", "Top 40", "Christian Rap", "Pop/Funk", "Jungle", "Native US", "Cabaret", "New Wave", "Psychadelic", "Rave", "Showtunes", "Trailer", "Lo-Fi", "Tribal", "Acid Punk", "Acid Jazz", "Polka", "Retro", "Musical", "Rock & Roll", "Hard Rock", "Folk", "Folk-Rock", "National Folk", "Swing", "Fast Fusion", "Bebob", "Latin", "Revival", "Celtic", "Bluegrass", "Avantgarde", "Gothic Rock", "Progressive Rock", "Psychedelic Rock", "Symphonic Rock", "Slow Rock", "Big Band", "Chorus", "Easy Listening", "Acoustic","Humour", "Speech", "Chanson", "Opera", "Chamber Music", "Sonata", "Symphony", "Booty Bass", "Primus", "Porn Groove", "Satire", "Slow Jam", "Club", "Tango", "Samba", "Folklore", "Ballad", "Power Ballad", "Rhytmic Soul", "Freestyle", "Duet", "Punk Rock", "Drum Solo", "Acapella", "Euro-House", "Dance Hall", "Goa", "Drum & Bass", "Club-House", "Hardcore", "Terror", "Indie", "BritPop", "Negerpunk", "Polsk Punk", "Beat", "Christian Gangsta", "Heavy Metal", "Black Metal", "Crossover", "Contemporary C", "Christian Rock", "Merengue", "Salsa", "Thrash Metal", "Anime", "JPop", "SynthPop" };

/// current status of the parsing @ingroup metadata-id3
enum ParseStatus { TagNotFound, PartialTagAtTail, TagFoundPartial, TagFoundComplete, TagProcessed};


/// ID3 verion 1 TAG (130 bytes)
/// @ingroup metadata-id3
struct ID3v1 {
    char header[3]; // TAG
    char title[30];
    char artist[30];
    char album[30];
    char year[4];
    char comment[30];
    char zero_byte[1];    
    char track[1];    
    char genre;    
};


/// ID3 verion 1 Enchanced TAG (227 bytes)
/// @ingroup metadata-id3
struct ID3v1Enhanced {
    char header[4]; // TAG+
    char title[60];
    char artist[60];
    char album[60];
    char speed;
    char genre[30];    
    char start[6];
    char end[6];
};


/**
 * @brief ID3 Meta Data Common Functionality
 * @ingroup metadata-id3
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
class MetaDataID3Base  {
  public:

    MetaDataID3Base() = default;

    void setCallback(void (*fn)(MetaDataType info, const char* str, int len)) {
        callback = fn;
        armed = fn!=nullptr;
    }

  protected:
    void (*callback)(MetaDataType info, const char* title, int len);
    bool armed = false;

    /// find the tag position in the string - if not found we return -1;
    int findTag(const char* tag, const char*str, size_t len){
        if (tag==nullptr || str==nullptr || len<=0) return -1;
        // The tags are usally in the first 500 bytes - we limit the search
        if (len>1600){
            len = 1600;
        }
        size_t tag_len = strlen(tag);
        if (tag_len >= len) return -1;
        for (size_t j=0;j<=len-tag_len-1;j++){
            if (memcmp(str+j,tag, tag_len)==0){
                return j;
            }
        }
        return -1;
    }

};


/**
 * @brief Simple ID3 Meta Data API which supports ID3 V1
 * @ingroup metadata-id3
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class MetaDataID3V1  : public MetaDataID3Base {
  public:

    MetaDataID3V1() = default;

    /// (re)starts the processing
    void begin() {
        end();
        status = TagNotFound;
        use_bytes_of_next_write = 0;
        memset(tag_str, 0, 5);
    }

    /// Ends the processing and releases the memory
    void end() {
        if (tag!=nullptr){
            delete tag;
            tag = nullptr;
        }
        if (tag_ext!=nullptr){
            delete tag_ext;
            tag_ext = nullptr;
        }
    }

    /// provide the (partial) data which might contain the meta data
    size_t write(const uint8_t* data, size_t len){
        if (armed){ 
            switch(status){
                case TagNotFound:
                    processTagNotFound(data,len);
                    break;
                case PartialTagAtTail:
                    processPartialTagAtTail(data,len);
                    break;
                case TagFoundPartial:
                    processTagFoundPartial(data,len);
                    break;
                default:                
                    // do nothing
                    break;
            }
        }
        return len;
    }

  protected:
    int use_bytes_of_next_write = 0;
    char tag_str[5] = "";
    ID3v1 *tag = nullptr;
    ID3v1Enhanced *tag_ext = nullptr;
    ParseStatus status = TagNotFound;


    /// try to find the metatdata tag in the provided data
    void processTagNotFound(const uint8_t* data, size_t len) {
        // find tags
        int pos = findTag("TAG+",(const char*) data, len);
        if (pos>0){
            tag_ext = new ID3v1Enhanced();
            if (tag_ext!=nullptr){
                if (len-pos>=sizeof(ID3v1Enhanced)){
                    memcpy(tag,data+pos,sizeof(ID3v1Enhanced));
                    processnotifyAudioChange();                    
                } else {
                    use_bytes_of_next_write = min(sizeof(ID3v1Enhanced), len-pos);
                    memcpy(tag_ext, data+pos, use_bytes_of_next_write);
                    status = TagFoundPartial;
                }
            }
        } else {
            pos = findTag("TAG", (const char*) data, len);
            if (pos>0){
                tag = new ID3v1();
                if (tag!=nullptr){
                    if (len-pos>=sizeof(ID3v1)){
                        memcpy(tag,data+pos,sizeof(ID3v1));
                        processnotifyAudioChange();                    
                    } else {
                        use_bytes_of_next_write = min(sizeof(ID3v1), len-pos);
                        memcpy(tag,data+pos,use_bytes_of_next_write);
                        status = TagFoundPartial;
                        
                    }
                }
            }
        }

        // we did not find a full tag we check if the end might start with a tag
        if (pos==-1){
            if (data[len-3] == 'T' && data[len-2] == 'A' && data[len-1] == 'G'){
                strcpy(tag_str,"TAG");
                status = TagFoundPartial;
            } else if (data[len-2] == 'T' && data[len-1] == 'A' ){
                strcpy(tag_str,"TA");
                status = TagFoundPartial;                
            } else if (data[len-1] == 'T'){            
                strcpy(tag_str,"T");
                status = TagFoundPartial;
            }
        }
    }

    /// We had part of the start tag at the end of the last write, now we get the full data
    void processPartialTagAtTail(const uint8_t* data, size_t len) {
        int tag_len = strlen(tag_str);
        int missing = 5 - tag_len;
        strncat((char*)tag_str, (char*)data, missing);
        if (strncmp((char*)tag_str, "TAG+", 4)==0){
            tag_ext = new ID3v1Enhanced();
            memcpy(tag,tag_str, 4);
            memcpy(tag,data+len,sizeof(ID3v1Enhanced));
            processnotifyAudioChange();                    
        } else if (strncmp((char*)tag_str,"TAG",3)==0){
            tag = new ID3v1();
            memcpy(tag,tag_str, 3);
            memcpy(tag,data+len,sizeof(ID3v1));
            processnotifyAudioChange();                    
        }
    }

    /// We have the beginning of the metadata and need to process the remainder
    void processTagFoundPartial(const uint8_t* data, size_t len) {
        if (tag!=nullptr){
            int remainder = sizeof(ID3v1) - use_bytes_of_next_write;
            memcpy(tag,data+use_bytes_of_next_write,remainder);
            processnotifyAudioChange();                 
            use_bytes_of_next_write = 0;   
        } else if (tag_ext!=nullptr){
            int remainder = sizeof(ID3v1Enhanced) - use_bytes_of_next_write;
            memcpy(tag_ext,data+use_bytes_of_next_write,remainder);
            processnotifyAudioChange();                 
            use_bytes_of_next_write = 0;   
        }
    }

    /// executes the callbacks
    void processnotifyAudioChange() {
        if (callback==nullptr) return;

        if (tag_ext!=nullptr){
            callback(Title, tag_ext->title,strnlength(tag_ext->title,60));
            callback(Artist, tag_ext->artist,strnlength(tag_ext->artist,60));
            callback(Album, tag_ext->album,strnlength(tag_ext->album,60));
            callback(Genre, tag_ext->genre,strnlength(tag_ext->genre,30));
            delete tag_ext;
            tag_ext = nullptr;
            status = TagProcessed;
        }

        if (tag!=nullptr){
            callback(Title, tag->title,strnlength(tag->title,30));
            callback(Artist, tag->artist,strnlength(tag->artist,30));
            callback(Album, tag->album,strnlength(tag->album,30));        
            uint16_t genre = tag->genre;
            if (genre < sizeof(genres)){
                const char* genre_str = genres[genre];
                callback(Genre, genre_str,strlen(genre_str));
            }
            delete tag;
            tag = nullptr;
            status = TagProcessed;
        }
    }

};

// -------------------------------------------------------------------------------------------------------------------------------------

#define UnsynchronisationFlag 0x40
#define ExtendedHeaderFlag 0x20
#define ExperimentalIndicatorFlag 0x10
        
// Relevant v2 Tags        
static const char* id3_v2_tags[] = {"TALB", "TOPE", "TPE1", "TIT2", "TCON"};


// ID3 verion 2 TAG Header (10 bytes)  @ingroup metadata-id3
struct ID3v2 {
    uint8_t header[3]; // ID3
    uint8_t version[2];
    uint8_t flags;
    uint8_t size[4];
};

// /// ID3 verion 2 Extended Header 
// struct ID3v2ExtendedHeader {
//     uint8_t size[4];
//     uint16_t flags;
//     uint32_t padding_size;
// }; 


// ID3 verion 2 Tag  
struct ID3v2Frame {
    uint8_t id[4]; 
    uint8_t size[4];
    uint16_t flags;
}; 

// ID3 verion 2 Tag  
struct ID3v2FrameString {
    uint8_t id[4]; 
    uint8_t size[4];
    uint16_t flags;

// encoding:
// 00 – ISO-8859-1 (ASCII).
// 01 – UCS-2 (UTF-16 encoded Unicode with BOM), in ID3v2.2 and ID3v2.3.
// 02 – UTF-16BE encoded Unicode without BOM, in ID3v2.4.
// 03 – UTF-8 encoded Unicode, in ID3v2.4.
    uint8_t encoding; // encoding byte for strings
}; 

static const int ID3FrameSize = 11;

/**
 * @brief Simple ID3 Meta Data API which supports ID3 V2: We only support the "TALB", "TOPE", "TIT2", "TCON" tags
 * @ingroup metadata-id3
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class MetaDataID3V2 : public MetaDataID3Base  {
  public:
    MetaDataID3V2() = default;

    /// (re)starts the processing
    void begin() {
        status = TagNotFound;
        use_bytes_of_next_write = 0;
        actual_tag = nullptr;
        tag_active = false;
        tag_processed = false;
        result.resize(result_size);
    }
    
    /// Ends the processing and releases the memory
    void end() {
        status = TagNotFound;
        use_bytes_of_next_write = 0;
        actual_tag = nullptr;
        tag_active = false;
        tag_processed = false;
    }

    /// provide the (partial) data which might contain the meta data
    size_t write(const uint8_t* data, size_t len){
        if (armed){ 
            switch(status){
                case TagNotFound:
                    processTagNotFound(data,len);
                    break;
                case PartialTagAtTail:
                    processPartialTagAtTail(data,len);
                    break;
                default:                
                    // do nothing
                    break;
            }
        }
        return len;
    }

    /// provides the ID3v2 header
    ID3v2 header() {
        return tagv2;
    }

    /// provides the current frame header
    ID3v2FrameString frameHeader() {
        return frame_header;
    }

    /// returns true if the tag has been provided
    bool isProcessed() {
        return tag_processed;
    }

    /// Defines the result buffer size (default is 256);
    void resize(int size){
        result_size = size;
        if (result.size()==0) {
            result.resize(result_size);
        }
    }

  protected:
    ID3v2 tagv2;
    bool tag_active = false;
    bool tag_processed = false;
    ParseStatus status = TagNotFound;
    const char* actual_tag;
    ID3v2FrameString frame_header;
    int use_bytes_of_next_write = 0;
    int result_size = 256;
    Vector<char> result{0};
    uint64_t total_len = 0;
    uint64_t end_len = 0;

    // calculate the synch save size
    uint32_t calcSize(uint8_t chars[4]) {
        uint32_t byte0 = chars[0];
        uint32_t byte1 = chars[1];
        uint32_t byte2 = chars[2];
        uint32_t byte3 = chars[3];
        return byte0 << 21 | byte1 << 14 | byte2 << 7 | byte3;
    }

    /// try to find the metatdata tag in the provided data
    void processTagNotFound(const uint8_t* data, size_t len) {

        // activate tag processing when we have an ID3 Tag
        if (!tag_active && !tag_processed){
            int pos = findTag("ID3", (const char*) data, len);
            if (pos>=0){
                // fill v2 tag header
                tag_active = true;  
                // if we have enough data for the header we process it
                if (len>=pos+sizeof(ID3v2)){
                    memcpy(&tagv2, data+pos, sizeof(ID3v2));   
                    end_len = total_len + calcSize(tagv2.size);
                }
            }
        }

        // deactivate tag processing when we are out of range
        if (end_len>0 && total_len>end_len){
            tag_active = false;
            tag_processed = false;
        }

        
        if (tag_active){
            // process all tags in current buffer
            const char* partial_tag = nullptr;
            for (const char* tag : id3_v2_tags){
                actual_tag = tag;
                int tag_pos = findTag(tag, (const char*)  data, len);

                if (tag_pos>0 && tag_pos+sizeof(ID3v2Frame)<=len){
                    // setup tag header
                    memmove(&frame_header, data+tag_pos, sizeof(ID3v2FrameString));

                    // get tag content
                    if(calcSize(frame_header.size) <= len){
                        int l = min(calcSize(frame_header.size)-1, (uint32_t) result.size());
                        memset(result.data(), 0, result.size());
                        strncpy((char*)result.data(), (char*) data+tag_pos+ID3FrameSize, l);
                        int checkLen = min(l, 10);
                        if (isAscii(checkLen)){
                            processnotifyAudioChange();
                        } else {
                            LOGW("TAG %s ignored", tag);
                        }
                    } else {
                        partial_tag = tag;
                        LOGI("%s partial tag", tag);
                    }
                } 
            }
            
            // save partial tag information so that we process the remainder with the next write
            if (partial_tag!=nullptr){
                int tag_pos = findTag(partial_tag, (const char*)  data, len);
                memmove(&frame_header, data+tag_pos, sizeof(ID3v2FrameString));
                int size = min(len - tag_pos, (size_t) calcSize(frame_header.size)-1); 
                strncpy((char*)result.data(), (char*)data+tag_pos+ID3FrameSize, size);
                use_bytes_of_next_write = size;
                status = PartialTagAtTail;
            }
        }
    
        total_len += len;

    }
    
    int isCharAscii(int ch) {return ch >= 0 && ch < 128; }

    /// Make sure that the result is a valid ASCII string
    bool isAscii(int l){
        // check on first 10 characters
        int m = l < 5 ? l : 10;
        for (int j=0; j<m; j++){
            if (!isCharAscii(result[j])) {
                return false;
            }
        }
        return true;
    }

    /// We have the beginning of the metadata and need to process the remainder
    void processPartialTagAtTail(const uint8_t* data, size_t len) {
        int remainder = calcSize(frame_header.size) - use_bytes_of_next_write;
        memcpy(result.data()+use_bytes_of_next_write, data, remainder);
        processnotifyAudioChange();    

        status = TagNotFound;
        processTagNotFound(data+use_bytes_of_next_write, len-use_bytes_of_next_write);
    }

    /// For the time beeing we support only ASCII and UTF8
    bool encodingIsSupported(){
        return frame_header.encoding == 0 || frame_header.encoding == 3;
    }

    int strpos(char* str, const char* target) {
        char* res = strstr(str, target); 
        return (res == nullptr) ? -1 : res - str;
    }


    /// executes the callbacks
    void processnotifyAudioChange() {
        if (callback!=nullptr && actual_tag!=nullptr && encodingIsSupported()){
            LOGI("callback %s",actual_tag);
            if (memcmp(actual_tag,"TALB",4)==0)
                callback(Album, result.data(), strnlength(result.data(), result.size()));
            else if (memcmp(actual_tag,"TPE1",4)==0)
                callback(Artist, result.data(),strnlength(result.data(), result.size()));
            else if (memcmp(actual_tag,"TOPE",4)==0)
                callback(Artist, result.data(),strnlength(result.data(), result.size()));
            else if (memcmp(actual_tag,"TIT2",4)==0)
                callback(Title, result.data(),strnlength(result.data(), result.size()));
            else if (memcmp(actual_tag,"TCON",4)==0) {
                if (result[0]=='('){
                    // convert genre id to string
                    int end_pos = strpos((char*)result.data(), ")");
                    if (end_pos>0){
                        // we just use the first entry
                        result[end_pos]=0;
                        int idx = atoi(result.data()+1);
                        if (idx>=0 && idx< (int)sizeof(genres)){
                            strncpy((char*)result.data(), genres[idx], result.size());
                        }
                    }
                }
                callback(Genre, result.data(),strnlength(result.data(), result.size()));
            } 
        }
    }

};

/**
 * @brief Simple ID3 Meta Data Parser which supports ID3 V1 and V2 and implements the Stream interface. You just need to set the callback(s) to receive the result 
 * and copy the audio data to this stream.
 * @ingroup metadata-id3
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
class MetaDataID3 : public AbstractMetaData {
  public:

    MetaDataID3() = default;
    
    ~MetaDataID3(){
        end();
    }

    void setCallback(void (*fn)(MetaDataType info, const char* str, int len)) {
        id3v1.setCallback(fn);        
        id3v2.setCallback(fn);        
    }

    void setFilter(ID3TypeSelection sel) {
        this->filter = sel;
    }

    void begin() {
        TRACEI();
        id3v1.begin();
        id3v2.begin();
    }

    void end() {
        TRACEI();
        id3v1.end();
        id3v2.end();
    }

    /// Provide tha audio data to the API to parse for Meta Data
    virtual size_t write(const uint8_t *data, size_t len){
        TRACED();
        if (filter & SELECT_ID3V2) id3v2.write(data, len);
        if (!id3v2.isProcessed()) {
            if (filter & SELECT_ID3V1) id3v1.write(data, len);
        }
        return len;
    }

    /// Defines the ID3V3 result buffer size (default is 256);
    void resize(int size){
        id3v2.resize(size);
    }

  protected:
    MetaDataID3V1 id3v1;
    MetaDataID3V2 id3v2;
    int filter = SELECT_ID3;
};

} // namespace