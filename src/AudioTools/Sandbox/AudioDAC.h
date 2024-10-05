#pragma once

#include "AudioTools/CoreAudio/AudioLogger.h"
#include "AudioTools/CoreAudio/Buffers.h"
#include "AudioTools/CoreAudio/AudioTimer/AudioTimer.h"
#include "AudioTools/CoreAudio/AudioOutput.h"

namespace audio_tools {

// forward declaration
class OversamplingDAC;
volatile uint32_t output_frame_count = 0;

/**
 * @brief Config info for DeltaSigma DAC
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
class DACInfo : public AudioInfo {
    public:
        friend class OversamplingDAC;    

        DACInfo() {
            sample_rate = 44100;
            channels = 2;
        } 

        /// By default we do not oversample 
        int oversample_factor = 1;

        /// Defines the pins: channel 0 is start_pin, channel 1 is start_pin+1 etc.
        int start_pin = PIN_PWM_START;

        /// Max number of bits used to output signal 
        int output_bits = 64;
        
        /// Provides the update bit rate 
        uint32_t outputBitRate() {
            const int bits = sizeof(int32_t) * 8;
            return sample_rate * oversample_factor * bits;
        }

        /// Provides the update frame rate 
        uint32_t outputSampleRate() {
            return (uint32_t)sample_rate * oversample_factor;
        }

        /// Logs the configuration settings to the console (if logging is active)
        void logInfo(bool withPins=false) {
            AudioInfo::logInfo();
            LOGI("oversample_factor: %d",oversample_factor );
            LOGI("output_bits: %d",output_bits );
            if (withPins){
                for (int j=0;j<channels;j++){
                    LOGI("pin%d: %d", j, start_pin+j);
                }
            }
        }
};

/**
 * @brief Output method for DeltaSigma DAC
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
class DACOut {
    public:
        void write(int pin, bool value){
            digitalWrite(pin, value);
        }
};


/**
 * @brief Abstract Software Implementation of an Oversampling DAC
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
class OversamplingDAC : public AudioOutput {
    public:

        OversamplingDAC(){
        }

        virtual ~OversamplingDAC(){
            end();
        }

        virtual DACInfo defaultConfig() {
            DACInfo result;
            return result;
        }

        /// starts the Delta Sigma DAC
        virtual bool begin(DACInfo cfg){
            TRACED();

            // reset if already running     
            if (active){       
                end();
            }

            // start processing
            this->info = cfg;

            // check audio data
            if (info.bits_per_sample!=16){
                LOGE("Only 16 Bits per sample are supported - you requested %d", info.bits_per_sample);
                return false;
            }

            if (info.sample_rate <= 0){
                LOGE("invalid sample_rate: %d", info.sample_rate);
                return false;
            }

            // setup memory
            const int channels = info.channels;
            current_values = new int32_t[channels];
            last_values = new int32_t[channels];
            cummulated_error = new int32_t[channels];
            startTimer();
            active = true;
            return true;
        }

        virtual uint32_t outputRate() = 0;

        /// Stops the output 
        virtual void end() {
            TRACED();
            active = false;
            reset();
            timer_object.end();
        }

        /// Writes a single byte (of audio data) to the output buffer
        virtual size_t write(uint8_t c){
            int result = 1;
            write_buffer[write_buffer_pos++] = c;
            if (write_buffer_pos==4){
                result = write(write_buffer, 4);
                if (result<=0){
                    // write failed - so we need to trigger a re-write
                    write_buffer_pos=3;
                } else{
                    result = 1;
                }
            }
            return result;
        }

        /// Writes the audio data to the output buffer
        virtual size_t write(const uint8_t *data, size_t len){
            TRACED();
            if (len==0) return 0;
            int16_t *ptr = (int16_t *)data;

            // fill buffer with delta sigma data
            int frames = std::min(len/(bytes_per_sample*info.channels), (size_t) availableFramesToWrite());
            while(is_blocking && frames==0){
                delay(10);
                frames = std::min(len/(bytes_per_sample*info.channels), (size_t) availableFramesToWrite());
            }
            int samples = frames * info.channels;
            for (int j=0; j<samples; j++){
                quantize(ptr[j], j % 2);
            }
            // return bytes
            return samples*2;
        }

        /// To be used for testing, we just count the number of frames that were sent to output
        virtual uint32_t outputFrameCount() {
            return output_frame_count;
        }


        TimerAlarmRepeating &timer() {
            return timer_object;
        }

        void setBlocking(bool blocking){
            is_blocking = blocking;
        }

        bool isBlocking() {
            return is_blocking;
        }

    protected:
        TimerAlarmRepeating timer_object;
        DACInfo info;
        DACOut out;
        int32_t *current_values = nullptr;
        int32_t *last_values = nullptr;
        int32_t *cummulated_error = nullptr;
        uint8_t write_buffer[4];
        //uint32_t output_frame_count = 0;
        uint8_t bytes_per_sample = 2;
        int write_buffer_pos = 0;
        int current_bit = -1;
        const int fixedPosValue=0x007fff00; /* 24.8 of max-signed-int */
        bool active;
        bool is_blocking = true;

