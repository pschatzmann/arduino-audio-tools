/**
 * @file AudioA2DP.h
 * @author Phil Schatzmann
 * @brief A2DP Support via Arduino Streams
 * @copyright GPLv3
 * 
 */
#pragma once

#include "AudioConfig.h"

#include "AudioTools.h"
#include "BluetoothA2DPSink.h"
#include "BluetoothA2DPSource.h"
#include "AudioTools/AudioStreams.h"

namespace audio_tools {

/**
 * @brief Covnerts the data from T src[][2] to a Frame array 
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 * @tparam T 
 */

template<typename T>
class A2DPChannelConverter {
    public:
        A2DPChannelConverter( int16_t (*convert_ptr)(T from)){
            this->convert_ptr = convert_ptr;
        }

        // The data is provided as int24_t tgt[][2] but  returned as int24_t
        void convert(T src[][2], Frame* channels, size_t size) {
            for (int i=size; i>0; i--) {
                channels[i].channel1 = (*convert_ptr)(src[i][0]);
                channels[i].channel2 = (*convert_ptr)(src[i][1]);
            }
        }

    protected:
        int16_t (*convert_ptr)(T from);

};

class A2DPStream;
static A2DPStream *A2DPStream_self=nullptr;
// buffer which is used to exchange data
static RingBuffer<uint8_t> *a2dp_buffer = nullptr;
// flag to indicated that we are ready to process data
static bool is_a2dp_active = false;

int32_t a2dp_stream_source_sound_data(Frame* data, int32_t len);
void a2dp_stream_sink_sound_data(const uint8_t* data, uint32_t len);

enum A2DPStartLogic {StartWhenBufferFull, StartOnConnect};
enum A2DPNoData {A2DPSilence, A2DPWhoosh};

/**
 * @brief Configuration for A2DPStream
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class A2DPConfig {
    public:
        A2DPStartLogic startLogic = StartWhenBufferFull;
        A2DPNoData noData = A2DPSilence;
        RxTxMode mode = RX_MODE;
        const char* name = "A2DP"; 
        bool auto_reconnect = false;
        int bufferSize = A2DP_BUFFER_SIZE * A2DP_BUFFER_COUNT;
};


/**
 * @brief Stream support for A2DP: begin(TX_MODE) uses a2dp_source - begin(RX_MODE) a a2dp_sink
 * The data is in int16_t with 2 channels at 44100 hertz. 
 * We support only one instance of the class!
 * @ingroup io
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class A2DPStream : public AudioStream {

    public:
        A2DPStream() {
            TRACED();
            xSemaphore = xSemaphoreCreateMutex();
            // A2DPStream can only be used once
            assert(A2DPStream_self==nullptr);
            A2DPStream_self = this;
        }

        /// Release the allocate a2dp_source or a2dp_sink
        ~A2DPStream(){
            TRACED();
            if (a2dp_source!=nullptr) delete a2dp_source;
            if (a2dp_sink!=nullptr) delete a2dp_sink;      
            A2DPStream_self = nullptr;      
        }

        A2DPConfig defaultConfig(RxTxMode mode=RX_MODE){
            A2DPConfig cfg;
            cfg.mode = mode;
            if(mode=TX_MODE){
                cfg.name="[Unknown]";
            }
            return cfg;
        }

        /// provides access to the 
        BluetoothA2DPSource &source() {
            if (a2dp_source==nullptr){
                a2dp = a2dp_source = new BluetoothA2DPSource();
            }
            return *a2dp_source;
        }

        /// provides access to the BluetoothA2DPSink
        BluetoothA2DPSink &sink(){
            if (a2dp_sink==nullptr){
                a2dp = a2dp_sink = new BluetoothA2DPSink();
            }
            return *a2dp_sink;
        }

        void begin(RxTxMode mode, const char* name){
            A2DPConfig cfg;
            cfg.mode = mode;
            cfg.name = name;
            begin(cfg);
        }

        /// Opens the processing
        void begin(A2DPConfig cfg){
            this->config = cfg;
            LOGI("Connecting to %s",cfg.name);
            if (a2dp_buffer==nullptr){
                a2dp_buffer = new RingBuffer<uint8_t>(cfg.bufferSize);
            }

            switch (cfg.mode){
                case TX_MODE:
                    LOGI("Starting a2dp_source...");
                    source(); // allocate object
                    a2dp_source->set_auto_reconnect(cfg.auto_reconnect);
                    a2dp_source->set_volume(volume * 100);
                    if(cfg.name=="[Unknown]"){
                        //search next available device
                        a2dp_source->set_ssid_callback(detectedDevice);
                    }
                    a2dp_source->set_on_connection_state_changed(a2dpStateCallback, this);
                    a2dp_source->start((char*)cfg.name, a2dp_stream_source_sound_data);  
                    while(!a2dp_source->is_connected()){
                        LOGD("waiting for connection");
                        delay(1000);
                    }
                    LOGI("a2dp_source is connected...");
                    notifyBaseInfo(44100);
                    //is_a2dp_active = true;
                    break;

                case RX_MODE:
                    LOGI("Starting a2dp_sink...");
                    sink(); // allocate object
                    a2dp_sink->set_auto_reconnect(cfg.auto_reconnect);
                    a2dp_sink->set_stream_reader(&a2dp_stream_sink_sound_data, false);
                    a2dp_sink->set_volume(volume * 100);
                    a2dp_sink->set_on_connection_state_changed(a2dpStateCallback, this);
                    a2dp_sink->set_sample_rate_callback(sample_rate_callback);
                    a2dp_sink->start((char*)cfg.name);
                    while(!a2dp_sink->is_connected()){
                        LOGD("waiting for connection");
                        delay(1000);
                    }
                    LOGI("a2dp_sink is connected...");
                    is_a2dp_active = true;
                    break;
            }            
        }

        /// checks if we are connected
        bool isConnected() {
            if (a2dp_source==nullptr && a2dp_sink==nullptr) return false;
            if (a2dp_source!=nullptr) return a2dp_source->is_connected();
            return a2dp_sink->is_connected();
        }

        /// is ready to process data
        bool isReady() {
            return is_a2dp_active;
        }

        /// convert to bool
        operator bool() {
            return isReady();
        }

        /// Writes the data into a temporary send buffer - where it can be picked up by the callback
        virtual size_t write(const uint8_t* data, size_t len) {   
            if (a2dp_buffer==nullptr) return 0;
             LOGD("%s: %zu", LOG_METHOD, len);

            // blocking write - we wait for space in buffer
            bool isBufferFull = true;
            while(isBufferFull){
                lockSemaphore(true);
                isBufferFull = a2dp_buffer->availableForWrite()<len;
                lockSemaphore(false);

               // we wait until the buffer is full
               if (isBufferFull){
                    if (!is_a2dp_active){
                        is_a2dp_active = true;
                        LOGW("is_a2dp_active -> true with %d bytes", a2dp_buffer->available());
                    }
                    delay(100);
                    LOGD("Waiting for free buffer space - available: %d", a2dp_buffer->available());
                } 
            }

            // write to buffer
            lockSemaphore(true);
            size_t result = a2dp_buffer->writeArray(data, len);
            lockSemaphore(false);
            LOGD("write %d -> %d", len, result);
            return result;
        }

        virtual size_t write(uint8_t c) {
            LOGE( "write(char) not supported");
            assert(false);
            return -1;
        }

        virtual void flush() {
        }

        /// Reads the data from the temporary buffer
        virtual size_t readBytes(uint8_t *data, size_t len) { 
            if (a2dp_buffer==nullptr) return 0;
            size_t result = 0; 
            if (is_a2dp_active){
                LOGD("readBytes %d", len);

                result = a2dp_buffer->readArray(data, len);
                LOGI("readBytes %d->%d", len,result);
            } else {
                LOGW( "readBytes failed because !is_a2dp_active");
            }
            return result;
        }

        // not supported
        virtual int read() {
            LOGE( "read() not supported");
            return -1;
        }
        // not supported
        virtual int peek() {
            LOGE( "peek() not supported");
            return -1;
        }
       
        virtual int available() {
            // only supported in tx mode
            if (config.mode!=RX_MODE || a2dp_buffer==nullptr ) return 0;
            return a2dp_buffer->available();
        }

        virtual int availableForWrite() {
            // only supported in tx mode
            if (config.mode!=TX_MODE || a2dp_buffer==nullptr ) return 0;
            // return infor from buffer
            return a2dp_buffer->availableForWrite();
        }

        // Define the volme (values between 0.0 and 1.0)
        virtual void setVolume(float volume){
            this->volume = volume;
            // 128 is max volume
            if (a2dp!=nullptr) a2dp->set_volume(volume * 128);
        }

        virtual void setNotifyAudioChange (AudioBaseInfoDependent &bi) {
            audioBaseInfoDependent = &bi;
        }


    protected:
        A2DPConfig config;
        BluetoothA2DPSource *a2dp_source = nullptr;
        BluetoothA2DPSink *a2dp_sink = nullptr;
        BluetoothA2DPCommon *a2dp=nullptr;
        AudioBaseInfoDependent *audioBaseInfoDependent=nullptr;
        float volume = 1.0;
        // semaphore to synchronize acess to the buffer
        SemaphoreHandle_t xSemaphore = NULL;

        // auto-detect device to send audio to (TX-Mode)
        static bool detectedDevice(const char* ssid, esp_bd_addr_t address, int rssi){
            LOGW("found Device: %s rssi: %d", ssid, rssi);
            //filter out weak signals
            return (rssi > -75);
        }
    
        static void a2dpStateCallback(esp_a2d_connection_state_t state, void *caller){
            TRACED();
            A2DPStream *self = (A2DPStream*)caller;
            if (state==ESP_A2D_CONNECTION_STATE_CONNECTED && self->config.startLogic==StartOnConnect){
                 is_a2dp_active = true;
            } 
            LOGW("==> state: %s", self->a2dp->to_str(state));
        }

        bool lockSemaphore(bool locked, bool immediate=false){
            bool result = false;
            if (locked){
                result = xSemaphoreTake( xSemaphore, immediate ? 10 : portMAX_DELAY ) == pdTRUE;     
            } else {
                result = xSemaphoreGive( xSemaphore ) == pdTRUE;
            }
            return result;    
        }

        // callback used by A2DP to provide the a2dp_source sound data
        static int32_t a2dp_stream_source_sound_data(Frame* data, int32_t len) {
            if (a2dp_buffer==nullptr) return 0;
            int32_t result_len = 0;

            bool isAvailable = false;
            bool isLocked = false;
            if (is_a2dp_active) {
                if (A2DPStream_self->lockSemaphore(true)) {
                    isAvailable = a2dp_buffer->available()>0;
                    LOGD("buffer: %d, free %d",a2dp_buffer->available(), a2dp_buffer->availableForWrite() )
                    isLocked = true;
                } else {
                    LOGE("lock failed");
                }
            }

            // at first call we start with some empty data
            if (isAvailable){
                // the data in the file must be in int16 with 2 channels 
                size_t result_len_bytes = a2dp_buffer->readArray((uint8_t*)data, len*sizeof(Frame));
                A2DPStream_self->lockSemaphore(false);

                // result is in number of frames
                result_len = result_len_bytes / sizeof(Frame);
            } else {
                if (isLocked) A2DPStream_self->lockSemaphore(false);

                // prevent underflow on first call
                switch (A2DPStream_self->config.noData) {
                    case A2DPSilence:
                        memset(data, 0, len*sizeof(Frame));
                        break;
                    case A2DPWhoosh:
                        for (int j=0;j<len;j++){
                            data[j].channel2 = data[j].channel1 = (rand() % 50) - 25; 
                        }
                        break;
                }
                result_len = len;
                delay(3);

                // Priority: 22  on core 0
                // LOGI("Priority: %d  on core %d", uxTaskPriorityGet(NULL), xPortGetCoreID());

            }

            LOGD("a2dp_stream_source_sound_data: %d -> %d (%s)", len, result_len, isAvailable?"+":"-");   
            return result_len;
        }

        /// callback used by A2DP to write the sound data
        static void a2dp_stream_sink_sound_data(const uint8_t* data, uint32_t len) {
            if (is_a2dp_active && a2dp_buffer!=nullptr){
                uint32_t result_len = a2dp_buffer->writeArray(data, len);
                LOGD("a2dp_stream_sink_sound_data %d -> %d", len, result_len);
                // allow some other task 
                //yield();
            } 
        }

        /// notify subscriber with AudioBaseInfo
        void notifyBaseInfo(int rate){
            if (audioBaseInfoDependent!=nullptr){
                AudioBaseInfo info;
                info.channels = 2;
                info.bits_per_sample = 16;
                info.sample_rate = rate;
                audioBaseInfoDependent->setAudioInfo(info);
            }
        }

        /// callback to update audio info with used a2dp sample rate
        static void sample_rate_callback(uint16_t rate) {
            if (A2DPStream_self->audioBaseInfoDependent!=nullptr){
                A2DPStream_self->notifyBaseInfo(rate);
            }
        }

};

}
