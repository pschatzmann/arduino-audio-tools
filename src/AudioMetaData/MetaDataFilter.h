#pragma once
#include "AudioTools/AudioLogger.h"

namespace audio_tools {

/**
 * @brief Class which filters out ID3v1 and ID3v2 Metadata and provides only the audio data to the decoder
 * @ingroup metadata
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
template <class Decoder>
class MetaDataFilter {
    public:
        /// Default Constructor
        MetaDataFilter() = default;

        /// Constructor which assigns the decoder
        MetaDataFilter(Decoder *decoder){
            setDecoder(decoder);
        }

        /// Defines the decoder to which we write the data
        void setDecoder(Decoder *decoder){
            p_decoder = decoder;
        }

        /// (Re)starts the processing
        void begin() {
            TRACED();
            start = 0;
        }

        /// Writes the data to the decoder 
        size_t write(uint8_t* data, size_t len){
            TRACED();
            if (p_decoder==nullptr) return 0;
            int pos=0; int meta_len=0;
            if (findTag(data, len, pos, meta_len)){
                LOGD("pos: %d len: %d",pos, meta_len);
                if (start<pos){
                    p_decoder->write(data+start,pos);
                }

                int start_idx2 = pos+meta_len;
                int len2 =  len-start_idx2;    
                if (start_idx2<len){
                    // we still have some audio to write
                    p_decoder->write(data+start_idx2,len2);
                } else {
                    // ignore audio of next write
                    start = meta_len - len2;
                }
            } else {
                // ignore start number of characters
                if (start>=len){
                    start -= len;
                } else {
                    p_decoder->write(data+start,len-start);
                    start = 0;
                }
            }
            return len;
        }

    protected:
        Decoder *p_decoder=nullptr;
        enum MetaType {TAG, TAG_PLUS, ID3};
        int start = 0;
        /// ID3 verion 2 TAG Header (10 bytes)
        struct ID3v2 {
            uint8_t header[3]; // ID3
            uint8_t version[2];
            uint8_t flags;
            uint8_t size[4];
        } tagv2;

        /// determines if the data conatins a ID3v1 or ID3v2 tag
        bool findTag(uint8_t* data, size_t len, int &pos_tag, int &meta_len){
            MetaType tag_type;
            if (find((const char*)data, len, pos_tag, tag_type)){
                switch(tag_type){
                    case TAG:
                        LOGD("TAG");
                        meta_len = 128;
                        break;
                    case TAG_PLUS:
                        LOGD("TAG+");
                        meta_len = 227;
                        break;
                    case ID3:
                        LOGD("ID3");
                        memcpy(&tagv2, data+pos_tag, sizeof(ID3v2));   
                        meta_len = calcSizeID3v2(tagv2.size);
                        break;

                }
                return true;
            }
            return false;
        }

        // calculate the synch save size for ID3v2
        uint32_t calcSizeID3v2(uint8_t chars[4]) {
            uint32_t byte0 = chars[0];
            uint32_t byte1 = chars[1];
            uint32_t byte2 = chars[2];
            uint32_t byte3 = chars[3];
            return byte0 << 21 | byte1 << 14 | byte2 << 7 | byte3;
        }

        /// find the tag position in the string;
        bool find(const char*str, size_t len, int &pos, MetaType &type){
            if (str==nullptr || len<=0) return false;
            for (size_t j=0;j<=len-3;j++){
                if (str[j]=='T' && str[j+1]=='A' && str[j+2]=='G'){
                    type = str[j+3]=='+' ? TAG_PLUS : TAG;
                    return true;
                } else if (str[j]=='I' && str[j+1]=='D' && str[j+2]=='3'){
                    type = ID3;
                    return true;
                }
            }
            return false;
        }
};

}