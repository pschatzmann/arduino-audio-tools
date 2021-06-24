#pragma once

#ifdef USE_ESP8266_AUDIO

// support for the ESP8266 Audio Library
#include "AudioTools.h"
#include "AudioOutput.h"
#include "SoundData.h"


class AudioOutputWithCallback : public AudioOutput, public CallbackStream<Channels> {
    public:
        // Default constructor
        AudioOutputWithCallback(int bufferSize, int bufferCount ):CallbackStream<Channels>(bufferSize) {
        }

        /// Additinal method for ESP8266 Audio Framework -  puts the sample into a buffer
        virtual bool ConsumeSample(int16_t sample[2]) {
            Channels c;
            c.channel1 = sample[0];
            c.channel2 = sample[1];
            return callback_buffer_ptr->write(c);
        };

        /// Provides the data from the internal buffer to the callback
        size_t read(Channels *src, size_t len){
            return active ? this->callback_buffer_ptr->readArray(src, len) : 0;
        }


}
#endif