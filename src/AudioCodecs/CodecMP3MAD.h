#pragma once

#define MINIMP3_IMPLEMENTATION
#define MINIMP3_NO_STDIO
#define LOGGING_ACTIVE true

#include "Stream.h"
#include "AudioTools/AudioTypes.h"
#include "MP3DecoderMAD.h"

namespace audio_tools {

// forward audio changes
AudioBaseInfoDependent *audioChangeMAD;

/**
 * @brief MP3 Decoder using https://github.com/pschatzmann/arduino-libmad
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class MP3DecoderMAD : public AudioDecoder  {
    public:

        MP3DecoderMAD(){
            LOGD(LOG_METHOD);
            mad = new libmad::MP3DecoderMAD();
        }

        MP3DecoderMAD(libmad::MP3DataCallback dataCallback, libmad::MP3InfoCallback infoCB=nullptr){
            LOGD(LOG_METHOD);
            mad = new libmad::MP3DecoderMAD(dataCallback, infoCB);
        }

        MP3DecoderMAD(Print &mad_output_streamput, libmad::MP3InfoCallback infoCB = nullptr){
            LOGD(LOG_METHOD);
            mad = new libmad::MP3DecoderMAD(mad_output_streamput, infoCB);
        }

        ~MP3DecoderMAD(){
            LOGD(LOG_METHOD);
            delete mad;
        }

        void setOutputStream(Print &out){
            LOGD(LOG_METHOD);
            mad->setOutput(out);
        }

        /// Defines the callback which receives the decoded data
        void setAudioDataCallback(libmad::MP3DataCallback cb){
            LOGD(LOG_METHOD);
            mad->setDataCallback(cb);
        }

        /// Defines the callback which receives the Info changes
        void setInfoCallback(libmad::MP3InfoCallback cb){
            LOGD(LOG_METHOD);
            mad->setInfoCallback(cb);
        }

         /// Starts the processing
        void begin(){
            LOGD(LOG_METHOD);
            mad->begin();
        }

        /// Releases the reserved memory
        void end(){
            LOGD(LOG_METHOD);
            mad->end();
        }

        /// Provides the last valid audio information
        libmad::MadAudioInfo audioInfoEx(){
            LOGD(LOG_METHOD);
            return mad->audioInfo();
        }

        AudioBaseInfo audioInfo(){
            LOGD(LOG_METHOD);
            libmad::MadAudioInfo info = audioInfoEx();
            AudioBaseInfo base;
            base.channels = info.channels;
            base.sample_rate = info.sample_rate;
            base.bits_per_sample = info.bits_per_sample; 
            return base;
        }

        /// Makes the mp3 data available for decoding: however we recommend to provide the data via a callback or input stream
        size_t write(const void *data, size_t len){
            LOGD(LOG_METHOD);
            return mad->write(data,len);
        }

        /// Makes the mp3 data available for decoding: however we recommend to provide the data via a callback or input stream
        size_t write(void *data, size_t len){
            LOGD(LOG_METHOD);
            return mad->write(data,len);
        }

        /// Returns true as long as we are processing data
        operator bool(){
            return (bool)*mad;
        }

        libmad::MP3DecoderMAD *driver() {
            return mad;
        }

        static void audioChangeCallback(libmad::MadAudioInfo &info){
            if (audioChangeMAD!=nullptr){
                LOGD(LOG_METHOD);
                AudioBaseInfo base;
                base.channels = info.channels;
                base.sample_rate = info.sample_rate;
                base.bits_per_sample = info.bits_per_sample;
                // notify audio change
                audioChangeMAD->setAudioInfo(base);
            }
        }

        virtual void setNotifyAudioChange(AudioBaseInfoDependent &bi) {
            LOGD(LOG_METHOD);
            audioChangeMAD = &bi;
            // register audio change handler
            mad->setInfoCallback(audioChangeCallback);
        }

    protected:
        libmad::MP3DecoderMAD *mad;

};

} // namespace


