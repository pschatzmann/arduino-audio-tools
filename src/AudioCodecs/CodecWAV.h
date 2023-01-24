#pragma once

#include "AudioCodecs/AudioEncoded.h"

#define WAV_FORMAT_PCM 0x0001
#define TAG(a, b, c, d) ((static_cast<uint32_t>(a) << 24) | (static_cast<uint32_t>(b) << 16) | (static_cast<uint32_t>(c) << 8) | (d))
#define READ_BUFFER_SIZE 512

/** 
 * @defgroup codec-wav wav
 * @ingroup codecs
 * @brief Codec wav   
**/


namespace audio_tools {

/**
 * @brief Sound information which is available in the WAV header
 * @ingroup codec-wav
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
struct WAVAudioInfo : AudioBaseInfo {
    WAVAudioInfo() = default;
    WAVAudioInfo(const AudioBaseInfo& from){
        sample_rate = from.sample_rate;    
        channels = from.channels;       
        bits_per_sample=from.bits_per_sample; 
    }

    int format=WAV_FORMAT_PCM;
    int byte_rate=0;
    int block_align=0;
    bool is_streamed=true;
    bool is_valid=false;
    uint32_t data_length=0;
    uint32_t file_size=0;
};

INLINE_VAR const char* wav_mime = "audio/wav";

/**
 * @brief Parser for Wav header data
 * for details see https://de.wikipedia.org/wiki/RIFF_WAVE
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
class WAVHeader  {
    public:
        WAVHeader() = default;

        /// Call begin when header data is complete to parse the data
        void begin(){
            LOGI("WAVHeader::begin: %u",(unsigned) len);
            this->data_pos = 0l;            
            memset((void*)&headerInfo, 0, sizeof(WAVAudioInfo));
            while (!eof()) {
                uint32_t tag, tag2, length;
                tag = read_tag();
                if (eof())
                    break;
                length = read_int32();
                if (!length || length >= 0x7fff0000) {
                    headerInfo.is_streamed = true;
                    length = ~0;
                }
                if (tag != TAG('R', 'I', 'F', 'F') || length < 4) {
                    seek(length, SEEK_CUR);
                    continue;
                }
                tag2 = read_tag();
                length -= 4;
                if (tag2 != TAG('W', 'A', 'V', 'E')) {
                    seek(length, SEEK_CUR);
                    continue;
                }
                // RIFF chunk found, iterate through it
                while (length >= 8) {
                    uint32_t subtag, sublength;
                    subtag = read_tag();
                    if (eof())
                        break;
                    sublength = read_int32();
                    length -= 8;
                    if (length < sublength)
                        break;
                    if (subtag == TAG('f', 'm', 't', ' ')) {
                        if (sublength < 16) {
                            // Insufficient data for 'fmt '
                            break;
                        }
                        headerInfo.format          = read_int16();
                        headerInfo.channels        = read_int16();
                        headerInfo.sample_rate     = read_int32();
                        headerInfo.byte_rate       = read_int32();
                        headerInfo.block_align     = read_int16();
                        headerInfo.bits_per_sample = read_int16();
                        if (headerInfo.format == 0xfffe) {
                            if (sublength < 28) {
                                // Insufficient data for waveformatex
                                break;
                            }
                            skip(8);
                            headerInfo.format = read_int32();
                            skip(sublength - 28);
                        } else {
                            skip(sublength - 16);
                        }
                        headerInfo.is_valid = true;
                    } else if (subtag == TAG('d', 'a', 't', 'a')) {
                        sound_pos = tell();
                        headerInfo.data_length = sublength;
                        if (!headerInfo.data_length || headerInfo.is_streamed) {
                            headerInfo.is_streamed = true;
                            logInfo();
                            return;
                        }
                        seek(sublength, SEEK_CUR);
                    } else {
                        skip(sublength);
                    }
                    length -= sublength;
                }
                if (length > 0) {
                    // Bad chunk?
                    seek(length, SEEK_CUR);
                }
            }
            logInfo();        
            len = 0;
        }

        /// Resets the len
        void end(){
            len = 0;
        }

        /// Adds data to the 44 byte wav header data buffer, returns the index at which the audio starts
        int write(uint8_t* data, size_t data_len){
            int write_len = min(data_len, 44 - len);
            memmove(buffer, data+len, write_len);
            len+=write_len;
            LOGI("WAVHeader::write: %u -> %d -> %d",(unsigned) data_len, write_len, (int)len);
            return write_len;
        }

        /// Returns true if the header is complete (with 44 bytes)
        bool isDataComplete() {
            return len==44;
        }

        // provides the AudioInfo
        WAVAudioInfo &audioInfo() {
            return headerInfo;
        }

    protected:
        struct WAVAudioInfo headerInfo;
        uint8_t buffer[44];
        size_t len=0;
        size_t data_pos = 0;
        size_t sound_pos = 0;

        uint32_t read_tag() {
            uint32_t tag = 0;
            tag = (tag << 8) | getChar();
            tag = (tag << 8) | getChar();
            tag = (tag << 8) | getChar();
            tag = (tag << 8) | getChar();
            return tag;
        }

        uint32_t getChar32() {
            return getChar();
        }

        uint32_t read_int32() {
            uint32_t value = 0;
            value |= getChar32() <<  0;
            value |= getChar32() <<  8;
            value |= getChar32() << 16;
            value |= getChar32() << 24;
            return value;
        }

        uint16_t read_int16() {
            uint16_t value = 0;
            value |= getChar() << 0;
            value |= getChar() << 8;
            return value;
        }

        void skip(int n) {
            int i;
            for (i = 0; i < n; i++)
                getChar();
        }

        int getChar() {
            if (data_pos<len) 
                return buffer[data_pos++];
            else
                return -1;
        }

        void seek(long int offset, int origin ){
            if (origin==SEEK_SET){
                data_pos = offset;
            } else if (origin==SEEK_CUR){
                data_pos+=offset;
            }
        }

        size_t tell() {
            return data_pos;
        }

        bool eof() {
            return data_pos>=len-1;
        }

        void logInfo(){
            LOGI("WAVHeader sound_pos: %lu", (unsigned long) sound_pos);
            LOGI("WAVHeader channels: %d ", headerInfo.channels);
            LOGI("WAVHeader bits_per_sample: %d", headerInfo.bits_per_sample);
            LOGI("WAVHeader sample_rate: %d ", headerInfo.sample_rate);
            LOGI("WAVHeader format: %d", headerInfo.format);
        }

};


/**
 * @brief WAVDecoder - We parse the header data on the first record
 * and send the sound data to the stream which was indicated in the
 * constructor. Only WAV files with WAV_FORMAT_PCM are supported!
 * @ingroup codec-wav
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class WAVDecoder : public AudioDecoder {
    public:
        /**
         * @brief Construct a new WAVDecoder object
         */

