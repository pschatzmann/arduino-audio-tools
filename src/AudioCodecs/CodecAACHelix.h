#pragma once

#include "Stream.h"
#include "AudioTools/AudioTypes.h"

#define HELIX_LOGGING_ACTIVE false
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
        	LOGD(__FUNCTION__);
            aac = new libhelix::AACDecoderHelix();
        }
        /**
         * @brief Construct a new AACDecoderMini object
         * 
         * @param out_stream 
         */
        AACDecoderHelix(Print &out_stream){
        	LOGD(__FUNCTION__);
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
        	LOGD(__FUNCTION__);
            aac = new libhelix::AACDecoderHelix(out_stream);
            setNotifyAudioChange(bi);
        }  

        /**
         * @brief Destroy the AACDecoderMini object
         * 
         */
        ~AACDecoderHelix(){
         	LOGD(__FUNCTION__);
            delete aac;
        }

        /// Defines the output Stream
		virtual void setOutputStream(Print &out_stream){
        	LOGD(__FUNCTION__);
            aac->setOutput(out_stream);
		}

        /// Starts the processing
        void begin(){
        	LOGD(__FUNCTION__);
            aac->begin();
        }

        /// Releases the reserved memory
        virtual void end(){
        	LOGD(__FUNCTION__);
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
        	LOGD(__FUNCTION__);
            audioChangeAACHelix = &bi;
        }

        /// notifies the subscriber about a change
        static void infoCallback(_AACFrameInfo &i){
            if (audioChangeAACHelix!=nullptr){
            	LOGD(__FUNCTION__);
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

