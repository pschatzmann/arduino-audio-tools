#pragma once
#ifdef USE_USB
#if! __has_include("mbed.h") 
#error "This functionality is only supported on ARM Mbed devices"
#endif

#include "mbed.h"
#include "USBAudio.h"
#include "AudioTools.h"


namespace audio_tools {

/**
 * @brief Stream support for mbed USBAudio https://os.mbed.com/docs/mbed-os/v6.10/mbed-os-api-doxy/class_u_s_b_audio.html
 * @author Phil Schatzmann
 * @copyright GPLv3
*/
class AudioUSB : public BufferedStream {
    public:
        AudioUSB():BufferedStream(DEFAULT_BUFFER_SIZE){
        }

        ~AudioUSB(){
            end();
        } 

        void begin(uint32_t frequency_tx=8000, uint8_t channel_count_tx=1, uint32_t frequency_rx=48000, uint8_t channel_count_rx=1,  uint32_t buffer_ms=10, uint16_t vendor_id=0x7bb8, uint16_t product_id=0x1111, uint16_t product_release=0x0100){
            if (audio!=nullptr) {
                audio = new USBAudio(true,frequency_rx,channel_count_rx,frequency_tx,channel_count_tx,buffer_ms,vendor_id,product_id,product_release);
            }
        }

        void end(){
            if (audio!=nullptr)
                delete audio;
            audio = nullptr;
        }

        operator bool() {
            return audio != nullptr;
        }

    protected:
        USBAudio *audio = nullptr;

        virtual size_t writeExt(const uint8_t* data, size_t len){
            if (audio==nullptr) return 0;
            bool result = audio->write((uint8_t*)data, len);
            return result ? len : 0;
        }

        virtual size_t readExt( uint8_t *data, size_t len){
            if (audio==nullptr) return 0;
            bool result = audio->read(data, len);
            return result ? len : 0;
        }
};

}
#endif