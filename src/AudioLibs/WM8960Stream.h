#pragma once

#include "AudioTools/AudioStreams.h"
#include "AudioI2S/I2SStream.h"
#include "mtb_wm8960.h" // https://github.com/pschatzmann/arduino-wm8960

namespace audio_tools {

/**
 * @brief Configuration for WM8960
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class WM8960Config : public I2SConfig {
  public:
    WM8960Config() : I2SConfig() {}
    WM8960Config(RxTxMode mode):I2SConfig(mode){
        sample_rate = 44100;
        channels = 2;
        bits_per_sample = 16;
    }
    /// Volume that is used on start (range 0.0 to 1.0)
    float default_volume = 0.6;
    /// enalbel pll for wm8960 - default is true
    bool vs1053_enable_pll = true;
    /// masterclock rate for wm8960 - default is 0
    uint32_t vs1053_mclk_hz = 0;
    /// Define wire if we do not use the default Wire object
    TwoWire *wire=nullptr;
    /// Dump registers
    bool vs1053_dump = false;
    /// Number of i2c write retry on fail: 0 = endless until success
    uint32_t i2c_retry_count = 0;
    // optional features: use bitmask with WM8960_FEATURE_MICROPHONE,WM8960_FEATURE_HEADPHONE,WM8960_FEATURE_SPEAKER
    int8_t features = -1;           

};

/**
 * @brief Stream for reading and writing audio data using the WM8960 Codec Chip
 * You need to install https://github.com/pschatzmann/arduino-wm8960
 * @ingroup io
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class WM8960Stream : public AudioStream {

public:

    WM8960Stream() = default;

    WM8960Config defaultConfig(RxTxMode mode=TX_MODE) {
        TRACED();
        WM8960Config c(mode);
        return c;
    }

    /// defines the default configuration that is used with the next begin()
    void setAudioInfo(WM8960Config c){
        cfg = c;
        begin(c);
    }

    void setAudioInfo(AudioInfo c){
        cfg.copyFrom(c);
        begin(cfg);
    }

    /// Starts with the default config or restarts
    bool begin() {
        return begin(cfg);
    }

    /// Starts with the indicated configuration
    bool begin(WM8960Config config) {
        TRACEI();
        cfg = config;

        // setup wm8960
        if (!init(cfg.rx_tx_mode)){
            LOGE("init");
            return false;
        }
        setVolume(cfg.default_volume);
        if (!mtb_wm8960_activate()){
            LOGE("mtb_wm8960_activate");
            return false;
        }
        if (!configure_clocking()){
            LOGE("configure_clocking");
            return false;
        }
        if (config.vs1053_dump){
            mtb_wm8960_dump();
        }

        // setup output
        i2s.begin(cfg);

        return true;
    }

    /// Stops the processing and releases the memory
    void end(){
        TRACEI();
        i2s.end();
        mtb_wm8960_deactivate();
        mtb_wm8960_free();
    }

    /// Sets both input and output volume value (from 0 to 1.0)
    bool setVolume(float vol){
        // make sure that value is between 0 and 1
        setVolumeIn(vol);
        setVolumeOut(vol);
        return true;
    }

    void setVolumeIn(float vol){
        adjustInputVolume(vol);
    }

    void setVolumeOut(float vol){
        setOutputVolume(vol);        
    }

    /// provides the volume
    float volumeIn() {
        return volume_in;
    }

    float volumeOut() {
        return volume_out;
    }

    size_t readBytes (uint8_t *data, size_t size) override{
        return i2s.readBytes(data, size);
    }

    size_t write (const uint8_t *data, size_t size) override {
        return i2s.write(data, size);
    }

protected:
    WM8960Config cfg;
    I2SStream i2s;
    float volume_in;
    float volume_out;

    void adjustInputVolume(float vol){
        if (vol>1.0f) {
            volume_in = 1.0f;
            volumeError(vol);
        } else if (vol<0.0f){
            volume_in = 0.0f;
            volumeError(vol);
        } else {
            volume_in = vol;
        }
        int vol_int = map(volume_in*100, 0, 100, 0 ,30);
        mtb_wm8960_adjust_input_volume(vol_int);
    }

    void setOutputVolume(float vol){
        if (vol>1.0f) {
            volume_out = 1.0f;
            volumeError(vol);
        } else if (vol<0.0f){
            volume_out = 0.0f;
            volumeError(vol);
        } else {
            volume_out = vol;
        }
        int vol_int = volume_out==0.0? 0 : map(volume_out*100, 0, 100, 30 ,0x7F);
        mtb_wm8960_set_output_volume(vol_int);
    }

    bool init(RxTxMode mode){
        mtb_wm8960_set_write_retry_count(cfg.i2c_retry_count);
        // define wire object
        mtb_wm8960_set_wire(cfg.wire);

        // init features if not defined depending on mode
        if (cfg.features==-1){
            switch(mode){
                case RX_MODE:
                    cfg.features = WM8960_FEATURE_MICROPHONE1;
                    break;
                case TX_MODE:
                    cfg.features = WM8960_FEATURE_HEADPHONE | WM8960_FEATURE_SPEAKER;
                    break;
                case RXTX_MODE: 
                    cfg.features = WM8960_FEATURE_MICROPHONE1 | WM8960_FEATURE_HEADPHONE | WM8960_FEATURE_SPEAKER;
                    break;
            }
            LOGW("Setup features: %d", cfg.features);
        }
        return mtb_wm8960_init(cfg.features);
    }

    bool configure_clocking(){
        if (cfg.vs1053_mclk_hz==0){
            // just pick a multiple of the sample rate
            cfg.vs1053_mclk_hz = 512 * cfg.sample_rate;
        }
        if (!mtb_wm8960_configure_clocking(cfg.vs1053_mclk_hz, cfg.vs1053_enable_pll, sampleRate(cfg.sample_rate), wordLength(cfg.bits_per_sample), modeMasterSlave(cfg.is_master))){
            LOGE("mtb_wm8960_configure_clocking");
            return false;
        }
        return true;
    }

    mtb_wm8960_adc_dac_sample_rate_t sampleRate(int rate){
        switch(rate){
            case 48000:
                return WM8960_ADC_DAC_SAMPLE_RATE_48_KHZ;
            case 44100:
                return WM8960_ADC_DAC_SAMPLE_RATE_44_1_KHZ;
            case 32000:
                return WM8960_ADC_DAC_SAMPLE_RATE_32_KHZ;
            case 24000:
                return WM8960_ADC_DAC_SAMPLE_RATE_24_KHZ;
            case 22050:
                return WM8960_ADC_DAC_SAMPLE_RATE_22_05_KHZ;
            case 16000:
                return WM8960_ADC_DAC_SAMPLE_RATE_16_KHZ;
            case 12000:
                return WM8960_ADC_DAC_SAMPLE_RATE_12_KHZ;
            case 11025:
                return WM8960_ADC_DAC_SAMPLE_RATE_11_025_KHZ;
            case 8018:
                return WM8960_ADC_DAC_SAMPLE_RATE_8_018_KHZ;
            case 8000:
                return WM8960_ADC_DAC_SAMPLE_RATE_8_KHZ;
            default:
                LOGE("Unsupported rate: %d",rate);
                return WM8960_ADC_DAC_SAMPLE_RATE_44_1_KHZ;
        }
    }

    mtb_wm8960_word_length_t wordLength(int bits){
        switch(bits){
            case 16:
                return WM8960_WL_16BITS;
            case 20:
                return WM8960_WL_20BITS;
            case 24:
                return WM8960_WL_24BITS;
            case 32:
                return WM8960_WL_32BITS;
            default:
                LOGE("Unsupported bits: %d", bits);
                return WM8960_WL_16BITS;
        }
    }

    /// if microcontroller is master then module is slave
    mtb_wm8960_mode_t modeMasterSlave(bool microcontroller_is_master){
        return microcontroller_is_master ? WM8960_MODE_SLAVE : WM8960_MODE_MASTER;
    }

    void volumeError(float vol){
        LOGE("Invalid volume %f", vol);
    }
};

}