        /// updates the buffer with analog value (represented by number of 1)
        virtual void quantize(int16_t newSamp, int left_right_idx) = 0;

        /// determines how many frames we can write to the buffer
        virtual size_t availableFramesToWrite() = 0;

        /// Releases the memory
        virtual void reset() {
            if (current_values!=nullptr){
                delete current_values;
                current_values = nullptr;
            }
            if (last_values!=nullptr){
                delete last_values;
                last_values = nullptr;
            }
            if (cummulated_error!=nullptr){
                delete cummulated_error;
                cummulated_error = nullptr;
            }
        }

        virtual void startTimer() = 0;

};



/**
 * @brief Software Implementation of a Simple DAC -  We quantize a digital int16_t sample my mapping the value to the range of 0b0 to info.output_bits number of 1, 
 * where the intensity is represented by the number of ones. This is very similar to PWM!
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
class SimpleDAC : public OversamplingDAC {
    friend void dacTimerCallback(void *ptr);

    public:
        SimpleDAC() : OversamplingDAC() {
        }

        ~SimpleDAC(){
            end();
            if (active_count!=nullptr){
                delete[]active_count;
            }
        }

        bool begin(DACInfo cfg) override {
            TRACED();
            cfg.logInfo(true);
            // default processing
            return OversamplingDAC::begin(cfg);
            if (active_count==nullptr){
                active_count = new uint8_t[cfg.channels];
            }
        }

        uint32_t outputRate() override {
            return info.outputBitRate();
        }


        virtual void startTimer() {
            TRACED();
            // start (optional) timer
            if (outputRate()>0){
                uint32_t timeUs = AudioTime::toTimeUs(outputRate());
                auto dacTimerCallback = [] (void *ptr)->void IRAM_ATTR {
                    output_frame_count++;
                    ((SimpleDAC*) ptr)->writePins();
                };
                timer_object.setCallbackParameter(this);
                timer_object.begin(dacTimerCallback, timeUs, US);
                LOGI("Timer started");
            } else {
                LOGW("No output because output Rate <=0");
            }
        }


    protected:
        RingBuffer<uint8_t> buffer = RingBuffer<uint8_t>(DEFAULT_BUFFER_SIZE);
        int bit_counter = 0;
        uint8_t *active_count=nullptr;
        DACOut out;

        void writePins()  {
            int channels = info.channels;
            int32_t current_values[channels];
            if (active && buffer.available()>=2){
                // determine number of hight bits
                if (bit_counter==0){
                    active_count[0]=buffer.read();
                    active_count[1]=buffer.read();
                }

                // write bits for all channels
                for (int j=0;j<channels;j++){
                    out.write(info.start_pin+j, active_count[j]<=bit_counter);
                }

                // move to next bit
                bit_counter++;
                if (bit_counter>=info.output_bits){
                    bit_counter = 0;
                }
            }
        }
        size_t availableFramesToWrite() override {
            // 1 sample = 1 byte -> divide by channels
            return buffer.availableForWrite() / info.channels;
        }

        /// updates the buffer with analog value (represented by number of 1)
        void quantize(int16_t audioValue, int left_right_idx) override {
            // map to values from intput to number of active output_bits;
            uint16_t sample = map(audioValue, -32768, 32767, 0, info.output_bits );
            buffer.write(sample);
        }


};


/**
 * @brief Software Implementation of a Simple DAC -  We quantize a digital int16_t sample my mapping the value to the range of 0b0 to 0b11111111111111111111111111111111, 
 * where the intensity is represented by the number of ones. This gives an overall resultion of 5 bits and just uses one single timer.
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */

class OversamplingDAC32 : public OversamplingDAC {
    public:
        OversamplingDAC32() : OversamplingDAC() {
        }

        bool begin(DACInfo cfg) override {
            TRACED();
            cfg.logInfo(true);
            // default processing
            return OversamplingDAC::begin(cfg);
        }

        virtual uint32_t outputRate() override {
            return info.outputBitRate();
        }

