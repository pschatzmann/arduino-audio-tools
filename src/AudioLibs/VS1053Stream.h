#pragma once

#include "AudioTools/AudioStreams.h"
#include "VS1053.h"
#include "AudioCodecs/CodecCopy.h"
namespace audio_tools {

/**
 * @brief VS1053 Output Interface Class based on https://github.com/baldram/ESP_VS1053_Library
 * The write is expecting encoded data.
 * @copyright GPLv3
 */
class VS1053BaseStream : public AudioStreamX {
public:
    VS1053BaseStream(uint8_t _cs_pin, uint8_t _dcs_pin, uint8_t _dreq_pin,int16_t _reset_pin=-1){
        this->_cs_pin = _cs_pin;
        this->_dcs_pin = _dcs_pin;
        this->_dreq_pin = _dreq_pin;
        this->_reset_pin = _reset_pin;
    }

    bool begin() {
        p_vs1053 = new VS1053(_cs_pin,_dcs_pin,_dreq_pin, _reset_pin);
        // initialize SPI
        SPI.begin();

        p_vs1053->begin();
        p_vs1053->startSong();
        p_vs1053->switchToMp3Mode(); // optional, some boards require this    
        if (p_vs1053->getChipVersion() == 4) { // Only perform an update if we really are using a VS1053, not. eg. VS1003
            p_vs1053->loadDefaultVs1053Patches(); 
        }
        return true;
    }

    void end(){
        if (p_vs1053!=nullptr){
            p_vs1053->stopSong();
            delete p_vs1053;
            p_vs1053 = nullptr;
        }
    }

    /// value from 0 to 1.0
    void setVolume(float vol){
        // make sure that value is between 0 and 1
        float volume = vol;
        if (volume>1.0) volume = 1.0;
        if (volume<0.0) volume = 0.0;
        if (p_vs1053!=nullptr){
            // Set the player volume.Level from 0-100, higher is louder
            p_vs1053->setVolume(volume*100.0);
        } 
    }

    /// provides the volume
    float volume() {
        if (p_vs1053==nullptr) return -1.0;
        return p_vs1053->getVolume()/100.0;;
    }

    /// Adjusting the left and right volume balance, higher to enhance the right side, lower to enhance the left side.
    void setBalance(float bal){
        float balance = bal;
        if (balance<-1.0) balance = -1;
        if (balance>1.0) balance = 1;
        if (p_vs1053!=nullptr){
            p_vs1053->setBalance(balance*100.0);
        }
    }
    /// Get the currenet balance setting (-1.0..1.0)
    float balance(){
        if (p_vs1053==nullptr) return -1.0;
        return static_cast<float>(p_vs1053->getBalance())/100.0;
    }

    /// Provides the treble amplitude value
    float treble() {
        if (p_vs1053==nullptr) return -1.0;
        return static_cast<float>(p_vs1053->treble())/100.0;
    }

    /// Sets the treble amplitude value (range 0 to 1.0)
    void setTreble(float val){
        float value = val;
        if (value<0.0) value = 0.0;
        if (value>1.0) value = 1.0;
        if (p_vs1053!=nullptr){
            p_vs1053->setTreble(value*100);
        }
    }

    /// Provides the Bass amplitude value 
    float bass() {
        if (p_vs1053==nullptr) return -1;
        return static_cast<float>(p_vs1053->bass())/100.0;
    }

    /// Sets the bass amplitude value (range 0 to 1.0)
    void setBass(float value){
        if (p_vs1053!=nullptr){
            p_vs1053->setBass(value*100.0);
        }
    }

    /// Sets the treble frequency limit in hz (range 0 to 15000)
    void setTrebleFrequencyLimit(uint16_t value){
        if (p_vs1053!=nullptr){
            p_vs1053->setTrebleFrequencyLimit(value);
        }
    }
    /// Sets the bass frequency limit in hz (range 0 to 15000)
    void setBassFrequencyLimit(uint16_t value){
        if (p_vs1053!=nullptr){
            p_vs1053->setBassFrequencyLimit(value);
        }
    }

