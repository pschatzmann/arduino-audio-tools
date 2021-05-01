#pragma once

#include "Arduino.h"
#include "AudioTools.h"

#define WAV_FORMAT_PCM 0x0001
#define TAG(a, b, c, d) (((a) << 24) | ((b) << 16) | ((c) << 8) | (d))

namespace audio_tools {


/**
 * @brief Sound information which is available in the WAV header
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
struct WAVAudioInfo {
    int format;
    int sample_rate;
    int bits_per_sample;
    int channels;
    int byte_rate;
    int block_align;
    bool is_streamed;
    bool is_valid;
    uint32_t data_length;
    uint32_t file_size;
};


/**
 * @brief Parser for Wav header data
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
class WAVHeader  {
    public:
        WAVHeader(){
        };

        void begin(uint8_t* buffer, size_t len){
            Logger.printf(AudioLogger::Info,"WAVHeader len: ", len);

            this->buffer = buffer;
            this->len = len;
            this->data_pos = 0l;
            
            memset(&headerInfo, 0, sizeof(WAVAudioInfo));
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
            return;
        }


        // provides the AudioInfo
        WAVAudioInfo &audioInfo() {
            return headerInfo;
        }

        // provides access to the sound data for the first record
        bool soundData(uint8_t* &data, size_t &len){
            if (sound_pos > 0){
                data = buffer + sound_pos;
                len = max((long) (this->len - sound_pos),0l);
                sound_pos = 0;
                return true;
            }
            return false;
        }

    protected:
        struct WAVAudioInfo headerInfo;
        uint8_t* buffer;
        size_t len;
        size_t data_pos = 0;
        size_t sound_pos = 0;

        void logInfo(){
            Logger.printf(AudioLogger::Info,"WAVHeader sound_pos: ", sound_pos);
            Logger.printf(AudioLogger::Info,"WAVHeader channels: ", headerInfo.channels);
            Logger.printf(AudioLogger::Info,"WAVHeader bits_per_sample: ", headerInfo.bits_per_sample);
            Logger.printf(AudioLogger::Info,"WAVHeader sample_rate: ", headerInfo.sample_rate);
            Logger.printf(AudioLogger::Info,"WAVHeader format: ", headerInfo.format);
        }

        uint32_t read_tag() {
            uint32_t tag = 0;
            tag = (tag << 8) | getChar();
            tag = (tag << 8) | getChar();
            tag = (tag << 8) | getChar();
            tag = (tag << 8) | getChar();
            return tag;
        }

        uint32_t read_int32() {
            uint32_t value = 0;
            value |= getChar() <<  0;
            value |= getChar() <<  8;
            value |= getChar() << 16;
            value |= getChar() << 24;
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
};

AudioBaseInfoDependent AudioBaseInfoDependentNone;

/**
 * @brief WAVDecoder - We parse the header data on the first record
 * and send the sound data to the stream which was indicated in the
 * constructor.
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class WAVDecoder {
    public:
        WAVDecoder(Print &out_stream, AudioBaseInfoDependent &bi = AudioBaseInfoDependentNone){
            this->out = &out_stream;
            this->audioBaseInfoSupport = &bi;
        }
    
        void begin() {
            isFirst = true;
        }

        WAVAudioInfo &audioInfo() {
            return header.audioInfo();
        }

	    int write(void *in_ptr, size_t in_size){
            if (!isValid) return -1;
            if (isFirst){
                header.begin((uint8_t*)in_ptr, in_size);
                uint8_t *sound_ptr;
                size_t len;
                if (header.soundData(sound_ptr, len)){
                    isFirst = false;
                    isValid = header.audioInfo().is_valid;

                    Logger.printf(AudioLogger::Info,"WAV sample_rate: ", header.audioInfo().sample_rate);
                    Logger.printf(AudioLogger::Info,"WAV data_length: ", header.audioInfo().data_length);
                    Logger.printf(AudioLogger::Info,"WAV is_streamed: ", header.audioInfo().is_streamed);
                    Logger.printf(AudioLogger::Info,"WAVis_valid: ", header.audioInfo().is_valid);
                    
                    // check format
                    int format = header.audioInfo().format;
                    isValid = format == WAV_FORMAT_PCM;
                    if (format != WAV_FORMAT_PCM){
                        Logger.printf(AudioLogger::Error,"WAV format not supported: ", format);
                        isValid = false;
                    } else {
                        // update sampling rate if the target supports it
                        if (audioBaseInfoSupport!=nullptr){
                            AudioBaseInfo bi;
                            bi.sample_rate = header.audioInfo().sample_rate;
                            bi.channels = header.audioInfo().channels;
                            bi.bits_per_sample = header.audioInfo().bits_per_sample;
                            audioBaseInfoSupport->setAudioBaseInfo(bi);
                        }

                        // write prm data from first record
                        Logger.printf(AudioLogger::Info,"WAVDecoder writing first sound data");
                        out->write(sound_ptr, len);
                    }
                }
            } else {
                out->write((uint8_t*)in_ptr, in_size);
            }
        }

    protected:
        WAVHeader header;
        Print *out;
        AudioBaseInfoDependent *audioBaseInfoSupport;
        bool isFirst = true;
        bool isValid = true;

};



/**
 * @brief A simple WAV file encoder. 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class WAVEncoder {
    public: 
        WAVEncoder(Stream &out){
            stream_ptr = &out;
        }

        void setDataFormat(uint16_t format = WAV_FORMAT_PCM) {
            audioInfo.format = format;
        }

        void setFileSize(uint32_t length=0xffffffff) {
            audioInfo.file_size = length;
            this->max_samples = 0;
        }

        void setMaxSamples(uint32_t samples) {
            this->max_samples = samples;
        }

        virtual int begin(WAVAudioInfo &ai) {
            audioInfo = ai;
        }

        virtual int begin(int input_channels=2, int input_sample_rate=44100, int input_bits_per_sample=16) {
            audioInfo.channels = input_channels;
            audioInfo.sample_rate = input_sample_rate;
            audioInfo.bits_per_sample = input_bits_per_sample;
            if (max_samples>0){
                audioInfo.file_size = max_samples * audioInfo.bits_per_sample / 8 * audioInfo.channels;
            }
            writeRiffHeader();
            writeFMT();
            writeDataHeader();
            return 0;
        }
    
        virtual size_t write(void *in_ptr, size_t in_size){
            int32_t result = 0;
            if (audioInfo.file_size>0){
                size_t write_size = min((size_t)in_size,(size_t)audioInfo.file_size);
                result = stream_ptr->write((uint8_t*)in_ptr, write_size);
                audioInfo.file_size -= write_size;
            }  
            return result;
        }

    protected:
        Stream* stream_ptr;
        WAVAudioInfo audioInfo;
        uint32_t max_samples;

        void writeRiffHeader(){
            stream_ptr->write("RIFF",4);
            stream_ptr->write(audioInfo.file_size-8);
            stream_ptr->write("WAVE",4);
        }

        void writeFMT(){
            uint16_t fmt_len = 16;
            uint32_t byteRate = audioInfo.sample_rate * audioInfo.bits_per_sample * audioInfo.channels / 8;
            uint32_t frame_size = audioInfo.channels * audioInfo.bits_per_sample / 8;
            stream_ptr->write("FMT ",4);
            stream_ptr->write(fmt_len);
            stream_ptr->write(audioInfo.format); //PCM
            stream_ptr->write(audioInfo.channels); 
            stream_ptr->write(audioInfo.sample_rate); 
            stream_ptr->write(byteRate); 
            stream_ptr->write(frame_size);  //frame size
            stream_ptr->write(audioInfo.bits_per_sample);             
        }

        void writeDataHeader() {
            stream_ptr->write("DATA",4);
            audioInfo.file_size -=44;
            stream_ptr->write(audioInfo.file_size);
        }

};

}
