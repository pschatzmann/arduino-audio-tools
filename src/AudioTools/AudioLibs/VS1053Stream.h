#pragma once

#include "AudioTools/CoreAudio/AudioStreams.h"
#include "AudioTools/AudioCodecs/CodecCopy.h"
#include "AudioTools/AudioCodecs/AudioEncoded.h"
#include "AudioTools/AudioCodecs/CodecWAV.h"

#if VS1053_EXT
# include "VS1053Driver.h"
#else
# include "VS1053.h"
#endif

namespace audio_tools {

enum VS1053Mode {ENCODED_MODE, PCM_MODE, MIDI_MODE };

/**
 * @brief Configuration for VS1053Stream
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class VS1053Config : public AudioInfo {
  public:
    VS1053Config(){
        sample_rate = 44100;
        channels = 2;
        bits_per_sample = 16;
    }
    RxTxMode mode = TX_MODE;
    /// set to false if it is a pcm stream
    uint8_t cs_pin = VS1053_CS; 
    uint8_t dcs_pin = VS1053_DCS;
    uint8_t dreq_pin = VS1053_DREQ;
    int16_t reset_pin = VS1053_RESET; // -1 is undefined
    int16_t cs_sd_pin = VS1053_CS_SD; 
    /// The data is encoded as WAV, MPEG etc. By default this is false and supports PCM data
    bool is_encoded_data = false;
    /// set true for streaming midi
    bool is_midi = false;
    /// SPI.begin is called by the driver (default setting)
    bool is_start_spi = true; 
#if VS1053_EXT
    VS1053_INPUT input_device = VS1053_MIC;
#endif
};

/**
 * @brief VS1053 Output Interface which processes PCM data by default. If you want to write
 * encoded data set is_encoded_data = true in the configuration;
 * @ingroup io
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class VS1053Stream : public AudioStream, public VolumeSupport {

    /**
     * @brief Private output class for the EncodedAudioStream
     */
    class VS1053StreamOut : public AudioStream {
      public:
        VS1053StreamOut(VS1053 *vs){
            p_VS1053 = vs;
        }
        size_t write(const uint8_t *data, size_t len) override { 
            if (p_VS1053==nullptr) {
                LOGE("NPE");
                return 0;
            }
            TRACED();
            p_VS1053->playChunk((uint8_t*)data, len);
            return len;
        }
      protected:
        VS1053 *p_VS1053=nullptr;
    };

public:

    VS1053Stream() = default;

    VS1053Config defaultConfig(RxTxMode mode=TX_MODE) {
        TRACED();
        VS1053Config c;
        // recording is rather inefficient so we use a low sample rate as default
        if (mode==RX_MODE){
            c.sample_rate = 8000;
        } 
        c.mode = mode;
        return c;
    }

    /// defines the default configuration that is used with the next begin()
    void setAudioInfo(VS1053Config c){
        cfg = c;
    }

    void setAudioInfo(AudioInfo c){
        cfg.copyFrom(c);
    }

    /// Starts with the default config or restarts
    bool begin() {
        return begin(cfg);
    }

    /// Starts with the indicated configuration
    bool begin(VS1053Config cfg) {
        TRACEI();
        bool result = true;
        // enfornce encoded data for midi mode
        if (cfg.is_midi){
            cfg.is_encoded_data = true;
        }
        this->cfg = cfg;
        setAudioInfo(cfg);
        cfg.logInfo();
        LOGI("is_encoded_data: %s", cfg.is_encoded_data?"true":"false");
        LOGI("is_midi: %s", cfg.is_midi?"true":"false");
        LOGI("cs_pin: %d", cfg.cs_pin);
        LOGI("dcs_pin: %d", cfg.dcs_pin);
        LOGI("dreq_pin: %d", cfg.dreq_pin);
        LOGI("reset_pin: %d", cfg.reset_pin);
        LOGI("cs_sd_pin: %d", cfg.cs_sd_pin);

        if (p_vs1053==nullptr){
           p_vs1053 = new VS1053(cfg.cs_pin,cfg.dcs_pin,cfg.dreq_pin);
           p_vs1053_out = new VS1053StreamOut(p_vs1053);

            if (cfg.is_start_spi) {
                LOGI("SPI.begin()")
                SPI.begin();
            } else {
                LOGI("SPI not started");
            }

            if (cfg.reset_pin!=-1){
                LOGI("Setting reset pin to high: %d", cfg.reset_pin);
                pinMode(cfg.reset_pin, OUTPUT);
                digitalWrite(cfg.reset_pin, HIGH);
                delay(800);
            }
        }

        // Output Stream
        if (p_out==nullptr){
            AudioEncoder *p_enc = cfg.is_encoded_data ? &copy:p_encoder;
            p_out = new EncodedAudioStream(p_vs1053_out, p_enc);   
        }

        // hack to treat midi as separate mode
        const int MIDI_MODE = 100;
        int mode = cfg.mode;
        if (cfg.is_midi){
            mode = MIDI_MODE;
        }

        switch(mode){

            case TX_MODE:
                result = beginTx();
                break;
#if VS1053_EXT
            case MIDI_MODE:
                result = beginMidi();
                break;

            case RX_MODE:
                result = beginRx();
                break;
#endif
            default:
                LOGD("Mode not supported");
                result = false;
                break;
        }
        return result;
    }

    /// Stops the processing and releases the memory
    void end(){
        TRACEI();
        if (p_out!=nullptr){
            delete p_out;
            p_out = nullptr;
        }
        if (p_vs1053!=nullptr){
            //p_driver->end();
            p_vs1053->stopSong();
            p_vs1053->softReset();
            delete p_vs1053;
            p_vs1053 = nullptr;
        }
    }

