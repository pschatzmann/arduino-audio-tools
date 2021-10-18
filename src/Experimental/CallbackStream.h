#pragma once
#include "AudioConfig.h"
#include "AudioTools/AudioTypes.h"

/**
 * @brief CallbackAudioStream Configuration 
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
struct CallbackInfo : public AudioBaseInfo {
    RxTxMode rx_tx_mode = RX_MODE;
    bool use_timer = true;
    uint16_t buffer_size = DEFAULT_BUFFER_SIZE;
};

/**
 * @brief Callback driven Audio Source (rx_tx_mode==RX_MODE) or Audio Sink (rx_tx_mode==TX_MODE)
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
class CallbackAudioStream : public BufferedStream {
    public:
        CallbackAudioStream(){
        }

        ~CallbackAudioStream(){
            if (timer!=nullptr) delete timer;
            if (buffer!=nullptr) delete buffer;
            if (frame!=nullptr) delete[] frame;
        }

        CallbackInfo defaultConfig() {
            CallbackInfo def;
            return def;
        }

        void begin(CallbackInfo cfg, void (*frameCallback(uint8_t *data, uint16_t len))){
            this->cfg = cfg; 
            this->is_input = input;

            if (cfg.use_timer){
                frameSize = cfg.bits_per_sample * channels / 8;
                frame = new uint8_t[frameSize];
                timer = new TimerAlarmRepearing();
                buffer = new RingBuffer<uint8_t>(cfg.buffer_size);
                uint32_t time, AudioUtils.toTimeUs(cfg.sampling_rate);
                timer->setObject(this);
                timer->begin(timerCallback, timer, TimeUnit::US);
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
            CallbackAudioSource *src = (CallbackAudioSource*)obj;
            if (src->is_input){
                // input
                uint16_t available_bytes = frameCallback(obj->frame, obj->frameSize);
                if (src->buffer->write(obj->frame, available_bytes)!=available_bytes){
                    LOGE(UNDERFLOW);
                }
            } else {
                // output
                uint16_t available_bytes = src->buffer->readArray(obj->frame, obj->frameSize);
                if (available_bytes!=frameCallback(obj->frame, available_bytes)){
                    LOGE(UNDERFLOW);
                }
            }
        }

    protected:
        AudioBaseInfo cfg;
        bool active=false;
        uint16_t (*frameCallback(uint8_t *data, uint16_t len);
        // below only relevant with timer
        TimerAlarmRepeating *timer = nullptr;
        RingBuffer<uint8_t> *buffer = nullptr;
        uint8_t *frame=nullptr;
        uint16_t frameSize;
        const char* UNDERFLOW = "data underflow";

        // used for audio sink
        virtual size_t writeExt(const uint8_t* data, size_t len) {
            if (!active) return 0;

            if (!cfg.use_timer){
                return frameCallback(data, len);
            } else {
                return buffer->write(data, len);
            }
        }

        // used for audio source
        virtual size_t readExt( uint8_t *data, size_t len) {
            if (!active) return 0;

            if (!cfg.use_timer){
                return frameCallback(data, len);
            } else {
                return buffer->read(data, len);
            }
        }
       
};