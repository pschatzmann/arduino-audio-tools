#pragma once

#include "Stream.h"
#include "AudioTools/AudioTypes.h"

#define HELIX_LOGGING_ACTIVE false
#include "MP3DecoderHelix.h"

namespace audio_tools {

/**
 * @brief Audio Information for MP3
 * 
 */
struct MP3HelixAudioInfo : public AudioBaseInfo {
    MP3HelixAudioInfo() = default;
    MP3HelixAudioInfo(const MP3HelixAudioInfo& alt) = default;
    MP3HelixAudioInfo(const MP3FrameInfo& alt) {
        sample_rate = alt.samprate;
        channels = alt.nChans;
        bits_per_sample = alt.bitsPerSample;   
    }         
}; 

// forware declaration
class MP3DecoderHelix;
void dataCallback_MP3DecoderHelix(MP3FrameInfo &info, int16_t *pwm_buffer, size_t len);
MP3DecoderHelix *self_MP3DecoderHelix;


/**
 * @brief MP3 Decoder using libhelix: https://github.com/pschatzmann/arduino-libhelix
 * 
 */
class MP3DecoderHelix : public AudioDecoder  {
    public:

        MP3DecoderHelix() {
        	LOGD(__FUNCTION__);
        }
        /**
         * @brief Construct a new MP3DecoderMini object
         * 
         * @param out_stream 
         */
        MP3DecoderHelix(Print &out_stream){
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
        MP3DecoderHelix(Print &out_stream, AudioBaseInfoDependent &bi){
        	LOGD(__FUNCTION__);
            this->out = &out_stream;
            this->audioBaseInfoSupport = &bi;
        }  

        /**
         * @brief Destroy the MP3DecoderMini object
         * 
         */
        ~MP3DecoderHelix(){
            if (active){
                end();
            }
        }

        /// Defines the callback object to which the Audio information change is provided
        virtual void setNotifyAudioChange(AudioBaseInfoDependent &bi){
            this->audioBaseInfoSupport = &bi;
        }

        /// Defines the output Stream
		virtual void setStream(Stream &out_stream){
            this->out = &out_stream;
		}

        /// Starts the processing
        void begin(){
        	LOGD(__FUNCTION__);
            mp3.setDataCallback(dataCallback_MP3DecoderHelix);
            mp3.begin();
            self_MP3DecoderHelix = this;
            active = true;
        }

        /// Releases the reserved memory
        virtual void end(){
        	LOGD(__FUNCTION__);
            flush();
            mp3.end();
            self_MP3DecoderHelix = nullptr;
            active = false;
        }

        virtual AudioBaseInfo audioInfo(){
            return mp3HelixAudioInfo;
        }

        /// Write mp3 data to decoder
        size_t write(const void* fileData, size_t len) {
            return mp3.write((uint8_t*)fileData, len);
        }

        /// checks if the class is active 
        virtual operator boolean(){
            return active;
        }

        void flush(){
            out->flush();
        }


    friend void dataCallback_MP3DecoderHelix(MP3FrameInfo &info, int16_t *pwm_buffer, size_t len);

    protected:
        Print *out = nullptr;
        AudioBaseInfoDependent *audioBaseInfoSupport = nullptr;
        MP3HelixAudioInfo mp3HelixAudioInfo;
        libhelix::MP3DecoderHelix mp3;
        bool active;

        void notifyInfoChange(MP3HelixAudioInfo info) {
            if (info != mp3HelixAudioInfo ){
                audioBaseInfoSupport->setAudioInfo(info);
                mp3HelixAudioInfo = info;
            }
        }

        void writePCM(uint8_t* data, size_t len){
            out->write(data, len);
        }
};

/**
 * @brief Data Callback for libhelix
 * 
 * @param info 
 * @param pwm_buffer 
 * @param len 
 */
void dataCallback_MP3DecoderHelix(MP3FrameInfo &info, int16_t *pwm_buffer, size_t len) {
    if (self_MP3DecoderHelix!=nullptr){
        MP3HelixAudioInfo audio_info(info);
        if (len>0){
            self_MP3DecoderHelix->notifyInfoChange(info);
            self_MP3DecoderHelix->writePCM((uint8_t*)pwm_buffer, len*2);
        }
    }
}


} // namespace

