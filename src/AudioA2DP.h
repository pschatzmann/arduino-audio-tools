/**
 * @file AudioA2DP.h
 * @author Phil Schatzmann
 * @brief A2DP Support via Arduino Streams
 * @copyright GPLv3
 * 
 */
#pragma once

#include "AudioTools.h"

#ifdef USE_A2DP

#include "BluetoothA2DPSink.h"
#include "BluetoothA2DPSource.h"
#include "AudioESP8266.h"

namespace audio_tools {

/**
 * @brief Covnerts the data from T src[][2] to a Channels array 
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
        void convert(T src[][2], Channels* channels, size_t size) {
            for (int i=size; i>0; i--) {
                channels[i].channel1 = (*convert_ptr)(src[i][0]);
                channels[i].channel2 = (*convert_ptr)(src[i][1]);
            }
        }

    private:
        int16_t (*convert_ptr)(T from);

};

AudioOutputWithCallback a2dp_stream_out(A2DP_BUFFER_SIZE, A2DP_BUFFER_COUNT);

// callback used by A2DP to provide the sound data
int32_t a2dp_stream_source_sound_data(Channels* data, int32_t len) {
    // the data in the file must be in int16 with 2 channels 
    size_t result_len_bytes = a2dp_stream_out.read(data, len);
    // result is in number of frames
    int32_t result_len = result_len_bytes / sizeof(Channels);
    ESP_LOGD("get_sound_data", "%d -> %d", result_len );
    return result_len;
}

// callback used by A2DP to write the sound data
void a2dp_stream_sink_sound_data(const uint8_t* data, uint32_t len) {
    a2dp_stream_out.write(data, len);
}


/**
 * @brief Stream support for A2DP: When calling write we start a a2dp_source - when calling read we start a a2dp_sink
 * The data is in int16_t with 2 channels at 44100 hertz. 
 *
 * Because we support only one instance the class is implemented as singleton!
 */
class A2DPStream : public BufferedStream {
    public:

        /// Just defines the bluetooth name
        void begin(char* name) {
            this->name = name;
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
            return a2dp_source;
        }

        /// provides access to the BluetoothA2DPSink
        BluetoothA2DPSink &sink(){
            return a2dp_sink;
        }

    protected:
        BluetoothA2DPSource a2dp_source;
        BluetoothA2DPSink a2dp_sink;
        bool is_setup = false;
        char* name = nullptr;

        A2DPStream() : BufferedStream(A2DP_BUFFER_SIZE){
        }

        virtual size_t writeExt(const uint8_t* data, size_t len) {    
            if (name==nullptr) return 0;
            if (!is_setup){
                a2dp_source.start(name, a2dp_stream_source_sound_data);  
                is_setup = true;
            } 
            return a2dp_stream_out.write(data, len);
        }

        virtual size_t readExt( uint8_t *data, size_t len) { 
            if (name==nullptr) return 0;
            if (!is_setup){
                a2dp_sink.set_stream_reader(&a2dp_stream_sink_sound_data, false);
                a2dp_sink.start(name);
                is_setup = true;
            }
            return a2dp_stream_out.readBytes(data, len);
        }
};

}

#endif