        WAVDecoder(){
            TRACED();
            this->audioBaseInfoSupport = nullptr;
        }

        /**
         * @brief Construct a new WAVDecoder object
         * 
         * @param out_stream Output Stream to which we write the decoded result
         */
        WAVDecoder(Print &out_stream, bool active=true){
            TRACED();
            this->out = &out_stream;
            this->audioBaseInfoSupport = nullptr;
            this->active = active;
        }

        /**
         * @brief Construct a new WAVDecoder object
         * 
         * @param out_stream Output Stream to which we write the decoded result
         * @param bi Object that will be notified about the Audio Formt (Changes)
         */

        WAVDecoder(Print &out_stream, AudioBaseInfoDependent &bi){
            TRACED();
            this->out = &out_stream;
            this->audioBaseInfoSupport = &bi;
        }

        /// Defines the output Stream
        void setOutputStream(Print &out_stream){
            this->out = &out_stream;
        }

        void setNotifyAudioChange(AudioBaseInfoDependent &bi){
            this->audioBaseInfoSupport = &bi;
        }


        void begin() {
            TRACED();
            isFirst = true;
            active = true;
        }

        void end() {
            TRACED();
            active = false;
        }

        const char* mime(){
            return wav_mime;
        }

        WAVAudioInfo &audioInfoEx() {
            return header.audioInfo();
        }

        AudioBaseInfo audioInfo(){
            return header.audioInfo();
        }

