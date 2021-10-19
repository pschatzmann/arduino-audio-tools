#pragma once
#include "AudioConfig.h"
#include "AudioTools/AudioTypes.h"
#include "AudioTools/AudioLogger.h"
#include "AudioTimer/AudioTimer.h"

namespace audio_tools {

const char* UNDERFLOW = "data underflow";

/**
 * @brief CallbackAudioStream Configuration 
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
struct CallbackAudioStreamInfo : public AudioBaseInfo {
    RxTxMode rx_tx_mode = RX_MODE;
    uint16_t buffer_size = DEFAULT_BUFFER_SIZE;
    bool use_timer = true;
    int timer_id = 0;
    bool secure_timer = false;
};

/**
 * @brief Callback driven Audio Source (rx_tx_mode==RX_MODE) or Audio Sink (rx_tx_mode==TX_MODE). This class
 * allows to to integrate external libraries in order to consume or generate a data stream.
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
class CallbackAudioStream : public BufferedStream {
    public:
        CallbackAudioStream():BufferedStream(256) {
        }

        ~CallbackAudioStream(){
            if (timer!=nullptr) delete timer;
            if (buffer!=nullptr) delete buffer;
            if (frame!=nullptr) delete[] frame;
        }

        CallbackAudioStreamInfo defaultConfig() {
            CallbackAudioStreamInfo def;
            return def;
        }

        void begin(CallbackAudioStreamInfo cfg, uint16_t (*frameCB)(uint8_t *data, uint16_t len)){
            this->cfg = cfg; 
            this->frameCallback = frameCB;
            if (cfg.use_timer){
                frameSize = cfg.bits_per_sample * cfg.channels / 8;
                frame = new uint8_t[frameSize];
                timer = new TimerAlarmRepeating(cfg.secure_timer, cfg.timer_id);
                buffer = new RingBuffer<uint8_t>(cfg.buffer_size);
                uint32_t time = AudioUtils::toTimeUs(cfg.sample_rate);
                timer->setCallbackParameter(this);
                timer->begin(timerCallback, time, TimeUnit::US);
            }
            active = true;
        }

        void end() {
            if (cfg.use_timer){
                timer->end();
            }
            active = false;
        }

        // relevant only if use_timer == true
        static void timerCallback(void* obj){
            CallbackAudioStream *src = (CallbackAudioStream*)obj;
            if (src->cfg.rx_tx_mode==RX_MODE){
                // input
                uint16_t available_bytes = src->frameCallback(src->frame, src->frameSize);
                if (src->buffer->writeArray(src->frame, available_bytes)!=available_bytes){
                    LOGE(UNDERFLOW);
                }
            } else {
                // output
                uint16_t available_bytes = src->buffer->readArray(src->frame, src->frameSize);
                if (available_bytes != src->frameCallback(src->frame, available_bytes)){
                    LOGE(UNDERFLOW);
                }
            }
        }

    protected:
        CallbackAudioStreamInfo cfg;
        bool active=false;
        uint16_t (*frameCallback)(uint8_t *data, uint16_t len);
        // below only relevant with timer
        TimerAlarmRepeating *timer = nullptr;
        RingBuffer<uint8_t> *buffer = nullptr;
        uint8_t *frame=nullptr;
        uint16_t frameSize;

        // used for audio sink
        virtual size_t writeExt(const uint8_t* data, size_t len) {
            if (!active) return 0;

            if (!cfg.use_timer){
                return frameCallback((uint8_t*)data, len);
            } else {
                return buffer->writeArray((uint8_t* )data, len);
            }
        }

        // used for audio source
        virtual size_t readExt( uint8_t *data, size_t len) {
            if (!active) return 0;

            if (!cfg.use_timer){
                return frameCallback(data, len);
            } else {
                return buffer->readArray(data, len);
            }
        }
       
};

} // namespace