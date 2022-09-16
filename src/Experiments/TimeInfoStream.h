#pragma once

#include "AudioTools/AudioStreams.h"

namespace audio_tools {

/**
 * @brief Wrapper class that can define a start and (an optional) stop time
 * Usually it is used to wrap an Audio Sink (e.g. I2SStream), but wrapping an Audio Source is
 * supported as well.
 * Only wrap classes which represent PCM data!
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class TimeInfoStream : public AudioStreamX {
    public:
        TimeInfoStream(AudioStream &io, long startSeconds=0, long endSeconds=-1){
            p_stream = &io;
            p_print = &io;
            p_info = &io;
            start_time = startSeconds;
            end_time = endSeconds;
        }

        TimeInfoStream(AudioPrint &o,long startSeconds=0, long endSeconds=-1){
            p_print = &o;
            p_info = &o;
            start_time = startSeconds;
            end_time = endSeconds;
        }

        /// Defines the start time in seconds. The audio before the start time will be skipped
        void setStartTime(long startSeconds){
            start_time = startSeconds;
        }

        /// Defines (an optional) the end time in seconds. After the end time no audio is played and available() will return 0
        void setEndTime(long endSeconds){
            end_time = endSeconds;
        }

        /// Resets the current time 
        void setCurrentTime(double timeScalculateTimeeStartSeconds=0) {
            current_time = timeScalculateTimeeStartSeconds;
            bytes_per_second = -1.0; // trigger new calculation
        }

        /// Provides the current time in seconds from the start
        double currentTime() {
            return current_time;
        }

        /// Returns true if we are in a valid time range and are still playing sound 
        bool isPlaying() {
            if (current_time<start_time) return false;
            if (end_time>0 && current_time>end_time) return false;
            return true;
        }

        /// Returns true if we are not past the end time; 
        bool isActive() {
            if (end_time>0 && current_time>end_time) return false;
            return true;
        }

        operator bool() {
            return isActive();
        }

        /// Provides only data for the indicated start and end time. Only supported for 
        /// data which does not contain any heder information: so PCM, mp3 should work!
        size_t readBytes(uint8_t *buffer, size_t length) override { 
            // if reading is not supported we stop
            if (p_stream==nullptr) return 0;
            // if we are past the end we stop
            if (!isActive()) return 0;
            // read the data now
            size_t result=0;
            do {
                result = p_stream->readBytes(buffer, length);
                calculateTime(result); 
                // ignore data before start time
            } while(result>0 && current_time<start_time);
            return isPlaying()?result : 0; 
        }

        /// Plays only data for the indiated start and end time
        size_t write(const uint8_t *buffer, size_t length) override{ 
            calculateTime(length); 
            return isPlaying()?p_print->write(buffer, length):length;  
        }

        /// Provides the available bytes until the end time has reached 
        int available() override { 
            if (p_stream==nullptr) return 0;
            return isActive() ? p_stream->available() : 0; 
        }

        void setAudioInfo(AudioBaseInfo info) override {
            p_info->setAudioInfo(info);
        }

        int availableForWrite() override { return p_print->availableForWrite(); }

        /// Experimental: if used on mp3 you can set the compression ratio e.g. to 11 which will
        /// be used to approximate the time
        void setCompressionRatio(float ratio){
            compression_ratio = ratio;
        }

    protected:    
        Stream *p_stream=nullptr;
        Print *p_print=nullptr;
        AudioBaseInfoDependent *p_info=nullptr;
        long start_time = 0;
        long end_time = -1;
        double current_time = 0; 
        double bytes_per_second = -1.0;
        float compression_ratio = 1.0;

        void calculateTime(int bytes){
            if (bytes_per_second<0.0){
                int channels = p_info->audioInfo().channels;
                int sample_rate = p_info->audioInfo().sample_rate; // frames per second
                int bytes_per_sample = p_info->audioInfo().bits_per_sample / 8;
                bytes_per_second =  sample_rate * channels * bytes_per_sample;
            }
            current_time = (static_cast<double>(bytes) / bytes_per_second) * compression_ratio;
        }

};

}