        virtual size_t write(const void *in_ptr, size_t in_size) {
            TRACED();
            size_t result = 0;
            if (active) {
                if (isFirst){
                    // we expect at least the full header
                    int written = header.write((uint8_t*)in_ptr, in_size);
                    if (!header.isDataComplete()){
                        return in_size;
                    }
                    // parse header
                    header.begin();

                    size_t len = in_size - written;
                    uint8_t *sound_ptr = (uint8_t *) in_ptr + written;
                    isFirst = false;
                    isValid = header.audioInfo().is_valid;

                    LOGI("WAV sample_rate: %d", header.audioInfo().sample_rate);
                    LOGI("WAV data_length: %u", (unsigned) header.audioInfo().data_length);
                    LOGI("WAV is_streamed: %d", header.audioInfo().is_streamed);
                    LOGI("WAV is_valid: %s", header.audioInfo().is_valid ? "true" :  "false");
                    
                    // check format
                    int format = header.audioInfo().format;
                    isValid = format == WAV_FORMAT_PCM;
                    if (format != WAV_FORMAT_PCM){
                        LOGE("WAV format not supported: %d", format);
                        isValid = false;
                    } else {
                        // update sampling rate if the target supports it
                        AudioBaseInfo bi;
                        bi.sample_rate = header.audioInfo().sample_rate;
                        bi.channels = header.audioInfo().channels;
                        bi.bits_per_sample = header.audioInfo().bits_per_sample;
                        // we provide some functionality so that we could check if the destination supports the requested format
                        if (audioBaseInfoSupport!=nullptr){
                            isValid = audioBaseInfoSupport->validate(bi);
                            if (isValid){
                                LOGI("isValid: %s", isValid ? "true":"false");
                                audioBaseInfoSupport->setAudioInfo(bi);
                                // write prm data from first record
                                LOGI("WAVDecoder writing first sound data");
                                result = out->write(sound_ptr, len);
                            } else {
                                LOGE("isValid: %s", isValid ? "true":"false");
                            }
                        }
                    }
                    
                } else if (isValid)  {
                    result = out->write((uint8_t*)in_ptr, in_size);
                }
            }
            header.end();
            return result;
        }

        /// Alternative API which provides the data from an input stream
        int readStream(Stream &in){
            TRACED();
            uint8_t buffer[READ_BUFFER_SIZE];
            int len = in.readBytes(buffer, READ_BUFFER_SIZE);
            return write(buffer, len);
        }

        virtual operator bool() {
            return active;
        }

    protected:
        WAVHeader header;
        Print *out;
        AudioBaseInfoDependent *audioBaseInfoSupport;
        bool isFirst = true;
        bool isValid = true;
        bool active;

};

