#pragma once

#define MINIMP3_IMPLEMENTATION
#define MINIMP3_NO_STDIO
#define LOGGING_ACTIVE true

#include "Stream.h"
#include "AudioTools/AudioTypes.h"
#include "AudioCodecs/ext/minimp3/minimp3.h"

namespace audio_tools {

/**
 * @brief Audio Information for MP3
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
struct MP3MiniAudioInfo : public AudioBaseInfo {
    MP3MiniAudioInfo() = default;
    MP3MiniAudioInfo(const MP3MiniAudioInfo& alt) = default;
    MP3MiniAudioInfo(const minimp3::mp3dec_frame_info_t &alt){
        sample_rate = alt.hz;
        channels = alt.channels;
        bits_per_sample = 16;
    }
};

/**
 * @brief Audio Info provided by the decoder 
 */
typedef void (*MP3InfoCallback)(MP3MiniAudioInfo &info);
typedef void (*MP3DataCallback)(MP3MiniAudioInfo &info,int16_t *pwm_buffer, size_t len);

/**
 * @brief MP3 Decoder using https://github.com/lieff/minimp3
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class MP3DecoderMini : public AudioDecoder  {
    public:

        MP3DecoderMini() {
        	LOGD(__FUNCTION__);
        }
        /**
         * @brief Construct a new MP3DecoderMini object
         * 
         * @param out_stream 
         */
        MP3DecoderMini(Print &out_stream){
        	LOGD(__FUNCTION__);
            this->out = &out_stream;
        }  

        /**
         * @brief Construct a new MP3DecoderMini object. The decoded output will go to the 
         * print object.
         * 
         * @param out_stream 
         * @param bi 
         */
        MP3DecoderMini(Print &out_stream, AudioBaseInfoDependent &bi){
        	LOGD(__FUNCTION__);
            this->out = &out_stream;
            this->audioBaseInfoSupport = &bi;
        }  

        /**
         * @brief Construct a new MP3DecoderMini object. Callbacks are used to provide
         * the resultng decoded data.
         * 
         * @param dataCallback 
         * @param infoCallback 
         */
        MP3DecoderMini(MP3DataCallback dataCallback, MP3InfoCallback infoCallback=nullptr){
            this->pwmCallback = dataCallback;
            this->infoCallback = infoCallback;
        }

        /**
         * @brief Destroy the MP3DecoderMini object
         * 
         */
        ~MP3DecoderMini(){
            if (active){
                end();
            }
        }

        /// Defines the callback object to which the Audio information change is provided
        virtual void setNotifyAudioChange(AudioBaseInfoDependent &bi){
            this->audioBaseInfoSupport = &bi;
        }

        /// Defines the callback which provides the Audio information
        void setInfoCallback(MP3InfoCallback cb){
            this->infoCallback = cb;
        }    

        /// Defines the callback which provides the Audio data
        void setDataCallback(MP3DataCallback cb){
            this->pwmCallback = cb;
        }      

        /// Defines the output Stream
		virtual void setOutputStream(Print &out_stream){
            this->out = &out_stream;
		}

        /// Starts the processing
        virtual void begin(){
            begin(16*1024);
        }        

        /// Starts the processing
        void begin(int bufferLen){
        	LOGD(__FUNCTION__);
            flush();
            minimp3::mp3dec_init(&mp3d);
            buffer_len = bufferLen;
            active = true;
        }

        /// Releases the reserved memory
        virtual void end(){
        	LOGD(__FUNCTION__);
            flush();
            active = false;
            // release buffer
            delete[] buffer;
            buffer = nullptr;
        }

        virtual AudioBaseInfo audioInfo(){
            return audio_info;
        }

        /// Write mp3 data to decoder
        size_t write(const void* fileData, size_t len) {
        	LOGD("write: %zu",len);
            size_t result = 0;
            if (active){
                if (len>buffer_len){
                    writeBuffer((uint8_t*)fileData, len);
                    result = len;
                } else if (len==0) {
                    flush();
                } else {
                    result = writePart((uint8_t*)fileData, len);
                }
            }
            return len;
        }

