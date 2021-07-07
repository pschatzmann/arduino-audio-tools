#pragma once

#include "mozzi_config.h"
#include "hardware_defines.h"
#include "mozzi_analog.h"
#include "Mozzi.h"

namespace audio_tools {

/**
 * @brief Mozzi Configuration for input or output stream
 * 
 */
struct MozziConfig : AudioBaseInfo {
    uint16_t control_rate=0;
    void (*updateControl)() = nullptr; //&::updateControl;
    AudioOutput_t (*updateAudio)() = nullptr; // = &::updateAudio;

    MozziConfig(){
        channels = AUDIO_CHANNELS;
        sample_rate = AUDIO_RATE;
        bits_per_sample = sizeof(AudioOutputStorage_t)*8;
    }
};
/**
 * @brief Support for  https://sensorium.github.io/Mozzi/ 
 * Define your updateControl() method.
 * Define your updateAudio() method.
 * Start by calling begin(control_rate)
 * do not call audioHook(); in the loop !

 * @author Phil Schatzmann
 * @copyright GPLv3
 */
 
class MozziGenerator : public SoundGenerator<AudioOutputStorage_t> {
    public:
        MozziGenerator(){   
	 		LOGD(__FUNCTION__);
        }

        MozziGenerator(MozziConfig config){
            begin(config);
        }

        virtual ~MozziGenerator() {
            end();
        }

        void begin(MozziConfig config){
            info = config;
            if (info.control_rate==0){
                info.control_rate = CONTROL_RATE;
            }
            control_counter_max = info.sample_rate / info.control_rate;
            if (control_counter_max <= 0){
                control_counter_max = 1;
            }
            control_counter = control_counter_max;
        }

        /// Provides some key audio information
        MozziConfig config() {
            return info;
        }

        /// Provides a single sample
        virtual AudioOutputStorage_t readSample() {
            if (info.updateAudio==nullptr){
                LOGE("The updateAudio method has not been defined!");
                end();
                return 0;
            }

            // return prior right value from buffer
            if (is_read_buffer_filled){
                // for stereo output we might have the value already
                is_read_buffer_filled = false;
                return read_buffer;
            }

            // control update
            if (--control_counter<0){
                control_counter = control_counter_max;
                if (info.updateControl!=nullptr){
                    info.updateControl();
                }
            }

            // return left value
            AudioOutputStorage_t result = updateSample();
            return result;
        }

    protected:
        MozziConfig info;
        int control_counter_max;
        int control_counter;
        int read_buffer;
        bool is_read_buffer_filled = false;


        AudioOutputStorage_t updateSample(){
            AudioOutput out = info.updateAudio();
            // requested mono
            AudioOutputStorage_t result = 0;
            #if (AUDIO_CHANNELS == MONO)
                // generated stereo
                if (sizeof(out) == sizeof(AudioOutputStorage_t)*2){
                    result = out[0]/2 + out[1]/2;
                // generated mono
                } else  if (sizeof(out) == sizeof(AudioOutputStorage_t)){
                    result = out[0];
                }

            // requested stereo
            #elif (AUDIO_CHANNELS == STEREO)
                result = out[0];
                // generated stereo
                if (sizeof(out) == sizeof(AudioOutputStorage_t)*2){
                    read_buffer = out[1];
                    is_read_buffer_filled = true;
                // generated mono
                } else if (sizeof(out) == sizeof(AudioOutputStorage_t)){
                    read_buffer = out[0];
                    is_read_buffer_filled = true;
                }
            #endif
            return result;
        }


};



/**
 * @brief We use the output functionality of Mozzi to output audio data. We expect the data as array of int16_t with
 * one or two channels. Though we support the setting of a sample rate, we recommend to use the default sample rate
 * from Mozzi which is available with the AUDIO_RATE define.
 */
class MozziStream : public Stream {

    public:
        MozziStream(){
	 		LOGD(__FUNCTION__);
        }

        ~MozziStream(){
	 		LOGD(__FUNCTION__);
            end();
            if (input_ptr!=nullptr){
                delete input_ptr;
                input_ptr = nullptr;
            }
        }

