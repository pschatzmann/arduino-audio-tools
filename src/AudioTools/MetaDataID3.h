
/**
 * @brief Parser for MP3 ID3 Meta Data: The goal is to implement a simple API which provides the title, artist, albmum and the Genre
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#pragma once
#include <string.h>
#include <stdint.h>

namespace audio_tools {


/// ID3 verion 1 TAG (130 bytes)
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


/// String array with genres
const char *genres[] = { "Classic Rock", "Country", "Dance", "Disco", "Funk", "Grunge", "Hip-Hop", "Jazz", "Metal", "New Age", "Oldies", "Other", "Pop", "R&B", "Rap", "Reggae", "Rock", "Techno", "Industrial", "Alternative", "Ska", "Death Metal", "Pranks", "Soundtrack", "Euro-Techno", "Ambient", "Trip-Hop", "Vocal", "Jazz+Funk", "Fusion", "Trance", "Classical", "Instrumental", "Acid", "House", "Game", "Sound Clip", "Gospel", "Noise", "Alternative Rock", "Bass", "Soul", "Punk", "Space", "Meditative", "Instrumental Pop", "Instrumental Rock", "Ethnic", "Gothic", "Darkwave", "Techno-Insdustiral", "Electronic", "Pop-Folk", "Eurodance", "Dream", "Southern Rock", "Comedy", "Cult", "Gangsta", "Top 40", "Christian Rap", "Pop/Funk", "Jungle", "Native US", "Cabaret", "New Wave", "Psychadelic", "Rave", "Showtunes", "Trailer", "Lo-Fi", "Tribal", "Acid Punk", "Acid Jazz", "Polka", "Retro", "Musical", "Rock & Roll", "Hard Rock", "Folk", "Folk-Rock", "National Folk", "Swing", "Fast Fusion", "Bebob", "Latin", "Revival", "Celtic", "Bluegrass", "Avantgarde", "Gothic Rock", "Progressive Rock", "Psychedelic Rock", "Symphonic Rock", "Slow Rock", "Big Band", "Chorus", "Easy Listening", "Acoustic","Humour", "Speech", "Chanson", "Opera", "Chamber Music", "Sonata", "Symphony", "Booty Bass", "Primus", "Porn Groove", "Satire", "Slow Jam", "Club", "Tango", "Samba", "Folklore", "Ballad", "Power Ballad", "Rhytmic Soul", "Freestyle", "Duet", "Punk Rock", "Drum Solo", "Acapella", "Euro-House", "Dance Hall", "Goa", "Drum & Bass", "Club-House", "Hardcore", "Terror", "Indie", "BritPop", "Negerpunk", "Polsk Punk", "Beat", "Christian Gangsta", "Heavy Metal", "Black Metal", "Crossover", "Contemporary C", "Christian Rock", "Merengue", "Salsa", "Thrash Metal", "Anime", "JPop", "SynthPop" };

/// current status of the parsing
enum ParseStatus { TagNotFound, PartialTagAtTail, TagFoundPartial, TagFoundComplete, TagProcessed};


/**
 * @brief ID3 Meta Data Common Functionality
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
class MetaDataID3Base {
  public:

    MetaDataID3Base() = default;

    void setTitleCallback(void (*fn)(const char* str, int len)) {
        titleCB = fn;
        armed = true;
    }

    void setArtistCallback(void (*fn)(const char* str, int len)) {
        artistCB = fn;
        armed = true;
    }

    void setAlbumCallback(void (*fn)(const char* str, int len)) {
        albumCB = fn;
        armed = true;
    }

    void setGenreCallback(void (*fn)(const char* str, int len)) {
        genreCB = fn;
        armed = true;
    }

  protected:
    void (*titleCB)(const char* title, int len);
    void (*artistCB)(const char* artist, int len);
    void (*albumCB)(const char* album, int len);
    void (*genreCB)(const char* genre, int len);
    bool armed = false;

    /// find the tag position in the string - if not found we return -1;
    int findTag(const char* tag, const char*str, size_t len){
        int tag_len = strlen(tag);
        for (int j=0;j<=len-tag_len;j++){
            if (strncmp(str+j,tag, tag_len)==0){
                return j;
            }
        }
        return -1;
    }

};


/**
 * @brief Simple ID3 Meta Data API which supports ID3 V1
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
                    processNotify();                    
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
                        processNotify();                    
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
            processNotify();                    
        } else if (strncmp((char*)tag_str,"TAG",3)==0){
            tag = new ID3v1();
            memcpy(tag,tag_str, 3);
            memcpy(tag,data+len,sizeof(ID3v1));
            processNotify();                    
        }
    }

    /// We have the beginning of the metadata and need to process the remainder
    void processTagFoundPartial(const uint8_t* data, size_t len) {
        if (tag!=nullptr){
            int remainder = sizeof(ID3v1) - use_bytes_of_next_write;
            memcpy(tag,data+use_bytes_of_next_write,remainder);
            processNotify();                 
            use_bytes_of_next_write = 0;   
        } else if (tag_ext!=nullptr){
            int remainder = sizeof(ID3v1Enhanced) - use_bytes_of_next_write;
            memcpy(tag_ext,data+use_bytes_of_next_write,remainder);
            processNotify();                 
            use_bytes_of_next_write = 0;   
        }
    }

    /// executes the callbacks
    void processNotify() {
        if (tag_ext!=nullptr){
            if (titleCB!=nullptr)
                titleCB(tag_ext->title,strnlen(tag_ext->title,60));
            if (artistCB!=nullptr)
                artistCB(tag_ext->artist,strnlen(tag_ext->artist,60));
            if (albumCB!=nullptr)
                albumCB(tag_ext->album,strnlen(tag_ext->album,60));
            if (genreCB!=nullptr)
                genreCB(tag_ext->genre,strnlen(tag_ext->genre,30));
            delete tag_ext;
            tag_ext = nullptr;
            status = TagProcessed;
        }

        if (tag!=nullptr){
            if (titleCB!=nullptr)
                titleCB(tag->title,strnlen(tag->title,30));
            if (artistCB!=nullptr)
                artistCB(tag->artist,strnlen(tag->artist,30));
            if (albumCB!=nullptr)
                albumCB(tag->album,strnlen(tag->album,30));
        
            int genre = tag->genre;
            if (genreCB!=nullptr &&  genre < sizeof(genres)){
                const char* genre_str = genres[genre];
                genreCB(genre_str,strlen(genre_str));
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
        
/// Relevant v2 Tags        
const char* id3_v2_tags[] = {"TALB", "TOPE", "TIT2", "TCON"};

/// ID3 verion 2 TAG Header (10 bytes)
struct ID3v2 {
    uint8_t header[3]; // ID3
    uint8_t version[2];
    uint8_t flags;
    uint32_t size_encoded;
    uint32_t size() {
        return (size_encoded & 127) + 128 * ((size_encoded >> 8) & 127) + 16384 * ((size_encoded >>16) & 127) + 2097152 * ((size_encoded >> 24) & 127);
    }

};

/// ID3 verion 2 Extended Header 
struct ID3v2ExtendedHeader {
    uint32_t size;
    uint16_t flags;
    uint32_t padding_size;
}; 

/// ID3 verion 2 Tag
struct ID3v2Frame {
    uint8_t id[4]; 
    uint32_t size_encoded;
    uint16_t flags;
    uint32_t size() {
        return (size_encoded & 127) + 128 * ((size_encoded >> 8) & 127) + 16384 * ((size_encoded >>16) & 127) + 2097152 * ((size_encoded >> 24) & 127);
    }
}; 

/**
 * @brief Simple ID3 Meta Data API which supports ID3 V2: We only support the "TALB", "TOPE", "TIT2", "TCON" tags
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
    }
    

    /// Ends the processing and releases the memory
    void end() {
        status = TagNotFound;
        use_bytes_of_next_write = 0;
        actual_tag = nullptr;
        tag_active = false;
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


  protected:
    ID3v2 tagv2;
    bool tag_active = false;
    ParseStatus status = TagNotFound;
    const char* actual_tag;
    ID3v2Frame frame_header;
    int use_bytes_of_next_write = 0;
    char result[256];
    uint64_t total_len = 0;
    uint64_t end_len = 0;


    /// try to find the metatdata tag in the provided data
    void processTagNotFound(const uint8_t* data, size_t len) {

        // activate tag processing when we have an ID3 Tag
        if (!tag_active){
            int pos = findTag("ID3", (const char*) data, len);
            if (pos>0){
                // fill v2 tag header
                tag_active = true;  
                // if we have enough data for the header we process it
                if (len>=pos+sizeof(ID3v2)){
                    memcpy(&tagv2, data+pos, sizeof(ID3v2));   
                    end_len = total_len + tagv2.size();
                }
            }
        }

        // deactivate tag processing when we are out of range
        if (end_len>0 && total_len>end_len){
            tag_active = false;
        }

        
        if (tag_active){
            // process all tags in current buffer
            const char* partial_tag = nullptr;
            for (const char* tag : id3_v2_tags){
                actual_tag = tag;
                int tag_pos = findTag(tag, (const char*)  data, len);

                if (tag_pos>0 && tag_pos+sizeof(ID3v2Frame)<=len){
                    // setup tag header
                    memmove(&frame_header, data+tag_pos, sizeof(ID3v2Frame));

                    // get tag content
                    if(frame_header.size() <= len){
                        strncpy((char*)result, (char*) data+tag_pos+sizeof(ID3v2Frame), frame_header.size());
                        processNotify();
                    } else {
                        partial_tag = tag;
                    }
                }
            }
            
            // save partial tag information so that we process the remainder with the next write
            if (partial_tag!=nullptr){
                int tag_pos = findTag(partial_tag, (const char*)  data, len);
                memmove(&frame_header, data+tag_pos, sizeof(ID3v2Frame));
                int size = min(len - tag_pos,frame_header.size()); 
                strncpy((char*)result, (char*)data+tag_pos+sizeof(ID3v2Frame), size);
                use_bytes_of_next_write = size;
                status = PartialTagAtTail;
            }
        }
    
        total_len += len;

    }

    /// We have the beginning of the metadata and need to process the remainder
    void processPartialTagAtTail(const uint8_t* data, size_t len) {
        int remainder = frame_header.size() - use_bytes_of_next_write;
        memcpy(result+use_bytes_of_next_write, data, remainder);
        processNotify();    

        status = TagNotFound;
        processTagNotFound(data+use_bytes_of_next_write, len-use_bytes_of_next_write);
    }

    /// executes the callbacks
    void processNotify() {
        if (actual_tag!=nullptr){
            if (strcmp(actual_tag,"TALB")==0)
                titleCB(actual_tag,strnlen(actual_tag, 256));
            else if (strncmp(actual_tag,"TOPE",4)==0)
                artistCB(actual_tag,strnlen(actual_tag, 256));
            else if (strncmp(actual_tag,"TIT2",4)==0)
                albumCB(actual_tag,strnlen(actual_tag, 256));
            else if (strncmp(actual_tag,"TCON",4)==0)
                genreCB(actual_tag,strnlen(actual_tag, 256));
        }
    }

};


/**
 * @brief Simple ID3 Meta Data Parser which supports ID3 V1 and V2 and implements the Stream interface. You just need to set the callback(s) to receive the result 
 * and copy the audio data to this stream.
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
class MetaDataID3 : public BufferedStream {
  public:

    MetaDataID3(int buffer_size=512):BufferedStream(buffer_size) {
    }
    
    ~MetaDataID3(){
        end();
    }

    void setTitleCallback(void (*fn)(const char* str, int len)) {
        id3v1.setTitleCallback(fn);        
        id3v2.setTitleCallback(fn);        
    }

    void setArtistCallback(void (*fn)(const char* str, int len)) {
        id3v1.setArtistCallback(fn);        
        id3v2.setArtistCallback(fn);        
    }

    void setAlbumCallback(void (*fn)(const char* str, int len)) {
        id3v1.setAlbumCallback(fn);        
        id3v2.setAlbumCallback(fn);        
    }

    void setGenreCallback(void (*fn)(const char* str, int len)) {
        id3v1.setGenreCallback(fn);        
        id3v2.setGenreCallback(fn);        
    }

    void begin() {
        id3v1.begin();
        id3v2.begin();
    }

    void end() {
        id3v1.end();
        id3v2.end();
    }

  protected:
    MetaDataID3V1 id3v1;
    MetaDataID3V2 id3v2;

    /// not supported
    size_t readExt(uint8_t *data, size_t length){
        return 0;
    }

    /// Provide tha audio data to the API to parse for Meta Data
    virtual size_t writeExt(const uint8_t *data, size_t length){
        id3v1.write(data, length);
        id3v2.write(data, length);
        return length;
    }


};

} // namespace