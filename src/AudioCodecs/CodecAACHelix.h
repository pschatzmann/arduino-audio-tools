#pragma once

#include "Stream.h"
#include "AudioTools/AudioTypes.h"

#define HELIX_LOGGING_ACTIVE false
//#define HELIX_LOG_LEVEL Info
#include "AACDecoderHelix.h"

namespace audio_tools {

/**
 * @brief Audio Information for AAC
 * 
 */
struct AACHelixAudioInfo : public AudioBaseInfo {
    AACHelixAudioInfo() = default;
    AACHelixAudioInfo(const AACHelixAudioInfo& alt) = default;
    AACHelixAudioInfo(const _AACFrameInfo& alt) {
        sample_rate = alt.sampRateOut;
        channels = alt.nChans;
        bits_per_sample = alt.bitsPerSample;   
    }         
}; 

// forware declaration
class AACDecoderHelix;
void dataCallback_AACDecoderHelix(_AACFrameInfo &info, int16_t *pwm_buffer, size_t len);
AACDecoderHelix *self_AACDecoderHelix;


/**
 * @brief AAC Decoder using libhelix: https://github.com/pschatzmann/arduino-libhelix
 * 
 */
class AACDecoderHelix : public AudioDecoder  {
    public:

        AACDecoderHelix() {
        	LOGD(__FUNCTION__);
        }
        /**
         * @brief Construct a new AACDecoderMini object
         * 
         * @param out_stream 
         */
        AACDecoderHelix(Print &out_stream){
        	LOGD(__FUNCTION__);
            this->out = &out_stream;
        }  

        /**
         * @brief Construct a new AACDecoderMini object. The decoded output will go to the 
         * print object.
         * 
         * @param out_stream 
         * @param bi 
         */
        AACDecoderHelix(Print &out_stream, AudioBaseInfoDependent &bi){
        	LOGD(__FUNCTION__);
            this->out = &out_stream;
            this->audioBaseInfoSupport = &bi;
        }  

        /**
         * @brief Destroy the AACDecoderMini object
         * 
         */
        ~AACDecoderHelix(){
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
            AAC.setDataCallback(dataCallback_AACDecoderHelix);
            AAC.begin();
            self_AACDecoderHelix = this;
            active = true;
        }

        /// Releases the reserved memory
        virtual void end(){
        	LOGD(__FUNCTION__);
            flush();
            AAC.end();
            self_AACDecoderHelix = nullptr;
            active = false;
        }

        virtual AudioBaseInfo audioInfo(){
            return AACHelixAudioInfo;
        }

        /// Write AAC data to decoder
        size_t write(const void* fileData, size_t len) {
            return AAC.write((uint8_t*)fileData, len);
        }

        /// checks if the class is active 
        virtual operator boolean(){
            return active;
        }

        void flush(){
            out->flush();
        }


    friend void dataCallback_AACDecoderHelix(_AACFrameInfo &info, int16_t *pwm_buffer, size_t len);

    protected:
        Print *out = nullptr;
        AudioBaseInfoDependent *audioBaseInfoSupport = nullptr;
        AACHelixAudioInfo AACHelixAudioInfo;
        libhelix::AACDecoderHelix AAC;
        bool active;

        void notifyInfoChange(struct AACHelixAudioInfo info) {
            if (info != AACHelixAudioInfo ){
                audioBaseInfoSupport->setAudioInfo(info);
                AACHelixAudioInfo = info;
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
void dataCallback_AACDecoderHelix(_AACFrameInfo &info, int16_t *pwm_buffer, size_t len) {
    if (self_AACDecoderHelix!=nullptr){
        AACHelixAudioInfo audio_info(info);
        if (len>0){
            self_AACDecoderHelix->notifyInfoChange(info);
            self_AACDecoderHelix->writePCM((uint8_t*)pwm_buffer, len*2);
        }
    }
}


} // namespace