        virtual void startTimer() {
            TRACED();
            // start (optional) timer
            if (outputRate()>0){
                uint32_t timeUs = AudioTime::toTimeUs(outputRate());
                auto dacTimerCallback = [] (void *ptr)->void IRAM_ATTR {
                    output_frame_count++;
                    ((OversamplingDAC32*) ptr)->writePins();
                };
                timer_object.setCallbackParameter(this);
                timer_object.begin(dacTimerCallback, timeUs, US);
                LOGI("Timer started");
            } else {
                LOGW("No output because output Rate <=0");
            }
        }


    protected:
        RingBuffer<int32_t> buffer = RingBuffer<int32_t>(DEFAULT_BUFFER_SIZE);
        DACOut out;


        size_t availableFramesToWrite() override {
            // 1 sample = 4 types -> divede by 4 and channels
            return buffer.availableForWrite() / (sizeof(int32_t) * info.channels);
        }

        /// Default output 
        void writePins()  {
            int channels = info.channels;
            int32_t current_values[channels];
            if (active && buffer.available()>=2){
                // determine new values for all channels when all bits have been written
                if (current_bit < 0){
                    for (int j=0;j<channels;j++){
                        current_values[j] = buffer.read();
                    }
                    current_bit = 31;
                }

                // write bits for all channels
                for (int j=0;j<channels;j++){
                    out.write(info.start_pin+j, current_values[j] >> (current_bit) & 1);
                }
                // move to next bit
                current_bit--;
            }
        }
        
        /// updates the buffer with analog value (represented by number of 1)
        virtual void quantize(int16_t newSamp, int left_right_idx) override{
            // map to unsigned
            uint16_t sample = newSamp + (0xFFFF / 2);
            // Don't need lastSamp anymore, store this one for next round
            last_values[left_right_idx] = sample;

            // How much the comparison signal changes each oversample step
            int32_t diffPerStep = (sample - last_values[left_right_idx]) >> (4 + info.oversample_factor);
            for (int j = 0; j < info.oversample_factor; j++) {
                // convert to bits
                uint32_t bits = 0xFFFFFFFF; // max value 
                bits = bits >> (32 - (sample/2048));
                buffer.write(bits);
            }
        }
};



/**
 * @brief A SimpleDAC which uses the Serial UART to output values. This implementation is not using any timers and therefore should
 * work on any microcontroller.
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class SerialDAC : public OversamplingDAC32 {
    public:
        SerialDAC() : OversamplingDAC32() {
            serial = &Serial;
        }

        SerialDAC(HardwareSerial &out) : OversamplingDAC32() {
            serial = &out;
        }

        DACInfo defaultConfig() override {
            DACInfo result;
            result.oversample_factor = 10;
            return result;
        }

        bool begin(DACInfo info) override {
            TRACED();
            info.logInfo(false);
            this->cfg = &info;
            // setup baud rate in the uart
            int rate = info.sample_rate * 8 * info.oversample_factor ; //outputSampleRate();
            LOGI("Setting Baudrate: %d", rate);
            if (rate>0){
                serial->begin(rate);
            } else {
                LOGE("sample_rate not defined");
            }
            // default processing
            return OversamplingDAC::begin(info);
        }

        /// just write the data to the UART w/o buffering
        virtual size_t write(const uint8_t *data, size_t len) override {
            TRACED();
            return serial->write(data, len);
        }

        virtual uint32_t outputRate() override {
            return 0;
        }


    protected:
        HardwareSerial *serial = nullptr;
        int16_t totalSamples;
        DACInfo *cfg;

         /// updates the buffer with analog value (represented by number of 1)
        virtual void quantize(int16_t newSamp, int left_right_idx) override {
            if (left_right_idx==0){
                totalSamples = newSamp;
            } else {
                totalSamples += newSamp;
            }
            // on last channel we output
            if (left_right_idx==cfg->channels-1) {
                // combine left and right and map to unsigned
                uint16_t sample = (totalSamples / cfg->channels) + (0xFFFF / 2);
                // How much the comparison signal changes each oversample step
                int32_t diffPerStep = (sample - last_values[left_right_idx]) >> (4 + info.oversample_factor);
                for (int j = 0; j < info.oversample_factor; j++) {
                    // convert to bits
                    uint32_t bits = 0xFFFFFFFF; // max value 
                    bits = bits >> (32 - (sample/2048));
                    buffer.write(bits);
                }
            }
        }
};       

#ifdef USE_DELTASIGMA

/**
 * @brief Software Implementation of a Delta Sigma DAC - The advantage of this implementation is that it works with any
 * digital output pin and that it supports as many channels as necessary!
 * The logic is inspired by Earl Phil Howers ESP8266Audio's  AudioOutputI2SNoDAC implementation.
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */

