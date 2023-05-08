#pragma once

#include "Stream.h"
#include "AudioCodecs/AudioEncoded.h"
#include "AudioMetaData/MetaDataFilter.h"
#include "MP3DecoderHelix.h"

namespace audio_tools {

/**
 * @brief MP3 Decoder using libhelix: https://github.com/pschatzmann/arduino-libhelix
 * This is basically just a simple wrapper to provide AudioInfo and AudioInfoSupport
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class MP3DecoderHelix : public AudioDecoder  {
    public:

        MP3DecoderHelix() {
            TRACED();
            mp3 = new libhelix::MP3DecoderHelix();
            filter.setDecoder(mp3);
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
            TRACED();
            mp3 = new libhelix::MP3DecoderHelix();
            filter.setDecoder(mp3);
            if (mp3==nullptr){
                LOGE("Not enough memory for libhelix");
            }
            setOutput(out_stream);
        }  

        /**
         * @brief Construct a new MP3DecoderMini object. The decoded output will go to the 
         * print object.
         * 
         * @param out_stream 
         * @param bi 
         */
        MP3DecoderHelix(Print &out_stream, AudioInfoSupport &bi){
            TRACED();
            mp3 = new libhelix::MP3DecoderHelix();
            filter.setDecoder(mp3);
            if (mp3==nullptr){
                LOGE("Not enough memory for libhelix");
            }
            setOutput(out_stream);
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
        virtual void setOutput(Print &outStream){
            if (mp3!=nullptr) mp3->setOutput(outStream);
        }

        /// Starts the processing
        void begin(){
            TRACED();
            if (mp3!=nullptr) {
                mp3->setDelay(CODEC_DELAY_MS);   
                mp3->begin();
                filter.begin();
            } 
        }

        /// Releases the reserved memory
        void end(){
            TRACED();
            if (mp3!=nullptr) mp3->end();
        }

        MP3FrameInfo audioInfoEx(){
            return mp3->audioInfo();
        }

        AudioInfo audioInfo(){
            MP3FrameInfo i = audioInfoEx();
            AudioInfo baseInfo;
            baseInfo.channels = i.nChans;
            baseInfo.sample_rate = i.samprate;
            baseInfo.bits_per_sample = i.bitsPerSample;
            return baseInfo;
        }

        /// Write mp3 data to decoder
        size_t write(const void* mp3Data, size_t len) {
            LOGD("%s: %zu", LOG_METHOD, len);
            if (mp3==nullptr) return 0;
            return use_filter ? filter.write((uint8_t*)mp3Data, len): mp3->write((uint8_t*)mp3Data, len);
        }

        /// checks if the class is active 
        operator bool(){
            return mp3!=nullptr && (bool) *mp3;
        }

        libhelix::MP3DecoderHelix *driver() {
            return mp3;
        }

        /// Defines the callback object to which the Audio information change is provided
        void setNotifyAudioChange(AudioInfoSupport &bi){
            TRACED();
            audioChangeMP3Helix = &bi;
            if (mp3!=nullptr)  mp3->setInfoCallback(infoCallback, this);
        }

        /// notifies the subscriber about a change
        static void infoCallback(MP3FrameInfo &i, void * ref){
            MP3DecoderHelix* p_helix = (MP3DecoderHelix* )ref;
            if (p_helix!=nullptr && p_helix->audioChangeMP3Helix!=nullptr){
                TRACED();
                AudioInfo baseInfo;
                baseInfo.channels = i.nChans;
                baseInfo.sample_rate = i.samprate;
                baseInfo.bits_per_sample = i.bitsPerSample;
                p_helix->audioChangeMP3Helix->setAudioInfo(baseInfo);   
            }
        }

        /// Activates a filter that makes sure that helix does not get any metadata segments
        void setFilterMetaData(bool filter){
            use_filter = filter;
        }
        
        /// Check if the metadata filter is active
        bool isFilterMetaData() {
            return use_filter;
        }

        /// Provides the maximum frame size - this is allocated on the heap and you can reduce the heap size my minimizing this value
        size_t maxFrameSize() {
            return mp3->maxFrameSize();
        }

        /// Define your optimized maximum frame size
        void setMaxFrameSize(size_t len){
            mp3->setMaxFrameSize(len);
        }

#ifdef HELIX_PCM_CORRECTED
        /// Provides the maximum pwm buffer size - this is allocated on the heap and you can reduce the heap size my minimizing this value
        size_t maxPCMSize() {
            return mp3->maxPCMSize();
        }

        /// Define your optimized maximum pwm buffer size
        void setMaxPCMSize(size_t len) {
            mp3->setMaxPCMSize(len);
        }
#else
        /// Provides the maximum pwm buffer size - this is allocated on the heap and you can reduce the heap size my minimizing this value
        size_t maxPCMSize() {
            return mp3->maxPWMSize();
        }

        /// Define your optimized maximum pwm buffer size
        void setMaxPCMSize(size_t len) {
            mp3->setMaxPWMSize(len);
        }
#endif
    protected:
        libhelix::MP3DecoderHelix *mp3=nullptr;
        MetaDataFilter<libhelix::MP3DecoderHelix> filter;
        bool use_filter = false;
        // audio change notification target
        AudioInfoSupport *audioChangeMP3Helix=nullptr;


};


} // namespace


