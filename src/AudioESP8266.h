#pragma once

#include "AudioTools.h"
#ifdef USE_ESP8266_AUDIO
#include "AudioOutput.h"
#include "SoundData.h"

namespace audio_tools {

/**
 * @brief ESP8266Audio AudioOutput class which stores the data in a temporary buffer. 
 * The buffer can be consumed e.g. by a callback function by calling read();

 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AudioOutputWithCallback : public AudioOutput, public BufferedStream {
    public:
        // Default constructor
        AudioOutputWithCallback(int bufferSize, int bufferCount ):BufferedStream(bufferSize) {
            callback_buffer_ptr = new NBuffer<Channels>(bufferSize, bufferCount);
        }

        virtual ~AudioOutputWithCallback() {    
            delete callback_buffer_ptr;
        }

        /// Activates the output
        virtual bool begin() { 
            active = true;
            return true;
        }

        /// For ESP8266 Audio Framework -  puts the sample into a buffer
        virtual bool ConsumeSample(int16_t sample[2]) {
            Channels c;
            c.channel1 = sample[0];
            c.channel2 = sample[1];
            return callback_buffer_ptr->write(c);
        };
        
        /// stops the processing
        virtual bool stop() {
            active = false;
            return true;
        };

        /// Provides the data from the internal buffer to the callback
        size_t read(Channels *src, size_t len){
            return active ? this->callback_buffer_ptr->readArray(src, len) : 0;
        }

    
  protected:
        NBuffer<Channels> *callback_buffer_ptr;
        bool active;

        virtual size_t writeExt(const uint8_t* data, size_t len) {    
            return callback_buffer_ptr->writeArray((Channels*)data, len/sizeof(Channels));
        }

        virtual size_t readExt( uint8_t *data, size_t len) { 
            return callback_buffer_ptr->readArray((Channels*)data, len/sizeof(Channels));;
        }

};



}

#endif

