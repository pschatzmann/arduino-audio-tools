#pragma once

#include "Stream.h"
#include "AudioCodecs/AudioEncoded.h"
#include "AACDecoderHelix.h"

/** 
 * @defgroup helix Helix
 * @ingroup codecs
 * @brief Helix Decoder  
**/

namespace audio_tools {

// audio change notification target
AudioBaseInfoDependent *audioChangeAACHelix=nullptr;

/**
 * @brief AAC Decoder using libhelix: https://github.com/pschatzmann/arduino-libhelix
 * This is basically just a simple wrapper to provide AudioBaseInfo and AudioBaseInfoDependent
 * @ingroup helix
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AACDecoderHelix : public AudioDecoder  {
    public:

        AACDecoderHelix() {
            TRACED();
            aac = new libhelix::AACDecoderHelix();
            if (aac==nullptr){
                LOGE("Not enough memory for libhelix");
            }
        }
        /**
         * @brief Construct a new AACDecoderMini object
         * 
         * @param out_stream 
         */
        AACDecoderHelix(Print &out_stream){
            TRACED();
            aac = new libhelix::AACDecoderHelix(out_stream);
            if (aac==nullptr){
                LOGE("Not enough memory for libhelix");
            }
        }  

        /**
         * @brief Construct a new AACDecoderMini object. The decoded output will go to the 
         * print object.
         * 
         * @param out_stream 
         * @param bi 
         */
        AACDecoderHelix(Print &out_stream, AudioBaseInfoDependent &bi){
            TRACED();
            aac = new libhelix::AACDecoderHelix(out_stream);
            if (aac==nullptr){
                LOGE("Not enough memory for libhelix");
            }
            setNotifyAudioChange(bi);
        }  

        /**
         * @brief Destroy the AACDecoderMini object
         * 
         */
        ~AACDecoderHelix(){
             TRACED();
            if (aac!=nullptr) delete aac;
        }

        /// Defines the output Stream
        virtual void setOutputStream(Print &out_stream){
            TRACED();
            if (aac!=nullptr) aac->setOutput(out_stream);
        }

        /// Starts the processing
        void begin(){
            TRACED();
            if (aac!=nullptr) {
                aac->setDelay(CODEC_DELAY_MS);
                aac->begin();
            }
        }

        /// Releases the reserved memory
        virtual void end(){
            TRACED();
            if (aac!=nullptr) aac->end();
        }

        virtual _AACFrameInfo audioInfoEx(){
            return aac->audioInfo();
        }

        virtual AudioBaseInfo audioInfo(){
            AudioBaseInfo result;
            auto i = audioInfoEx();
            result.channels = i.nChans;
            result.sample_rate = i.sampRateOut;
            result.bits_per_sample = i.bitsPerSample;
            return result;
        }

        /// Write AAC data to decoder
        size_t write(const void* aac_data, size_t len) {
            return aac==nullptr ? 0 : aac->write(aac_data, len);
        }

        /// checks if the class is active 
        virtual operator bool(){
            return aac!=nullptr && (bool)*aac;
        }

        void flush(){
        //     aac->flush();
        }

        /// Defines the callback object to which the Audio information change is provided
        virtual void setNotifyAudioChange(AudioBaseInfoDependent &bi){
            TRACED();
            audioChangeAACHelix = &bi;
            if (aac!=nullptr) aac->setInfoCallback(infoCallback);
        }

        /// notifies the subscriber about a change
        static void infoCallback(_AACFrameInfo &i){
            if (audioChangeAACHelix!=nullptr){
                TRACED();
                AudioBaseInfo baseInfo;
                baseInfo.channels = i.nChans;
                baseInfo.sample_rate = i.sampRateOut;
                baseInfo.bits_per_sample = i.bitsPerSample;
                audioChangeAACHelix->setAudioInfo(baseInfo);   
            }
        }

        /// Provides the maximum frame size - this is allocated on the heap and you can reduce the heap size my minimizing this value
        size_t maxFrameSize() {
            return aac->maxFrameSize();
        }

        /// Define your optimized maximum frame size
        void setMaxFrameSize(size_t len){
            aac->setMaxFrameSize(len);
        }

        /// Provides the maximum pwm buffer size - this is allocated on the heap and you can reduce the heap size my minimizing this value
        size_t maxPWMSize() {
            return aac->maxPWMSize();
        }

        /// Define your optimized maximum pwm buffer size
        void setMaxPWMSize(size_t len) {
            aac->setMaxPWMSize(len);
        }

    protected:
        libhelix::AACDecoderHelix *aac=nullptr;

};


} // namespace

