#pragma once

#include "AudioTools/AudioStreams.h"
#include "VS1053.h"

namespace audio_tools {

/**
 * @brief VS1053 Output Interface Class based on https://github.com/baldram/ESP_VS1053_Library
 * 
 */
class VS1053Stream : public AudioStreamX {
public:
    VS1053Stream(uint8_t _cs_pin, uint8_t _dcs_pin, uint8_t _dreq_pin){
        this->_cs_pin = _cs_pin;
        this->_dcs_pin = _dcs_pin;
        this->_dreq_pin = _dreq_pin;
    }

    bool begin() {
        p_vs1053 = new VS1053(_cs_pin,_dcs_pin,_dreq_pin );
        p_vs1053->begin();
        p_vs1053->startSong();
        return true;
    }

    void end(){
        p_vs1053->stopSong();
        delete p_vs1053;
        p_vs1053 = nullptr;
    }
    /// value from 0 to 1.0
    void setVolume(float volume){
        // make sure that value is between 0 and 1
        float v = volume;
        if (v>1.0) v = 1.0;
        if (v<0) v = 0.0;
        // Set the player volume.Level from 0-100, higher is louder
        p_vs1053->setVolume(v*100);
    }

    virtual size_t write(const uint8_t *buffer, size_t size) override{ 
        if (p_vs1053==nullptr) return 0;
        p_vs1053->playChunk((uint8_t*)buffer, size);
        return size;
    }

    VS1053 &getVS1053() {
        if (p_vs1053==nullptr) begin();
        return *p_vs1053;
    }

protected:
    VS1053 *p_vs1053 = nullptr;
    uint8_t _cs_pin,  _dcs_pin,  _dreq_pin;

};

}