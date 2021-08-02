#pragma once

#define MINIMP3_IMPLEMENTATION
#define MINIMP3_NO_STDIO
//#define MINIMP3_NO_SIMD
#define LOGGING_ACTIVE true

#include "Stream.h"
#include "AudioTools/AudioTypes.h"
#include "AudioCodecs/ext/minimp3/minimp3.h"

namespace audio_tools {

/**
 * @brief Audio Info provided by the decoder 
 */
typedef AudioBaseInfo MP3MiniAudioInfo;
typedef void (*MP3InfoCallback)(MP3MiniAudioInfo &info);
typedef void (*MP3DataCallback)(MP3MiniAudioInfo &info,int16_t *pwm_buffer, size_t len);

/**
 * @brief MP3 Decoder using https://github.com/lieff/minimp3
 * 
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

        /// Defines the callback which provides the Audio information
        void setMP3InfoCallback(MP3InfoCallback cb){
            this->infoCallback = cb;
        }    

        /// Defines the callback which provides the Audio data
        void setMP3DataCallback(MP3DataCallback cb){
            this->pwmCallback = cb;
        }      

        /// Defines the output Stream
		void setStream(Stream &out_stream){
            this->out = &out_stream;
		}

        /// Starts the processing
        void begin(){
            begin(16*1024);
        }        

        /// Starts the processing
        void begin(int bufferLen){
        	LOGD(__FUNCTION__);
            flush();
            mp3dec_init(&mp3d);
            buffer_len = bufferLen;
            active = true;
        }

        /// Releases the reserved memory
        void end(){
        	LOGD(__FUNCTION__);
            flush();
            active = false;
            // release buffer
            delete[] buffer;
            buffer = nullptr;
        }


        /// Provides the last available MP3FrameInfo
        MP3MiniAudioInfo audioInfo(){
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
        mp3dec_t mp3d;
        mp3dec_frame_info_t mp3dec_info;
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
                int samples = mp3dec_decode_frame(&mp3d, fileData+buffer_pos, remaining_bytes, pcm, &mp3dec_info);
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
            MP3MiniAudioInfo info;
            info.sample_rate = mp3dec_info.hz;
            info.channels = mp3dec_info.channels;
            info.bits_per_sample = 16;
            provideResultCallback(info, samples);
            provideResultStream(info, samples);
            // store last audio_info so that we can detect any changes
            audio_info = info;
        }


        // return the result PWM data
        void provideResultCallback(MP3MiniAudioInfo &info, int samples){
        	LOGD(__FUNCTION__);
            // audio has changed
            if (infoCallback != nullptr && audio_info != info){
                infoCallback(info);
            }

            // provide result pwm data
            if(pwmCallback!=nullptr){
                // output via callback
                pwmCallback(info, (int16_t*) pcm, samples);
            } 
        }

        // return the result PWM data
        void provideResultStream(MP3MiniAudioInfo &info, int samples){
        	LOGD(__FUNCTION__);
            // audio has changed
            if (audioBaseInfoSupport!=nullptr){
                is_output_valid = audioBaseInfoSupport->validate(info);
                if (is_output_valid){
                    audioBaseInfoSupport->setAudioInfo(info); 
                }
            } else {
                is_output_valid = true;
            }

            // provide result pwm data
            if(out!=nullptr && is_output_valid){
                // output via callback
                out->write((uint8_t*)pcm, samples);
            } 
        }
};

} // namespace