    /// value from 0 to 1.0
    bool setVolume(float vol) override {
        // make sure that value is between 0 and 1
        float volume = vol;
        if (volume>1.0) volume = 1.0;
        if (volume<0.0) volume = 0.0;
        LOGD("setVolume: %f", volume);
        if (p_vs1053!=nullptr){
            // Set the player volume.Level from 0-100, higher is louder
            p_vs1053->setVolume(volume*100.0);
        } 
        return true;
    }

    /// provides the volume
    float volume() override {
        TRACED();
        if (p_vs1053==nullptr) return -1.0;
        return p_vs1053->getVolume()/100.0;;
    }

    /// Adjusting the left and right volume balance, higher to enhance the right side, lower to enhance the left side.
    void setBalance(float bal){
        float balance = bal;
        if (balance<-1.0) balance = -1;
        if (balance>1.0) balance = 1;
        LOGD("setBalance: %f", balance);
        if (p_vs1053!=nullptr){
            p_vs1053->setBalance(balance*100.0);
        }
    }
    /// Get the currenet balance setting (-1.0..1.0)
    float balance(){
        TRACED();
        if (p_vs1053==nullptr) return -1.0;
        return static_cast<float>(p_vs1053->getBalance())/100.0;
    }

    /// Write audio data
    virtual size_t write(const uint8_t *data, size_t len) override { 
        TRACED();
        if (len==0) return 0;
        if (p_out==nullptr) {
            LOGE("vs1053 is closed");
            return 0;
        }
        return p_out->write(data, len);
    }

    /// returns the VS1053 object
    VS1053 &getVS1053() {
        TRACED();
        return *p_vs1053;
    }

    /// Defines an alternative encoder that will be used (e.g. MP3Encoder). It must be allocated on the heap!
    bool setEncoder(AudioEncoder *enc){
        TRACEI();
        if (p_out!=nullptr){
            logError("setEncoder");
            return false;
        }
        if (p_encoder!=nullptr){
            delete p_encoder;
            p_encoder = enc;
        }
        return true;
    }

#if VS1053_EXT
    int available() override {
        int result  = getVS1053().available();
        LOGI("available: %d", result);
        return result;
    }
    size_t readBytes(uint8_t* data, size_t len) override {
        TRACED();
        return getVS1053().readBytes(data, len);
    }

    /// Provides the treble amplitude value
    float treble() {
        TRACED();
        return static_cast<float>(getVS1053().treble())/100.0;
    }

    /// Sets the treble amplitude value (range 0 to 1.0)
    void setTreble(float val){
        float value = val;
        if (value<0.0) value = 0.0;
        if (value>1.0) value = 1.0;
        LOGD("setTreble: %f", value);
        getVS1053().setTreble(value*100);
    }

    /// Provides the Bass amplitude value 
    float bass() {
        TRACED();
        return static_cast<float>(getVS1053().bass())/100.0;
    }

    /// Sets the bass amplitude value (range 0 to 1.0)
    void setBass(float val){
        float value = val;
        if (value<0.0) value = 0.0;
        if (value>1.0) value = 1.0;
        LOGD("setBass: %f", value);
        getVS1053().setBass(value*100.0);
    }

    /// Sets the treble frequency limit in hz (range 0 to 15000)
    void setTrebleFrequencyLimit(uint16_t value){
        LOGD("setTrebleFrequencyLimit: %u", value);
        getVS1053().setTrebleFrequencyLimit(value);
    }
    /// Sets the bass frequency limit in hz (range 0 to 15000)
    void setBassFrequencyLimit(uint16_t value){
        LOGD("setBassFrequencyLimit: %u", value);
        getVS1053().setBassFrequencyLimit(value);
    }

    /// Sends a midi message to the VS1053
    void sendMidiMessage(uint8_t cmd, uint8_t data1, uint8_t data2) {
        TRACEI();
#if USE_MIDI
        if (!cfg.is_midi){
            LOGE("start with is_midi=true");
            return;
        }
        if (p_vs1053==nullptr) {
            logError(__FUNCTION__);
            return;
        }
        p_vs1053->sendMidiMessage(cmd, data1, data2);
#endif
    }

#endif

protected:
    VS1053Config cfg;
    VS1053 *p_vs1053 = nullptr;
    VS1053StreamOut *p_vs1053_out = nullptr;
    EncodedAudioStream *p_out = nullptr;
    AudioEncoder *p_encoder = new WAVEncoder(); // by default we send wav data 
    CopyEncoder copy;  // used when is_encoded_data == true

    bool beginTx() {
        TRACEI();
        p_out->begin(cfg);      
        bool result = p_vs1053->begin();
        p_vs1053->startSong();
        p_vs1053->switchToMp3Mode(); // optional, some boards require this    
        if (p_vs1053->getChipVersion() == 4) { // Only perform an update if we really are using a VS1053, not. eg. VS1003
            p_vs1053->loadDefaultVs1053Patches(); 
        }
        delay(500);
        setVolume(VS1053_DEFAULT_VOLUME);   
        return result;
    }

#if VS1053_EXT

    bool beginRx() {
        TRACEI();
        VS1053Recording rec;
        rec.setSampleRate(cfg.sample_rate);
        rec.setChannels(cfg.channels);
        rec.setInput(cfg.input_device);
        return p_vs1053->beginInput(rec);
    }

    bool beginMidi(){
#if USE_MIDI
        TRACEI();
        p_out->begin(cfg);      
        bool result = p_vs1053->beginMidi();
        delay(500);
        setVolume(VS1053_DEFAULT_VOLUME);   
        return result;
#else
        return false;
#endif
    }

#endif
    void logError(const char* str){
        LOGE("Call %s after begin()", str);
    }


};

}