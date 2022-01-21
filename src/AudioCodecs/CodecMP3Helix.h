#pragma once

#include "Stream.h"
#include "AudioTools/AudioTypes.h"
#include "MP3DecoderHelix.h"

namespace audio_tools {

// audio change notification target
AudioBaseInfoDependent *audioChangeMP3Helix=nullptr;

/**
 * @brief MP3 Decoder using libhelix: https://github.com/pschatzmann/arduino-libhelix
 * This is basically just a simple wrapper to provide AudioBaseInfo and AudioBaseInfoDependent
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class MP3DecoderHelix : public AudioDecoder  {
    public:

        MP3DecoderHelix() {
            LOGD(LOG_METHOD);
            mp3 = new libhelix::MP3DecoderHelix();
            if (mp3==nullptr){
                LOGE("Not enough memory for libhelix");
            }
        }
        /**
         * @brief Construct a new MP3DecoderMini object
         * 
         * @param out_stream 
         */
        MP3DecoderHelix(Print &out_stream){
            LOGD(LOG_METHOD);
            mp3 = new libhelix::MP3DecoderHelix();
            if (mp3==nullptr){
                LOGE("Not enough memory for libhelix");
            }
            setOutputStream(out_stream);
        }  

        /**
         * @brief Construct a new MP3DecoderMini object. The decoded output will go to the 
         * print object.
         * 
         * @param out_stream 
         * @param bi 
         */
        MP3DecoderHelix(Print &out_stream, AudioBaseInfoDependent &bi){
            LOGD(LOG_METHOD);
            mp3 = new libhelix::MP3DecoderHelix();
            if (mp3==nullptr){
                LOGE("Not enough memory for libhelix");
            }
            setOutputStream(out_stream);
            setNotifyAudioChange(bi);
        }  

        /**
         * @brief Destroy the MP3DecoderMini object
         * 
         */
        ~MP3DecoderHelix(){
            if (mp3!=nullptr) delete mp3;
        }

        /// Defines the output Stream
        virtual void setOutputStream(Print &outStream){
            if (mp3!=nullptr) mp3->setOutput(outStream);
        }

        /// Starts the processing
        void begin(){
            LOGD(LOG_METHOD);
            if (mp3!=nullptr) {
                mp3->setDelay(CODEC_DELAY_MS);   
                mp3->begin();
            } 
        }

        /// Releases the reserved memory
        void end(){
            LOGD(LOG_METHOD);
            if (mp3!=nullptr) mp3->end();
        }

        MP3FrameInfo audioInfoEx(){
            return mp3->audioInfo();
        }

        AudioBaseInfo audioInfo(){
            MP3FrameInfo i = audioInfoEx();
            AudioBaseInfo baseInfo;
            baseInfo.channels = i.nChans;
            baseInfo.sample_rate = i.samprate;
            baseInfo.bits_per_sample = i.bitsPerSample;
            return baseInfo;
        }

        /// Write mp3 data to decoder
        size_t write(const void* mp3Data, size_t len) {
            LOGD("%s: %zu", LOG_METHOD, len);
            return mp3==nullptr ? 0 : mp3->write(mp3Data, len);
        }

        /// checks if the class is active 
        operator boolean(){
            return mp3!=nullptr && (bool) *mp3;
        }

        libhelix::MP3DecoderHelix *driver() {
            return mp3;
        }

        // void flush(){
        //     mp3->flush();
        // }

        /// Defines the callback object to which the Audio information change is provided
        void setNotifyAudioChange(AudioBaseInfoDependent &bi){
            LOGD(LOG_METHOD);
            audioChangeMP3Helix = &bi;
            if (mp3!=nullptr)  mp3->setInfoCallback(infoCallback);
        }

        /// notifies the subscriber about a change
        static void infoCallback(MP3FrameInfo &i){
            if (audioChangeMP3Helix!=nullptr){
                LOGD(LOG_METHOD);
                AudioBaseInfo baseInfo;
                baseInfo.channels = i.nChans;
                baseInfo.sample_rate = i.samprate;
                baseInfo.bits_per_sample = i.bitsPerSample;
                audioChangeMP3Helix->setAudioInfo(baseInfo);   
            }
        }


    protected:
        libhelix::MP3DecoderHelix *mp3=nullptr;

};


} // namespace