        /// Decodes the last outstanding data
        void flush() {
            if (buffer_pos>0 && buffer!=nullptr){
             	LOGD(__FUNCTION__);
                size_t consumed = writeBuffer(buffer, buffer_pos);
                buffer_pos -= consumed;
                // move not consumed bytes to the head
                memmove(buffer, buffer+consumed, buffer_pos);
            }
        }

        /// checks if the class is active 
        virtual operator boolean(){
            return active;
        }

    protected:
        MP3MiniAudioInfo audio_info;
        MP3DataCallback pwmCallback = nullptr;
        MP3InfoCallback infoCallback = nullptr;
        Print *out = nullptr;
        AudioBaseInfoDependent *audioBaseInfoSupport = nullptr;
        minimp3::mp3dec_t mp3d;
        minimp3::mp3dec_frame_info_t mp3dec_info;
        size_t buffer_len = 16*1024;
        size_t buffer_pos = 0;
        uint8_t *buffer=nullptr;
        short pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
        bool active;
        bool is_output_valid;


        // Splits the data up into individual parts - Returns the successfully consumed number of bytes
        int writeBuffer(uint8_t* fileData, size_t len){
        	LOGD(__FUNCTION__);
            // split into
            size_t remaining_bytes = len;
            buffer_pos = 0;
            while(true){
                LOGI("-> mp3dec_decode_frame: %zu -> %zu ", buffer_pos, remaining_bytes);
                int samples = minimp3::mp3dec_decode_frame(&mp3d, fileData+buffer_pos, remaining_bytes, pcm, &mp3dec_info);
                buffer_pos+= mp3dec_info.frame_bytes;
                remaining_bytes-=mp3dec_info.frame_bytes;
                if (samples>0){
                    provideResult(samples);
                } 
                if(remaining_bytes==0) {
                    LOGD("-> ended with remaining_bytes: %zu", remaining_bytes);
                    break;
                }
            }
            return buffer_pos;
        }

        // Writes the data in small pieces: the api recommends to combine 16 frames before calling mp3dec_decode_frame
        int writePart(uint8_t* fileData, size_t len){
        	LOGD(__FUNCTION__);
            // allocate buffer if it does not exist 
            if (buffer==nullptr){
                LOGI("Allocating buffer with %zu bytes", buffer_len);
                buffer = new uint8_t[buffer_len];
            }

            // check allocated buffer
            if (buffer==nullptr){
                LOGE("Could not allocate buffer");
                return 0;
            }

            // add data to buffer
            size_t write_len = std::min(len, buffer_len-buffer_pos);
            memmove(buffer+buffer_pos, fileData, write_len);
            buffer_pos += write_len;
            // if buffer has been filled to 90% we flush
            if (buffer_pos > buffer_len*90/100){
                // calling mp3dec_decode_frame
                flush();
            }

            // write reminder
            if (len!=write_len){
                int diff = len - write_len;
                memmove(buffer, fileData+write_len, diff);
                buffer_pos = diff;
            }
            return len;
        }

        void provideResult(int samples){
            LOGI("provideResult: %d samples", samples);
            MP3MiniAudioInfo info(mp3dec_info);
            provideAudioInfo(info);
            provideData(info, samples);
        }


        // Handles the audio_info changes
        void provideAudioInfo(MP3MiniAudioInfo &info){
            // audio has changed
            if (audio_info != info){
             	LOGI(__FUNCTION__);

                if (infoCallback != nullptr){
                    infoCallback(info);
                }

                if (audioBaseInfoSupport!=nullptr){
                    is_output_valid = audioBaseInfoSupport->validate(info);
                    if (is_output_valid){
                        audioBaseInfoSupport->setAudioInfo(info); 
                    }
                } else {
                    is_output_valid = true;
                }
                audio_info = info;
            }
        }


        // return the result PWM data
        void provideData(MP3MiniAudioInfo &info, int samples){
        	LOGD(__FUNCTION__);
            // provide result pwm data
            if(pwmCallback!=nullptr){
                // output via callback
                pwmCallback(info, (int16_t*) pcm, samples);
            } 

            // provide result pwm data
            if(out!=nullptr && is_output_valid){
                // output via callback
                int bytes = samples*2;
                int bytes_written = out->write((uint8_t*)pcm, bytes);
                if (bytes!=bytes_written){
                    LOGE("Could not write all audio data: %d of %d", bytes_written,bytes);
                }
            } 
        }

};

} // namespace

