/**
 * @file A2DPStream.h
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
#include "Concurrency/BufferRTOS.h"


namespace audio_tools {

class A2DPStream;
static A2DPStream *A2DPStream_self=nullptr;
// buffer which is used to exchange data
static BufferRTOS<uint8_t>a2dp_buffer{0, A2DP_BUFFER_SIZE, portMAX_DELAY, portMAX_DELAY};
// flag to indicated that we are ready to process data
static bool is_a2dp_active = false;

int32_t a2dp_stream_source_sound_data(Frame* data, int32_t len);
void a2dp_stream_sink_sound_data(const uint8_t* data, uint32_t len);

/// A2DP Startup Logic
enum A2DPStartLogic {StartWhenBufferFull, StartOnConnect};
/// A2DP Action when there is no data
enum A2DPNoData {A2DPSilence, A2DPWhoosh};

/**
 * @brief Configuration for A2DPStream
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class A2DPConfig {
    public:
        /// Logic when the processing is activated
        A2DPStartLogic startup_logic = StartWhenBufferFull;
        /// Action when a2dp is not active yet
        A2DPNoData startup_nodata = A2DPSilence;
        RxTxMode mode = RX_MODE;
        /// A2DP name
        const char* name = "A2DP"; 
        bool auto_reconnect = false;
        int buffer_size = A2DP_BUFFER_SIZE * A2DP_BUFFER_COUNT;
        /// Delay in ms which is added to each write
        int delay_ms = 1;
        /// when a2dp source is active but has no data we generate silence data
        bool silence_on_nodata = false;
};


/**
 * @brief Stream support for A2DP using https://github.com/pschatzmann/ESP32-A2DP: 
 * begin(TX_MODE) opens a a2dp_source and begin(RX_MODE) a a2dp_sink.
 * The data is in int16_t with 2 channels at 44100 hertz. 
 * We support only one instance of the class!
 * Please note that this is a conveniance class that supports the stream api,
 * however this is rather inefficient, beause quite a bit buffer needs to be allocated.
 * It is recommended to use the API with the callbacks. Examples can be found in the examples-basic-api
 * directory.
 * 
 * Requires: https://github.com/pschatzmann/ESP32-A2DP
 *
 * @ingroup io
 * @ingroup communications
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class A2DPStream : public AudioStream, public VolumeSupport {

    public:
        A2DPStream() {
            TRACED();
            // A2DPStream can only be used once
            assert(A2DPStream_self==nullptr);
            A2DPStream_self = this;
            info.bits_per_sample = 16;
            info.sample_rate = 44100;
            info.channels = 2;
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
            if(mode==TX_MODE){
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

        /// Starts the processing
        bool begin(RxTxMode mode, const char* name){
            A2DPConfig cfg;
            cfg.mode = mode;
            cfg.name = name;
            return begin(cfg);
        }

        /// Starts the processing
        bool begin(A2DPConfig cfg){
            this->config = cfg;
            bool result = false;
            LOGI("Connecting to %s",cfg.name);
            
            if (!a2dp_buffer.resize(cfg.buffer_size)){
                LOGE("a2dp_buffer resize failed");
                return false;
            }

            // initialize a2dp_silence_timeout
            if (config.silence_on_nodata){
                LOGI("Using StartOnConnect")
                config.startup_logic = StartOnConnect;
            }

            switch (cfg.mode){
                case TX_MODE:
                    LOGI("Starting a2dp_source...");
                    source(); // allocate object
                    a2dp_source->set_auto_reconnect(cfg.auto_reconnect);
                    a2dp_source->set_volume(volume() * A2DP_MAX_VOL);
                    if(Str(cfg.name).equals("[Unknown]")){
                        //search next available device
                        a2dp_source->set_ssid_callback(detected_device);
                    }
                    a2dp_source->set_on_connection_state_changed(a2dp_state_callback, this);
                    a2dp_source->start_raw((char*)cfg.name, a2dp_stream_source_sound_data);  
                    while(!a2dp_source->is_connected()){
                        LOGD("waiting for connection");
                        delay(1000);
                    }
                    LOGI("a2dp_source is connected...");
                    notify_base_Info(44100);
                    //is_a2dp_active = true;
                    result = true;
                    break;

                case RX_MODE:
                    LOGI("Starting a2dp_sink...");
                    sink(); // allocate object
                    a2dp_sink->set_auto_reconnect(cfg.auto_reconnect);
                    a2dp_sink->set_stream_reader(&a2dp_stream_sink_sound_data, false);
                    a2dp_sink->set_volume(volume() * A2DP_MAX_VOL);
                    a2dp_sink->set_on_connection_state_changed(a2dp_state_callback, this);
                    a2dp_sink->set_sample_rate_callback(sample_rate_callback);
                    a2dp_sink->start((char*)cfg.name);
                    while(!a2dp_sink->is_connected()){
                        LOGD("waiting for connection");
                        delay(1000);
                    }
                    LOGI("a2dp_sink is connected...");
                    is_a2dp_active = true;
                    result = true;
                    break;
                default:
                    LOGE("Undefined mode: %d", cfg.mode);
                    break;
            } 
            
            return result;           
        }

        void end() override {
            if (a2dp != nullptr) {
                a2dp->disconnect();
            }
            AudioStream::end();
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
        size_t write(const uint8_t* data, size_t len) override {   
            LOGD("%s: %zu", LOG_METHOD, len);
            if (config.mode == TX_MODE){
                // at 80% we activate the processing
                if(!is_a2dp_active 
                && config.startup_logic == StartWhenBufferFull
                && a2dp_buffer.available() >= 0.8f * a2dp_buffer.size()){
                    LOGI("set active");
                    is_a2dp_active = true;
                }

                // blocking write: if buffer is full we wait
                size_t free = a2dp_buffer.availableForWrite();
                while(len > free){
                    LOGD("Waiting for buffer: writing %d > available %d", (int) len, (int) free);
                    delay(5);
                    free = a2dp_buffer.availableForWrite();
                }
            }

            // write to buffer
            size_t result = a2dp_buffer.writeArray(data, len);
            LOGD("write %d -> %d", len, result);
            if (config.mode == TX_MODE){
                // give the callback a chance to retrieve the data
                delay(config.delay_ms);
            }
            return result;
        }

        /// Reads the data from the temporary buffer
        size_t readBytes(uint8_t *data, size_t len) override { 
            if (!is_a2dp_active){
                LOGW( "readBytes failed because !is_a2dp_active");
                return 0;
            }
            LOGD("readBytes %d", len);
            size_t result = a2dp_buffer.readArray(data, len);
            LOGI("readBytes %d->%d", len,result);
            return result;
        }
       
        int available() override {
            // only supported in tx mode
            if (config.mode!=RX_MODE) return 0;
            return a2dp_buffer.available();
        }

        int availableForWrite() override {
            // only supported in tx mode
            if (config.mode!=TX_MODE ) return 0;
            // return infor from buffer
            return a2dp_buffer.availableForWrite();
        }

        // Define the volme (values between 0.0 and 1.0)
        bool setVolume(float volume) override {
            VolumeSupport::setVolume(volume);
            // 128 is max volume
            if (a2dp!=nullptr) a2dp->set_volume(volume * A2DP_MAX_VOL);
            return true;
        }

    protected:
        A2DPConfig config;
        BluetoothA2DPSource *a2dp_source = nullptr;
        BluetoothA2DPSink *a2dp_sink = nullptr;
        BluetoothA2DPCommon *a2dp=nullptr;
        const int A2DP_MAX_VOL = 128;

        // auto-detect device to send audio to (TX-Mode)
        static bool detected_device(const char* ssid, esp_bd_addr_t address, int rssi){
            LOGW("found Device: %s rssi: %d", ssid, rssi);
            //filter out weak signals
            return (rssi > -75);
        }
    
        static void a2dp_state_callback(esp_a2d_connection_state_t state, void *caller){
            TRACED();
            A2DPStream *self = (A2DPStream*)caller;
            if (state==ESP_A2D_CONNECTION_STATE_CONNECTED && self->config.startup_logic==StartOnConnect){
                 is_a2dp_active = true;
            } 
            LOGW("==> state: %s", self->a2dp->to_str(state));
        }


        // callback used by A2DP to provide the a2dp_source sound data
        static int32_t a2dp_stream_source_sound_data(uint8_t* data, int32_t len) {
            int32_t result_len = 0;
            A2DPConfig config = A2DPStream_self->config;

            // at first call we start with some empty data
            if (is_a2dp_active){
                // the data in the file must be in int16 with 2 channels 
                yield();
                result_len = a2dp_buffer.readArray((uint8_t*)data, len);

                // provide silence data
                if (config.silence_on_nodata && result_len == 0){
                    memset(data,0, len);
                    result_len = len;
                }
            } else {

                // prevent underflow on first call
                switch (config.startup_nodata) {
                    case A2DPSilence:
                        memset(data, 0, len);
                        break;
                    case A2DPWhoosh:
                        int16_t *data16 = (int16_t*)data;
                        for (int j=0;j<len/4;j+=2){
                            data16[j+1] = data16[j] = (rand() % 50) - 25; 
                        }
                        break;
                }
                result_len = len;

                // Priority: 22  on core 0
                // LOGI("Priority: %d  on core %d", uxTaskPriorityGet(NULL), xPortGetCoreID());

            }
            LOGD("a2dp_stream_source_sound_data: %d -> %d", len, result_len);   
            return result_len;
        }

        /// callback used by A2DP to write the sound data
        static void a2dp_stream_sink_sound_data(const uint8_t* data, uint32_t len) {
            if (is_a2dp_active){
                uint32_t result_len = a2dp_buffer.writeArray(data, len);
                LOGD("a2dp_stream_sink_sound_data %d -> %d", len, result_len);
            } 
        }

        /// notify subscriber with AudioInfo
        void notify_base_Info(int rate){
            AudioInfo info;
            info.channels = 2;
            info.bits_per_sample = 16;
            info.sample_rate = rate;
            notifyAudioChange(info);            
        }

        /// callback to update audio info with used a2dp sample rate
        static void sample_rate_callback(uint16_t rate) {
            A2DPStream_self->info.sample_rate = rate;
            A2DPStream_self->notify_base_Info(rate);
        }

};

}
