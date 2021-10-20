/**
 * @file AudioA2DP.h
 * @author Phil Schatzmann
 * @brief A2DP Support via Arduino Streams
 * @copyright GPLv3
 * 
 */
#pragma once

#include "AudioConfig.h"
#ifdef USE_A2DP

#include "AudioTools.h"
#include "BluetoothA2DPSink.h"
#include "BluetoothA2DPSource.h"

namespace audio_tools {

/**
 * @brief Covnerts the data from T src[][2] to a Frame array 
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 * @tparam T 
 */

template<typename T>
class ChannelConverter {
    public:
        ChannelConverter( int16_t (*convert_ptr)(T from)){
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


// buffer which is used to exchange data
RingBuffer<uint8_t> a2dp_buffer(A2DP_BUFFER_SIZE*A2DP_BUFFER_COUNT);
// flag to indicated that we are ready to process data
volatile bool is_a2dp_active = false;

// callback used by A2DP to provide the a2dp_source sound data
int32_t a2dp_stream_source_sound_data(Frame* data, int32_t len) {
    LOGD("a2dp_stream_source_sound_data: %d", len);   
    size_t result_len = 0;
    // at first call we start with some empty data
    if (!is_a2dp_active){
        LOGI("setting is_a2dp_active to true");
        // prevent underflow on first call
        for(int j=0;j<len;j++){
            a2dp_buffer.write(0);
        }
        is_a2dp_active = true;        
    }
    // the data in the file must be in int16 with 2 channels 
    size_t result_len_bytes = a2dp_buffer.readArray((uint8_t*)data, len*sizeof(Frame));

    // result is in number of frames
    result_len = result_len_bytes / sizeof(Frame);

    // Log result
    if (AudioLogger::instance().level()==AudioLogger::Debug){
        LOGD("a2dp_stream_source_sound_data %d -> %d", len, result_len);   
    } else {
        if (result_len<len){
            LOGW("a2dp_stream_source_sound_data underflow: %d -> %d ", len, result_len);   
        }
    }

    // allow some other task 
    //yield();

    return result_len;
}

// callback used by A2DP to write the sound data
void a2dp_stream_sink_sound_data(const uint8_t* data, uint32_t len) {
    if (is_a2dp_active){
        uint32_t result_len = a2dp_buffer.writeArray(data, len);
        LOGI("a2dp_stream_sink_sound_data %d -> %d", len, result_len);
        // allow some other task 
        yield();
    }
}


/**
 * @brief Stream support for A2DP: begin(TX_MODE) uses a2dp_source - begin(RX_MODE) a a2dp_sink
 * The data is in int16_t with 2 channels at 44100 hertz. 
 *
 * Because we support only one instance the class is implemented as singleton!
 */
class A2DPStream : public Stream {
    public:
        // Release the allocate a2dp_source or a2dp_sink
        ~A2DPStream(){
            if (a2dp_source!=nullptr) delete a2dp_source;
            if (a2dp_sink!=nullptr) delete a2dp_sink;
        }

        /// Provides the A2DPStream singleton instance
        static A2DPStream &instance() {
            static A2DPStream *ptr;
            if (ptr==nullptr){
                ptr = new A2DPStream();
            }
            return *ptr;
        }

        /// provides access to the 
        BluetoothA2DPSource &source() {
            if (a2dp_source==nullptr){
                a2dp_source = new BluetoothA2DPSource();
            }
            return *a2dp_source;
        }

        /// provides access to the BluetoothA2DPSink
        BluetoothA2DPSink &sink(){
            if (a2dp_sink==nullptr){
                a2dp_sink = new BluetoothA2DPSink();
            }
            return *a2dp_sink;
        }

        /// Opens the processing
        void begin(RxTxMode mode, const char* name){
            this->mode = mode;
            this->name = name;

           switch (mode){
                case TX_MODE:
                    LOGI("Starting a2dp_source...");
                    source(); // allocate object
                    a2dp_source->start((char*)name, a2dp_stream_source_sound_data);  
                    while(!a2dp_source->isConnected()){
                        yield();
                    }
                    LOGI("a2dp_source is connected...");

                    //is_a2dp_active done in callback to prevent a buffer overflow
                    break;

                case RX_MODE:
                    LOGI("Starting a2dp_sink...");
                    sink(); // allocate object
                    a2dp_sink->set_stream_reader(&a2dp_stream_sink_sound_data, false);
                    a2dp_sink->start((char*)name);
                    while(!a2dp_sink->isConnected()){
                        yield();
                    }
                    LOGI("a2dp_sink is connected...");
                    is_a2dp_active = true;
                    break;
            }
        }

        /// checks if we are connected
        bool isConnected() {
            if (a2dp_source==nullptr && a2dp_sink==nullptr) return false;
            if (a2dp_source!=nullptr) return a2dp_source->isConnected();
            return a2dp_sink->isConnected();
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
            size_t result = 0; 
            if (is_a2dp_active){
                LOGD("write %d", len);
                yield();
                result = a2dp_buffer.writeArray(data,len);
                LOGI("write %d -> %d", len,result);
            } else {
                LOGW( "write failed because !is_a2dp_active");
            }
            return result;
        }

        virtual size_t write(uint8_t c) {
            LOGE( "write(char) not supported");
            return -1;
        }

        virtual void flush() {
        }

        /// Reads the data from the temporary buffer
        virtual size_t readBytes(uint8_t *data, size_t len) { 
            size_t result = 0; 
            if (is_a2dp_active){
                LOGD("readBytes %d", len);

                result = a2dp_buffer.readArray(data, len);
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
            return a2dp_buffer.available();
        }

        virtual int availableToWrite() {
            return a2dp_buffer.availableToWrite();
        }

    protected:
        BluetoothA2DPSource *a2dp_source = nullptr;
        BluetoothA2DPSink *a2dp_sink = nullptr;
        RxTxMode mode;
        const char* name = nullptr;

        A2DPStream() {
            LOGI("A2DPStream");
        }

};

}

#endif