    /// Write encoded (mp3, aac, wav etc) data
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
    int16_t _reset_pin=-1;
};

/**
 * @brief Configuration for VS1053Stream
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class VS1053Config : public AudioBaseInfo {
  public:
    VS1053Config(){
        sample_rate = 44100;
        channels = 2;
        bits_per_sample = 16;
    }
    /// set to false if it is a pcm stream
    bool is_encoded_data = false;
    uint8_t cs_pin = VS1053_CS; 
    uint8_t dcs_pin = VS1053_DCS;
    uint8_t dreq_pin = VS1053_DREQ;
    int16_t reset_pin = VS1053_RESET; // -1 is undefined
    uint8_t cs_sd_pin = VS1053_CS_SD; 
};

/**
 * @brief VS1053 Output Interface which processes PCM data by default. If you want to write
 * encoded data set is_encoded_data = true in the configuration;
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class VS1053Stream : public AudioStreamX {
public:
    VS1053Stream(){
    }

    VS1053Config defaultConfig() {
        VS1053Config c;
        return c;
    }

    /// Starts with the default config or restarts
    bool begin() {
        return begin(cfg);
    }

    /// Starts with the indicated configuration
    bool begin(VS1053Config cfg) {
        this->cfg = cfg;
        setAudioInfo(cfg);
        if (p_driver==nullptr){
            p_driver = new VS1053BaseStream(cfg.cs_pin,cfg.dcs_pin,cfg.dreq_pin, cfg.reset_pin);
        }
        if (p_out==nullptr){
            AudioEncoder *p_enc = cfg.is_encoded_data ? p_encoder :&copy;
            p_out = new EncodedAudioStream(p_driver, p_enc);   
        }
        p_out->begin(cfg);      
        p_driver->begin();
    }

    /// Stops the processing and releases the memory
    void end(){
        if (p_out!=nullptr){
            delete p_out;
            p_out = nullptr;
        }
        if (p_driver!=nullptr){
            p_driver->end();
            delete p_driver;
            p_driver = nullptr;
        }
    }

    /// Sets the volume: value from 0 to 1.0
    void setVolume(float volume){
        if (p_driver==nullptr) {
            logError(__FUNCTION__);
            return;
        };
        p_driver->setVolume(volume);
    }
    /// provides the volume
    float volume() {
        if (p_driver==nullptr) {
            logError(__FUNCTION__);
            return -1;
        }
        return p_driver->volume();
    }

    /// Adjusting the left and right volume balance, higher to enhance the right side, lower to enhance the left side.
    void setBalance(float balance){
        if (p_driver==nullptr) {
            logError(__FUNCTION__);
            return;
        }
        p_driver->setBalance(balance*100.0);
    }
    /// Get the currenet balance setting (-1.0..1.0)
    float balance(){
        if (p_driver==nullptr) {
            logError(__FUNCTION__);
            return -1;
        }
        return p_driver->balance();
    }

    /// Provides the treble amplitude value
    float treble() {
        if (p_driver==nullptr) {
            logError(__FUNCTION__);
            return -1;
        }
        return p_driver->treble();
    }

    /// Sets the treble amplitude value (range 0 to 1.0)
    void setTreble(float value){
        if (p_driver==nullptr) {
            logError(__FUNCTION__);
            return;
        }
        p_driver->setTreble(value);
    }

    /// Provides the Bass amplitude value 
    float bass() {
        if (p_driver==nullptr) {
            logError(__FUNCTION__);
            return -1;
        }
        return p_driver->bass();
    }

    /// Sets the bass amplitude value (range 0 to 1.0)
    void setBass(float value){
        if (p_driver==nullptr) {
            logError(__FUNCTION__);
            return;
        }
        p_driver->setBass(value*100.0);
    }

    /// Sets the treble frequency limit in hz (range 0 to 15000)
    void setTrebleFrequencyLimit(uint16_t value){
        if (p_driver==nullptr) {
            logError(__FUNCTION__);
            return;
        }
        p_driver->setTrebleFrequencyLimit(value);
    }
    /// Sets the bass frequency limit in hz (range 0 to 15000)
    void setBassFrequencyLimit(uint16_t value){
        if (p_driver==nullptr) {
            logError(__FUNCTION__);
            return;
        }
        p_driver->setBassFrequencyLimit(value);
    }

    /// Write audio data
    virtual size_t write(const uint8_t *buffer, size_t size) override{ 
        if (p_out==nullptr) return 0;
        return p_out->write(buffer, size);
    }

    /// returns the VS1053 object
    VS1053 &getVS1053() {
        return p_driver->getVS1053();
    }

    /// Defines an alternative encoder that will be used (e.g. MP3Encoder). It must be allocated on the heap!
    bool setEncoder(AudioEncoder *enc){
        if (p_out!=nullptr){
            logError("setEncoder");
            return false;
        }
        delete p_encoder;
        p_encoder = enc;
        return true;
    }

protected:
    VS1053Config cfg;
    VS1053BaseStream *p_driver=nullptr;
    EncodedAudioStream *p_out = nullptr;
    AudioEncoder *p_encoder = new WAVEncoder(); // by default we send wav data 
    CopyEncoder copy;  // used when is_encoded_data == true

    void logError(const char* str){
        LOGE("Call %s after begin()", str);
    }
};



}