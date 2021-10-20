#pragma once

#if defined(USE_HELIX) || defined(USE_DECODERS)

#include "Stream.h"
#include "AudioTools/AudioTypes.h"
#include "AACDecoderHelix.h"

namespace audio_tools {

// audio change notification target
AudioBaseInfoDependent *audioChangeAACHelix=nullptr;

/**
 * @brief AAC Decoder using libhelix: https://github.com/pschatzmann/arduino-libhelix
 * This is basically just a simple wrapper to provide AudioBaseInfo and AudioBaseInfoDependent
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AACDecoderHelix : public AudioDecoder  {
    public:

        AACDecoderHelix() {
        	LOGD(LOG_METHOD);
            aac = new libhelix::AACDecoderHelix();
        }
        /**
         * @brief Construct a new AACDecoderMini object
         * 
         * @param out_stream 
         */
        AACDecoderHelix(Print &out_stream){
        	LOGD(LOG_METHOD);
            aac = new libhelix::AACDecoderHelix(out_stream);
        }  

        /**
         * @brief Construct a new AACDecoderMini object. The decoded output will go to the 
         * print object.
         * 
         * @param out_stream 
         * @param bi 
         */
        AACDecoderHelix(Print &out_stream, AudioBaseInfoDependent &bi){
        	LOGD(LOG_METHOD);
            aac = new libhelix::AACDecoderHelix(out_stream);
            setNotifyAudioChange(bi);
        }  

        /**
         * @brief Destroy the AACDecoderMini object
         * 
         */
        ~AACDecoderHelix(){
         	LOGD(LOG_METHOD);
            delete aac;
        }

        /// Defines the output Stream
		virtual void setOutputStream(Print &out_stream){
        	LOGD(LOG_METHOD);
            aac->setOutput(out_stream);
		}

        /// Starts the processing
        void begin(){
        	LOGD(LOG_METHOD);
            aac->begin();
        }

        /// Releases the reserved memory
        virtual void end(){
        	LOGD(LOG_METHOD);
            aac->end();
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
            return aac->write(aac_data, len);
        }

        /// checks if the class is active 
        virtual operator boolean(){
            return (bool)*aac;
        }

        void flush(){
        //     aac->flush();
        }

        /// Defines the callback object to which the Audio information change is provided
        virtual void setNotifyAudioChange(AudioBaseInfoDependent &bi){
        	LOGD(LOG_METHOD);
            audioChangeAACHelix = &bi;
            aac->setInfoCallback(infoCallback);
        }

        /// notifies the subscriber about a change
        static void infoCallback(_AACFrameInfo &i){
            if (audioChangeAACHelix!=nullptr){
            	LOGD(LOG_METHOD);
                AudioBaseInfo baseInfo;
                baseInfo.channels = i.nChans;
                baseInfo.sample_rate = i.sampRateOut;
                baseInfo.bits_per_sample = i.bitsPerSample;
                audioChangeAACHelix->setAudioInfo(baseInfo);   
            }
        }
    protected:
        libhelix::AACDecoderHelix *aac=nullptr;

};


} // namespace

#endif

