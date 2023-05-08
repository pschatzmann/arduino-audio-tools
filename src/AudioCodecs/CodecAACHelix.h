#pragma once

#include "Stream.h"
#include "AudioCodecs/AudioEncoded.h"
#include "AACDecoderHelix.h"

namespace audio_tools {

/**
 * @brief AAC Decoder using libhelix: https://github.com/pschatzmann/arduino-libhelix
 * This is basically just a simple wrapper to provide AudioInfo and AudioInfoSupport
 * @ingroup codecs
 * @ingroup decoder
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
        AACDecoderHelix(Print &out_stream, AudioInfoSupport &bi){
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
        virtual void setOutput(Print &out_stream){
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

        virtual AudioInfo audioInfo(){
            AudioInfo result;
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
        virtual void setNotifyAudioChange(AudioInfoSupport &bi){
            TRACED();
            audioChangeAACHelix = &bi;
            if (aac!=nullptr) aac->setInfoCallback(infoCallback, this);
        }

        /// notifies the subscriber about a change
        static void infoCallback(_AACFrameInfo &i, void* ref){
            AACDecoderHelix *p_helix =  (AACDecoderHelix *)ref;
            if (p_helix!=nullptr && p_helix->audioChangeAACHelix!=nullptr){
                TRACED();
                AudioInfo baseInfo;
                baseInfo.channels = i.nChans;
                baseInfo.sample_rate = i.sampRateOut;
                baseInfo.bits_per_sample = i.bitsPerSample;
                p_helix->audioChangeAACHelix->setAudioInfo(baseInfo);   
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

#ifdef HELIX_PCM_CORRECTED
        /// Provides the maximum pwm buffer size - this is allocated on the heap and you can reduce the heap size my minimizing this value
        size_t maxPCMSize() {
            return aac->maxPCMSize();
        }

        /// Define your optimized maximum pwm buffer size
        void setMaxPCMSize(size_t len) {
            aac->setMaxPCMSize(len);
        }
#else
        /// Provides the maximum pwm buffer size - this is allocated on the heap and you can reduce the heap size my minimizing this value
        size_t maxPCMSize() {
            return aac->maxPWMSize();
        }

        /// Define your optimized maximum pwm buffer size
        void setMaxPCMSize(size_t len) {
            aac->setMaxPWMSize(len);
        }
#endif

    protected:
        libhelix::AACDecoderHelix *aac=nullptr;
        // audio change notification target
        AudioInfoSupport *audioChangeAACHelix=nullptr;


};


} // namespace

