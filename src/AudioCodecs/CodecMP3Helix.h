#pragma once

#include "Stream.h"
#include "AudioTools/AudioTypes.h"

#define HELIX_LOGGING_ACTIVE false
//#define HELIX_LOG_LEVEL Info
#include "MP3DecoderHelix.h"

namespace audio_tools {

// audio change notification target
AudioBaseInfoDependent *audioChangeMP3Helix=nullptr;

/**
 * @brief MP3 Decoder using libhelix: https://github.com/pschatzmann/arduino-libhelix
 * This is basically just a simple wrapper to provide AudioBaseInfo and AudioBaseInfoDependent
 */
class MP3DecoderHelix : public AudioDecoder  {
    public:

        MP3DecoderHelix() {
        	LOGD(__FUNCTION__);
            mp3 = new libhelix::MP3DecoderHelix();
        }
        /**
         * @brief Construct a new MP3DecoderMini object
         * 
         * @param out_stream 
         */
        MP3DecoderHelix(Print &out_stream){
        	LOGD(__FUNCTION__);
            mp3 = new libhelix::MP3DecoderHelix(out_stream);
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
            mp3 = new libhelix::MP3DecoderHelix(out_stream);
            setNotifyAudioChange(bi);
        }  

        /**
         * @brief Destroy the MP3DecoderMini object
         * 
         */
        ~MP3DecoderHelix(){
            delete mp3;
        }

        /// Defines the output Stream
		virtual void setOutputStream(Stream &outStream){
            mp3->setStream(outStream);
		}

        /// Starts the processing
        void begin(){
        	LOGD(__FUNCTION__);
            mp3->begin();
        }

        /// Releases the reserved memory
        virtual void end(){
        	LOGD(__FUNCTION__);
            mp3->end();
        }

        virtual libhelix::MP3FrameInfo audioInfoEx(){
            return mp3->audioInfo();
        }

        virtual AudioBaseInfo audioInfo(){
            libhelix::MP3FrameInfo i = audioInfoEx();
            AudioBaseInfo baseInfo;
            baseInfo.channels = i.channels;
            baseInfo.sample_rate = i.sample_rate;
            baseInfo.bits_per_sample = i.bits_per_sample;
            return baseInfo;
        }

        /// Write mp3 data to decoder
        size_t write(const void* mp3Data, size_t len) {
            mp3->write(mp3Data, len);
        }

        /// checks if the class is active 
        virtual operator boolean(){
            return (bool) *mp3;
        }

        libhelix::MP3DecoderHelix driver() {
            return mp3;
        }

        void flush(){
            mp3->flush();
        }

        /// Defines the callback object to which the Audio information change is provided
        virtual void setNotifyAudioChange(AudioBaseInfoDependent &bi){
        	LOGD(__FUNCTION__);
            audioChangeMP3Helix = &bi;
            setNotifyAudioChange(infoCallback);
        }

        /// notifies the subscriber about a change
        static void infoCallback(libhelix::MP3FrameInfo &i){
            if (audioChangeAACHelix!=nullptr){
            	LOGD(__FUNCTION__);
                AudioBaseInfo baseInfo;
                baseInfo.channels = i.channels;
                baseInfo.sample_rate = i.sample_rate;
                baseInfo.bits_per_sample = i.bits_per_sample;
                audioChangeAACHelix->setAudioInfo(baseInfo);   
            }
        }


    protected:
        libhelix::MP3DecoderHelix *mp3=nullptr;

};


} // namespace