        MozziConfig defaultConfig() {
            MozziConfig default_config;
            return default_config;
        }

        /// Starts Mozzi with its default parameters
        void begin(){
            begin(defaultConfig());
        }

        // Start Mozzi -  if controlRate > 0 we actiavate the sound generation (=>allow reads); the parameters describe the values if the
        // provided input stream or resulting output stream;
        void begin(MozziConfig cfg){
 	 		LOGD(__FUNCTION__);
            config = cfg;
            Mozzi.setAudioRate(config.sample_rate);
            // in output mode we do not allocate any unnecessary functionality
            if (input_ptr == nullptr && config.control_rate>0){
                input_ptr = new MozziGenerator(config);
            }
            if (cfg.channels != config.channels){
                LOGE("You need to change the AUDIO_CHANNELS in mozzi_config.h to %d", cfg.channels);
            }

            Mozzi.start(0);
        }

        void end(){
	 		LOGD(__FUNCTION__);
            Mozzi.stop();
        }

        /// unsupported operations
        virtual int availableForWrite() {       
            return Mozzi.canWrite() ? sizeof(int) : 0;
        }

        /// write an individual byte - if the frame is complete we pass it to Mozzi
        virtual size_t write(uint8_t c) {
            if (Mozzi.canWrite()){
                writeChar(c);
                return 1;
            } else {
                return 0;
            }
        }

        virtual size_t write(const uint8_t *buffer, size_t size) {
            for (size_t j=0;j<size;j++){
                if (write(buffer[j])<=0){
                    return j;
                }
            }
            return size;
        }

        virtual int available(){
            return 100000;  // just a random big number
        }

        virtual size_t readBytes(char *buffer, size_t length){
            return input_ptr==nullptr ? 0 : input_ptr->readBytes((uint8_t*)buffer, length);
        }

        virtual int read(){
            LOGE("read() not supported -  use readBytes!");
            return -1;            
        }

        virtual int peek(){
            LOGE("peek() not supported!");
            return -1;            
        }

        virtual void flush(){
        };

    protected:
        MozziConfig config;
        MozziGenerator *input_ptr = nullptr;
        NumberReader numberReader;
        int32_t frame[2];
        uint8_t buffer[64];
        int buffer_pos;

        // writes individual bytes and puts them together as MonoOutput or StereoOutput
        void writeChar(uint8_t c){
            buffer[buffer_pos++] = c;

            #if (AUDIO_CHANNELS == MONO)
                #warning "Mozzi is configured for Mono Output (to one channel only)"
                if (config.channels == 1 && buffer_pos==config.bits_per_sample/8){
                    numberReader.toNumbers(buffer, config.bits_per_sample,sizeof(AudioOutputStorage_t) * 8, true, 1, frame );
                    MonoOutput out = MonoOutput(frame[0]);
                    Mozzi.write(out);
                    buffer_pos = 0;
                } else if (config.channels == 2 && buffer_pos==config.bits_per_sample/8*2){
                    numberReader.toNumbers(buffer, config.bits_per_sample,sizeof(AudioOutputStorage_t) * 8, true, 2, frame );
                    MonoOutput out = MonoOutput(frame[0]/2 + frame[1]/2);
                    Mozzi.write(out);
                    buffer_pos = 0;
                }
            
            #elif (AUDIO_CHANNELS == STEREO)
                if (config.channels == 1 && buffer_pos==config.bits_per_sample/8){
                    numberReader.toNumbers(buffer, config.bits_per_sample,sizeof(AudioOutputStorage_t) * 8, true, 1, frame );
                    StereoOutput out = StereoOutput(frame[0],frame[0]);
                    Mozzi.write(out);
                    buffer_pos = 0;
                } else if (config.channels == 2 && buffer_pos==config.bits_per_sample/8*2){
                    numberReader.toNumbers(buffer, config.bits_per_sample,sizeof(AudioOutputStorage_t) * 8, true, 2, frame );
                    StereoOutput out = StereoOutput(frame[0],frame[1]);
                    Mozzi.write(out);
                    buffer_pos = 0;
                }
            
            #endif
        }

};


} // Namespace