/**
 * @brief A simple WAV file encoder. 
 * @ingroup codec-wav
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class WAVEncoder : public AudioEncoder {
    public: 
        // Empty Constructor - the output stream must be provided with begin()
        WAVEncoder(){
            audioInfo = defaultConfig();
        }        

        // Constructor providing the output stream
        WAVEncoder(Print &out){
            stream_ptr = &out;
            audioInfo = defaultConfig();
        }

        // Constructor providing the output stream and the WAVAudioInfo
        WAVEncoder(Print &out, WAVAudioInfo ai){
            stream_ptr = &out;
            setAudioInfo(ai);
        }

        /// Defines the otuput stream
        void setOutputStream(Print &out){
            stream_ptr = &out;
        }

        /// Provides "audio/wav"
        const char* mime(){
            return wav_mime;
        }

        // Provides the default configuration
        WAVAudioInfo defaultConfig() {
            WAVAudioInfo info;
            info.format = WAV_FORMAT_PCM;
            info.sample_rate = DEFAULT_SAMPLE_RATE;
            info.bits_per_sample = DEFAULT_BITS_PER_SAMPLE;
            info.channels = DEFAULT_CHANNELS;
            info.is_streamed = false;
            info.is_valid = true;
            info.data_length = 0x7fff0000;
            info.file_size = info.data_length + 36;
            return info;
        }

        /// Update actual WAVAudioInfo 
        virtual void setAudioInfo(AudioBaseInfo from) {
            audioInfo.sample_rate = from.sample_rate;    
            audioInfo.channels = from.channels;       
            audioInfo.bits_per_sample=from.bits_per_sample; 
        }

        /// Defines the WAVAudioInfo
        virtual void setAudioInfo(WAVAudioInfo ai) {
            audioInfo = ai;
            LOGI("sample_rate: %d", audioInfo.sample_rate);
            LOGI("channels: %d", audioInfo.channels);
            audioInfo.byte_rate = audioInfo.sample_rate * audioInfo.bits_per_sample * audioInfo.channels;
            audioInfo.block_align =  audioInfo.bits_per_sample / 8 * audioInfo.channels;
            if (audioInfo.is_streamed || audioInfo.data_length==0 || audioInfo.data_length >= 0x7fff0000) {
                LOGI("is_streamed! because length is %u",(unsigned) audioInfo.data_length);
                audioInfo.is_streamed = true;
                audioInfo.data_length = ~0;
            } else {
                size_limit = audioInfo.data_length;
                LOGI("size_limit is %d", (int) size_limit);
            }

        };

        /// starts the processing using the actual WAVAudioInfo
        virtual void begin() {
            begin(audioInfo);
        }

        /// starts the processing
        void begin(WAVAudioInfo &ai) {
            header_written = false;
            setAudioInfo(ai);
            is_open = true;
        }

        /// starts the processing
        void begin(Stream &out, WAVAudioInfo &ai) {
            stream_ptr = &out;
            begin(ai);
        }

        /// stops the processing
        void end() {
            is_open = false;
        }

        /// Writes PCM data to be encoded as WAV
        virtual size_t write(const void *in_ptr, size_t in_size){
            if (!is_open){
                LOGE("The WAVEncoder is not open - please call begin()");
                return 0;
            }
            if (stream_ptr==nullptr){
                LOGE("No output stream was provided");
                return 0;
            }
            if (!header_written){
                LOGI("Writing Header");
                writeHeader(stream_ptr);
                header_written = true;
            }

            int32_t result = 0;
            if (audioInfo.is_streamed){
                result = stream_ptr->write((uint8_t*)in_ptr, in_size);
            } else if (size_limit>0){
                size_t write_size = min((size_t)in_size,(size_t)size_limit);
                result = stream_ptr->write((uint8_t*)in_ptr, write_size);
                size_limit -= result;

                if (size_limit<=0){
                    LOGI("The defined size was written - so we close the WAVEncoder now");
                   // stream_ptr->flush();
                    is_open = false;
                }
            }  
            return result;
        }

        operator bool() {
            return is_open;
        }

        bool isOpen(){
            return is_open;
        }

        /// Adds adds n empty bytes at the beginning of the data
        void setDataOffset(uint16_t offset){
            this->offset = offset;
        }

        void writeHeader(Print *out) {
            writeRiffHeader(out);
            writeFMT(out);
            writeDataHeader(out);
        }

    protected:
        Print* stream_ptr;
        WAVAudioInfo audioInfo = defaultConfig();
        int64_t size_limit;
        bool header_written = false;
        volatile bool is_open;
        uint32_t offset=0; //adds n empty bytes at the beginning of the data


        void writeRiffHeader(Print *stream_ptr){
            stream_ptr->write("RIFF",4);
            write32(*stream_ptr, audioInfo.file_size-8);
            stream_ptr->write("WAVE",4);
        }

        void writeFMT(Print *stream_ptr){
            uint16_t fmt_len = 16;
            uint32_t byteRate = audioInfo.sample_rate * audioInfo.bits_per_sample * audioInfo.channels / 8;
            uint32_t frame_size = audioInfo.channels * audioInfo.bits_per_sample / 8;
            stream_ptr->write("fmt ",4);
            write32(*stream_ptr, fmt_len);
            write16(*stream_ptr, audioInfo.format); //PCM
            write16(*stream_ptr, audioInfo.channels); 
            write32(*stream_ptr, audioInfo.sample_rate); 
            write32(*stream_ptr, byteRate); 
            write16(*stream_ptr, frame_size);  //frame size
            write16(*stream_ptr, audioInfo.bits_per_sample);             
        }

        void write32(Print &stream, uint64_t value ){
            stream.write((uint8_t *) &value, 4);
        }
        
        void write16(Print &stream, uint16_t value ){
            stream.write((uint8_t *) &value, 2);
        }

        void writeDataHeader(Print *stream_ptr) {
            stream_ptr->write("data",4);
            audioInfo.file_size -=44;
            write32(*stream_ptr, audioInfo.file_size);
            if (offset>0) {
                char empty[offset];
                memset(empty,0, offset);
                stream_ptr->write(empty,offset);  // resolve issue with wrong aligment
            }
        }

};

}
