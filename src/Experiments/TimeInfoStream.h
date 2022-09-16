#pragma once

#include "AudioTools/AudioStreams.h"

namespace audio_tools {

/**
 * @brief Wrapper class that can define a start and (an optional) stop time
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

        void setStartTime(long startSeconds){
            start_time = startSeconds;
        }

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

        size_t readBytes(uint8_t *buffer, size_t length) override { 
            if (p_stream==nullptr) return 0;
            calculateTime(length); 
            return isPlaying()?p_stream->readBytes(buffer, length):length; 
        }

        size_t write(const uint8_t *buffer, size_t length) override{ 
            calculateTime(length); 
            return isPlaying()?p_print->write(buffer, length):length;  
        }

        int available() override { return p_stream!=nullptr ? p_stream->available():0; };
        int availableForWrite() override { return p_print->availableForWrite(); }

    protected:    
        Stream *p_stream=nullptr;
        Print *p_print=nullptr;
        AudioBaseInfoDependent *p_info=nullptr;
        long start_time = 0;
        long end_time = -1;
        double current_time = 0; 
        double bytes_per_second = -1.0;

        void calculateTime(int bytes){
            if (bytes_per_second<0.0){
                int channels = p_info->audioInfo().channels;
                int sample_rate = p_info->audioInfo().sample_rate; // frames per second
                int bytes_per_sample = p_info->audioInfo().bits_per_sample / 8;
                bytes_per_second =  sample_rate * channels * bytes_per_sample;
            }
            current_time = static_cast<double>(bytes) / bytes_per_second;
        }

};

}