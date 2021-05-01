
#pragma once

#include "AudioOutput.h"
#include "SoundData.h"
#include "AudioTools.h"

namespace audio_tools {

/**
 * @brief ESP8266Audio AudioOutput class which stores the data in a temporary buffer. 
 * The buffer can be consumed e.g. by a callback function by calling read();
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AudioOutputWithCallback : public AudioOutput
{
  public:
    AudioOutputWithCallback(int bufferSize, int bufferCount ){
        buffer_ptr = new NBuffer<Channels>(bufferSize, bufferCount);
    }

    virtual ~AudioOutputWithCallback() {    
        delete buffer_ptr;
    }

    /// Activates the output
    virtual bool begin() { 
        active = true;
        return true;
    }

    /// puts the sample into a buffer
    virtual bool ConsumeSample(int16_t sample[2]) {
        Channels c;
        c.channel1 = sample[0];
        c.channel2 = sample[1];
        return buffer_ptr->write(c);
    };

    /// stops the processing
    virtual bool stop() {
        active = false;
        return true;
    };

    /// Provides the data from the internal buffer to the callback
    size_t read(Channels *src, size_t len){
        return active ? this->buffer_ptr->readArray(src, len) : 0;
    }
    
  protected:
    NBuffer<Channels> *buffer_ptr;
    bool active;
};

}
