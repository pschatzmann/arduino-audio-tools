#pragma once

#define MINIMP3_IMPLEMENTATION
#define MINIMP3_NO_STDIO
#define LOGGING_ACTIVE true

#include "Stream.h"
#include "AudioTools/AudioTypes.h"
#include "MP3DecoderMAD.h"

namespace audio_tools {

// forward audio changes
AudioBaseInfoDependent audioChangeMAD;

/**
 * @brief MP3 Decoder using https://github.com/pschatzmann/arduino-libmad
 * 
 */
class MP3DecoderMAD : public AudioDecoder  {
    public:

        MP3DecoderMAD(){
            mad = new libmad::MP3DecoderMAD();
        }

        MP3DecoderMAD(libmad::MP3DataCallback dataCallback, libmad::MP3InfoCallback infoCB=nullptr){
            mad = new libmad::MP3DecoderMAD(dataCallback, infoCB);
        }

        MP3DecoderMAD(Print &mad_output_streamput, libmad::MP3InfoCallback infoCB = nullptr){
            mad = new libmad::MP3DecoderMAD(mad_output_streamput, infoCB);
        }

        ~MP3DecoderMAD(){
            delete mad;
        }

        void setOutputStream(Print &mad_output_streamput){
            mad->setOutput(input);
        }

        void setInputStream(Stream &input){
            mad->setInput(input);
        }

        /// Defines the callback which receives the decoded data
        void setDataCallback(libmad::MP3DataCallback cb){
            mad->setDataCallback(cb);
        }

        /// Defines the callback which receives the Info changes
        void setInfoCallback(libmad::MP3InfoCallback cb){
            mad->setInfoCallback(cb);
        }

        /// Defines the callback which provides input data
        void setInputCallback(libmad::MP3MadInputDataCallback input){
            mad->setInputCallback(input);
        }

         /// Starts the processing
        void begin(){
            mad->begin();
        }

        /// Releases the reserved memory
        void end(){
            mad->end();
        }

        /// Provides the last valid audio information
        libmad::MadAudioInfo audioInfoEx(){
            return mad->audioInfo();
        }

        BaseAudioInfo audioInfo(){
            MadAudioInfo info = audioInfoEx();
            AudioBaseInfo base;
            base.channels = info.channels;
            base.sample_rate = info.sample_rate;
            base.bits_per_sample = info.bits_per_sample; 
            return base;
        }

        /// Makes the mp3 data available for decoding: however we recommend to provide the data via a callback or input stream
        size_t write(const void *data, size_t len){
            return mad->write(data,len);
        }

        /// Makes the mp3 data available for decoding: however we recommend to provide the data via a callback or input stream
        size_t write(void *data, size_t len){
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
			if (audioChangeFDK!=nullptr){
				AudioBaseInfo base;
				base.channels = info.channels;
				base.sample_rate = info.sample_rate;
				base.bits_per_sample = info.bits_per_sample;
				// notify audio change
				audioChangeFDK->setAudioInfo(base);
			}
		}

		virtual void setNotifyAudioChange(AudioBaseInfoDependent &bi) {
			audioChangeMAD = &bi;
			// register audio change handler
			dec->setInfoCallback(audioChangeCallback);
		}

    protected:
        libmad::MP3DecoderMAD *mad;

};

} // namespace