class DeltaSigmaDAC : public OversamplingDAC32 {
    public:
        DeltaSigmaDAC() : OversamplingDAC32() {}

        DACInfo defaultConfig() override {
            DACInfo result;
            result.oversample_factor = 2;
            return result;
        }

        bool begin(DACInfo cfg) override {
            TRACED();
            cfg.logInfo(true);
            // default processing
            return OversamplingDAC::begin(cfg);
        }

        virtual uint32_t outputRate() override {
            return info.outputBitRate();
        }


    protected:
        /// updates the buffer with delta sigma values
        virtual void quantize(int16_t newSamp, int left_right_idx) override {
            // Don't need lastSamp anymore, store this one for next round
            last_values[left_right_idx] = newSamp;

            // How much the comparison signal changes each oversample step
            int32_t diffPerStep = (newSamp - last_values[left_right_idx]) >> (4 + info.oversample_factor);
            for (int j = 0; j < info.oversample_factor; j++) {
                uint32_t bits = 0; // The bits we convert the sample into, MSB to go on the wire first
                
                for (int i = 32; i > 0; i--) {
                    bits = bits << 1;
                    if (cummulated_error[left_right_idx] < 0) {
                        bits |= 1;
                        cummulated_error[left_right_idx] += fixedPosValue - newSamp;
                    } else {
                        // Bits[0] = 0 handled already by left shift
                        cummulated_error[left_right_idx] -= fixedPosValue + newSamp;
                    }
                    newSamp += diffPerStep; // Move the reference signal towards destination
                }                
                buffer.write(bits);
            }
        }
};

#endif

#ifdef ESP32 


/**
 * @brief Audio Output with PWM signal
 * 
 */
class PWMDAC : public OversamplingDAC {
    public:
        PWMDAC(int pwmFrequency=PWM_FREQENCY) : OversamplingDAC() {
            pmw_frequency = pwmFrequency;
        }

        virtual ~PWMDAC(){
            end();
        }

        virtual DACInfo defaultConfig() {
            DACInfo result;
            // we use 16bits by default
            result.output_bits = 16;
            return result;
        }

        virtual bool begin(DACInfo cfg) override {
            TRACED();
            cfg.logInfo(true);
            // default processing
            bool result = OversamplingDAC::begin(cfg);
            // automatic scaling
            max_pwm_value = pow(2, info.output_bits);
            setupPins();

            return result;
        }

        /// We use the sample rate to output values
        virtual uint32_t outputRate() override {
            return info.sample_rate;
        }

    protected:
        RingBuffer<uint16_t> buffer = RingBuffer<uint16_t>(DEFAULT_BUFFER_SIZE);
        uint32_t max_pwm_value;
        uint32_t pmw_frequency;

        virtual size_t availableFramesToWrite() override {
            // 1 sample = 1 byte -> divide by channels
            return buffer.availableForWrite() / info.channels;
        }

        /// updates the buffer with analog value (represented by number of 1)
        virtual void quantize(int16_t audioValue, int left_right_idx) override {
            // map to values from intput to number of active output_bits;
            uint16_t sample = map(audioValue, -32768, 32767, 0, max_pwm_value );
            buffer.write(sample);
        }

        void setupPins() {
            TRACED();
            LOGI("pmw_frequency: %u", pmw_frequency)
            LOGI("max_pwm_value: %u", max_pwm_value)
            for (int ch=0;ch<info.channels;ch++){
                ledcSetup(ch, pmw_frequency, info.output_bits);                
                // attach the channel to the GPIO to be controlled
                ledcAttachPin(info.start_pin, ch);
            }
        }

        /// Output data to pins
        void writePins()  {
            if (buffer.available()>=info.channels){
                for (int ch=0;ch<info.channels;ch++){
                    ledcWrite(ch, buffer.read());
                }
            }
        }

        virtual void startTimer() override {
            TRACED();
            // start (optional) timer
            if (outputRate()>0){
                uint32_t timeUs = AudioTime::toTimeUs(outputRate());
                auto dacTimerCallback = [] (void *ptr)->void IRAM_ATTR {
                    output_frame_count++;
                    ((PWMDAC*) ptr)->writePins();
                };
                timer_object.setCallbackParameter(this);
                timer_object.begin(dacTimerCallback, timeUs, US);
                LOGI("Timer started");
            } else {
                LOGW("No output because output Rate <=0");
            }
        }
};


#endif

} // namesapce

