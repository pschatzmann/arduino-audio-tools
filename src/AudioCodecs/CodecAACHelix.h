#pragma once

//#include "Stream.h"
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

        AACDecoderHelix(bool raw=false) {
            TRACED();
            aac = new libhelix::AACDecoderHelix();
            setRaw(raw);
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

        void setRaw(bool flag){
            if (aac!=nullptr) aac->setRaw(flag);
        }

        /// Defines the output Stream
        virtual void setOutput(Print &out_stream){
            TRACED();
            AudioDecoder::setOutput(out_stream);
            if (aac!=nullptr) aac->setOutput(out_stream);
        }

        /// Starts the processing
        void begin() override {
            TRACED();
            if (aac!=nullptr) {
                aac->setDelay(CODEC_DELAY_MS);
                aac->setInfoCallback(infoCallback, this);
                aac->begin();
            }
        }

        /// Releases the reserved memory
        virtual void end() override {
            TRACED();
            if (aac!=nullptr) aac->end();
        }

        virtual _AACFrameInfo audioInfoEx(){
            return aac->audioInfo();
        }

        virtual AudioInfo audioInfo() override{
            AudioInfo result;
            auto i = audioInfoEx();
            result.channels = i.nChans;
            result.sample_rate = i.sampRateOut;
            result.bits_per_sample = i.bitsPerSample;
            return result;
        }

        void setAudioInfo(AudioInfo info) override {
            this->info = info;
            if(p_notify!=nullptr && info_notifications_active){
                p_notify->setAudioInfo(info);   
            }
        }

        /// Write AAC data to decoder
        size_t write(const void* aac_data, size_t len) override {
            LOGD("AACDecoderHelix::write: %d", (int)len);
            if (aac==nullptr) return 0;
            int open = len;
            int processed = 0;
            uint8_t *data = (uint8_t*)aac_data;
            while(open>0){
                 int act_write = aac->write(data+processed, min(open, DEFAULT_BUFFER_SIZE));
                 open -= act_write;
                 processed += act_write;
            }
            return processed;
        }

        /// checks if the class is active 
        virtual operator bool() override {
            return aac!=nullptr && (bool)*aac;
        }

        void flush(){
        //     aac->flush();
        }

        // /// Defines the callback object to which the Audio information change is provided
        // virtual void setNotifyAudioChange(AudioInfoSupport &bi) override {
        //     TRACED();
        //     audioChangeAACHelix = &bi;
        //     if (aac!=nullptr) aac->setInfoCallback(infoCallback, this);
        // }

        /// notifies the subscriber about a change
        static void infoCallback(_AACFrameInfo &i, void* ref){
            AACDecoderHelix *p_helix =  (AACDecoderHelix *)ref;
            if (p_helix!=nullptr){
                TRACED();
                AudioInfo baseInfo;
                baseInfo.channels = i.nChans;
                baseInfo.sample_rate = i.sampRateOut;
                baseInfo.bits_per_sample = i.bitsPerSample;
                //p_helix->audioChangeAACHelix->setAudioInfo(baseInfo);   
                LOGW("sample_rate: %d", i.sampRateOut);
                p_helix->setAudioInfo(baseInfo);
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

        void setAudioInfoNotifications(bool active){
            info_notifications_active = active;
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
        bool info_notifications_active = true;


};


} // namespace

