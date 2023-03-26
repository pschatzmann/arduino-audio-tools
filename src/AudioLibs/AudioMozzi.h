#pragma once

#include "AudioConfig.h"
#include "mozzi_config.h"
#include "hardware_defines.h"
#include "mozzi_analog.h"
#include "Mozzi.h"
#include "AudioTools/AudioStreams.h"

namespace audio_tools {

/**
 * @brief Mozzi Configuration for input or output stream
 * 
 */
struct MozziConfig : AudioInfo {
    uint16_t control_rate=CONTROL_RATE;
    void (*updateControl)() = nullptr; //&::updateControl;
    AudioOutput_t (*updateAudio)() = nullptr; // = &::updateAudio;

    MozziConfig(){
        channels = AUDIO_CHANNELS;
        sample_rate = AUDIO_RATE;
        bits_per_sample = 16;
    }
};
/**
 * @brief Support for  https://sensorium.github.io/Mozzi/ 
 * Define your updateControl() method.
 * Define your updateAudio() method.
 * Start by calling begin(control_rate)
 * do not call audioHook(); in the loop !
 * @ingroup generator
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
 
class MozziGenerator : public SoundGenerator<int16_t> {
    public:
        MozziGenerator(){   
             TRACED();
        }

        MozziGenerator(MozziConfig config){
            begin(config);
        }

        virtual ~MozziGenerator() {
            end();
        }

        void begin(MozziConfig config){
            SoundGenerator<int16_t>::begin();
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
        virtual int16_t readSample() {
            if (info.updateAudio==nullptr){
                LOGE("The updateAudio method has not been defined in the configuration !");
                stop();
                return 0;
            }

            // return value from buffer -> channel 2 
            if (is_read_buffer_filled){
                // for stereo output we might have the value already
                is_read_buffer_filled = false;
                return read_buffer;
            }

            // control update
            if (--control_counter<0){
                control_counter = control_counter_max;
                if (info.updateControl!=nullptr){
                    LOGD("updateControl");
                    info.updateControl();
                }
            }

            // return left value
            int16_t result = updateSample();
            return result;
        }

    protected:
        MozziConfig info;
        int control_counter_max;
        int control_counter;
        int read_buffer;
        bool is_read_buffer_filled = false;


        // we make sure that we return one sample for each 
        // requested number of channels sequentially
        int16_t updateSample(){
            AudioOutput out = info.updateAudio();
            // requested mono
            int16_t result = 0;
            #if (AUDIO_CHANNELS == MONO)
                if (info.channels==2){
                    result = out[0];
                    read_buffer = out[0];
                    is_read_buffer_filled = true;
                } else  if (info.channels==1){
                    result = out[0];
                }

            // requested stereo
            #elif (AUDIO_CHANNELS == STEREO)
                result = out[0];
                if (info.channels==2){
                    result = out[0];
                    read_buffer = out[1];
                    is_read_buffer_filled = true;
                } else if (info.channels==1){
                    result = out[0]/2 + out[1]/2;
                }
            #endif
            return result;
        }


};



/**
 * @brief We use the output functionality of Mozzi to output audio data. We expect the data as array of int16_t with
 * one or two channels. Though we support the setting of a sample rate, we recommend to use the default sample rate
 * from Mozzi which is available with the AUDIO_RATE define.
 * @ingroup dsp
 */
class MozziStream : public AudioStream {

    public:
        MozziStream(){
             TRACED();
        }

        ~MozziStream(){
             TRACED();
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
              TRACED();
            config = cfg;
            Mozzi.setAudioRate(config.sample_rate);
            // in output mode we do not allocate any unnecessary functionality
            if (cfg.channels != config.channels){
                LOGE("You need to change the AUDIO_CHANNELS in mozzi_config.h to %d", cfg.channels);
            }

            Mozzi.start(config.control_rate);
        }

        void end(){
             TRACED();
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
                    LOGE("Could not write all data: %d of %d", j, size);
                    return j;
                }
            }
            return size;
        }

        virtual int available(){
            return 100000;  // just a random big number
        }

        virtual size_t readBytes(char *buffer, size_t length){
            LOGD("readBytes: %d", length);
            return get_input_ptr()->readBytes((uint8_t*)buffer, length, config.channels);
        }

        virtual int read(){
            return not_supported(-1);
        }

        virtual int peek(){
            return not_supported(-1);
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

        MozziGenerator *get_input_ptr(){
            if (input_ptr==nullptr){
                // allocate  generator only when requested
                if (config.control_rate>0){
                    config.control_rate = CONTROL_RATE;
                }
                input_ptr = new MozziGenerator(config);
            }
            return input_ptr;
        }

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

