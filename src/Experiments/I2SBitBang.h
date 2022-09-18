#pragma once

#include "AudioConfig.h"
#ifdef USE_I2S
#include "Experiments/I2SBitBangHandler.h"

#define MIN_WRITE_LEN I2S_BUFFER_SIZE/2

namespace audio_tools {

// pointer to actual I2S class 
void *p_I2S_Instance = nullptr;



/**
 * @brief Abstract I2S Base class for differerent bit bang implementations of the I2S Protocol. It uses 3 different signal lines: 
 * 1) a clock, 2) a left right selectsion and 3) a data line. 
 * 
 * The left audio is transmitted on the low cycle of the word select clock and the right channel is transmitted on the high. 
 * Currently we only support int16_t values. The values are sent in 16 cycles: so there is no extra unused space left and
 * left aligned is equal to right aligned.
 *   
 * When updating the 3 lines we try to update all 3 lines in one go.
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class BitBangI2SBase {
    public:
        BitBangI2SBase(){
        }

        /**
         * @brief Starts the I2S interface
         * 
         * @param cfg 
         */
        virtual bool begin(I2SConfig cfg) = 0;

        /**
         * @brief Stops I2S
         * 
         */
        void end() {
            TRACED();
            active = false;
        }

        /**
         * @brief Write some audio data
         * 
         * @param data 
         * @param len 
         * @return size_t 
         */
        size_t writeBytes(const void* data, size_t len){
            LOGD("%s: %d", LOG_METHOD, len);
            size_t result = 0;
            if (p_driver->trylock()){
                result = buffer.writeArray((uint8_t*)data, len);
                p_driver->unlock();
            }
            return result;
        }

        /**
         * @brief Read some audio data
         * 
         * @param data 
         * @param len 
         * @return size_t 
         */
        size_t readBytes(void* data, size_t len){
            LOGD("%s: %d", LOG_METHOD, len);
            size_t result = 0;
            if (p_driver->trylock()){
                result = buffer.readArray((uint8_t*)data, len);
                p_driver->unlock();
            }
            return result;
        }

        /**
         * @brief Provides the information how many bytes we can write
         * 
         * @return size_t 
         */
        size_t availableForWrite(){
            size_t result = 0;
            if (p_driver->trylock()){
                result = buffer.availableForWrite();
                p_driver->unlock();
            }
            return result;
        }

        /**
         * @brief Number of bytes that are currently available to read
         * 
         * @return size_t 
         */

        size_t available(){
            size_t result = 0;
            if (p_driver->trylock()){
                result = buffer.available();
                p_driver->unlock();
            }
            return result;
        }

        /**
         * @brief Provides the default configuration
         * 
         * @param mode 
         * @return I2SConfig 
         */

        I2SConfig defaultConfig(RxTxMode mode) {
            I2SConfig c(mode);
            return c;
        }

        /**
         * @brief Provides the actual configuration
         * 
         * @return I2SConfig 
         */
        I2SConfig config() {
            return cfg;
        }

        /**
         * @brief Defines the Reader
         * 
         * @param reader 
         */
        void setI2ScenarioHandler(BitBangI2SScenarioHandler*reader){
            i2sHandler = reader;
            p_driver = reader->driver();
        }

        /**
         * @brief Returns true if I2S has been started
         * 
         * @return true 
         * @return false 
         */
        operator bool() {
            return active;
        }

    protected:
        I2SConfig cfg;
        bool active = false;
        RingBuffer<uint8_t> buffer = RingBuffer<uint8_t>(I2S_BUFFER_SIZE*I2S_BUFFER_COUNT);
        BitBangI2SScenarioHandler *i2sHandler = nullptr;
        I2SDriver *p_driver;

        void setupPins() {
            LOGI("setupPins: %s", cfg.rx_tx_mode == TX_MODE ? "OUTPUT": "INPUT" );
            int modeData =  cfg.rx_tx_mode == TX_MODE ? OUTPUT : INPUT;
            int modeClock =  cfg.is_master ? OUTPUT : INPUT;
            pinMode(cfg.pin_data, modeData);   
            pinMode(cfg.pin_ws, modeClock);   
            pinMode(cfg.pin_bck, modeData);    
        }


};



/**
 * @brief Some Processors have multiple cores. We use one core to bit bang the I2S functioinality
 * just using basic functionality like loops. This is a base abstract class which needs to implement
 * the processor specific functionality.
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class BitBangI2SToCore : public BitBangI2SBase {
    public:
        BitBangI2SToCore() {
            p_I2S_Instance = this;
        }

        /**
         * @brief Starts the I2S processing
         * 
         * @param cfg 
         */
        virtual bool begin(I2SConfig cfg) {
            TRACED();
            this->cfg = cfg;
            cfg.logInfo();
            if (i2sHandler==nullptr){
                LOGE("The i2sHandler is null");
                return false;
            }
            setupPins();
            bitRate = cfg.sample_rate * cfg.bits_per_sample * 2;
            bitTimeUs = (1000000 / bitRate) * 2;
            active = beginIO();
            return active;
        }


    protected:
        int bitRate;
        int bitTimeUs;
        int16_t audio[2]; 
        int byte_count = 2;

        // setup for write as master
        bool beginIO() {
            // calculate timings
            size_t writeTimeUs = measureWriteTimes(1000) / 1000;
            // adjust Waiting time by time spend for write
            bitTimeUs -= writeTimeUs;
            LOGI("The sample rate is %d -> The bit time is %d us", cfg.sample_rate, bitTimeUs);

            if (bitTimeUs<0){
                LOGW("Bitrate is too high - we use maximum possible value to write out the data")
                bitTimeUs = 0;
            }

            // prepare input
            if (cfg.rx_tx_mode == RX_MODE || !cfg.is_master){
                if (i2sHandler==nullptr){
                    LOGE("The i2sHandler is null");
                    return false;
                }
               // we would like the reading spead to be twice of the received bit clock speed
                size_t writeTimeUs = measureWriteTimes(1000) / 1000;
                if (bitTimeUs<writeTimeUs*2){
                    LOGW("Sample rate is too high: %d - we might loose some data",cfg.sample_rate);
                }
            }

            return p_driver->startCore(run_loop_callback);
        }


        /// mesures the pin write times
        size_t measureWriteTimes(size_t count){
            size_t start = micros();
            for (size_t j=0;j<count;j++){
                p_driver->fastWrite(false, false, false);
                p_driver->fastWrite(true, true, true);
            }
            return micros()-start;
        }

        /// Process output in endless loop
        void runLoop() {
            TRACED();
            while(active) {
                int64_t end1 = micros()+bitTimeUs;
                int64_t end2 = end1+bitTimeUs;
                i2sHandler->process();
                int64_t diff = end1 - micros();
                if (diff>0) delayMicroseconds(diff);                
                p_driver->clearBitClock();
                diff = end2 - micros();
                if (diff>0) delayMicroseconds(diff);
            }
        }

        /// Callback for input
        static void run_loop_callback() {
            BitBangI2SToCore *self = (BitBangI2SToCore*)p_I2S_Instance;
            self->runLoop();
        }
};

/**
 * @brief Bit Banging I2S using a function to generate the clock signal and interrupts
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class BitBangI2SWithInterrupts : public BitBangI2SBase {
    public:
        BitBangI2SWithInterrupts() : BitBangI2SBase() {
            p_I2S_Instance = this;
        }

        bool begin(I2SConfig cfg) {
            TRACED();
            cfg.logInfo();
            if (i2sHandler==nullptr){
                LOGE("The i2sHandler is null");
                return false;
            }

            this->cfg = cfg;
            setupPins();
            LOGI("The sample rate is %d hz", cfg.sample_rate);

            // start interrupts on core 1
            p_driver->startCore(start_core_cb);
            active = true;
            return true;
        }

        /// provides the actual configuration
        I2SConfig config() {
            return cfg;
        }

    protected:

        /// Starts the clock signal
        virtual void startClockOutSignal(unsigned long frequency) = 0;

        /// Starts the pin interrupt on the clock signal
        virtual void startPinInterrupt() = 0;

        /// Callback which is called by input change on the clock pin
        static void gpio_callback() {
            BitBangI2SWithInterrupts *self =(BitBangI2SWithInterrupts *)p_I2S_Instance;
            self->i2sHandler->process();
        } 

        /// Starts the PWM and Interrupts on a separate core
        static void start_core_cb() {
            BitBangI2SWithInterrupts *self =(BitBangI2SWithInterrupts *)p_I2S_Instance;
            I2SConfig cfg = self->cfg;

            // for the time beeing we support only int16_t
            int channels = 2;
            unsigned long bitRate = cfg.sample_rate * cfg.bits_per_sample * channels;
            LOGI("bitRate: %lu", bitRate);
            if (cfg.is_master){
                // adjust Waiting  time by time spend for write
                self->startClockOutSignal(bitRate);
            } 
            
            // start raising interrupt on clock pin
            self->startPinInterrupt();
            self->active = true;
            while (true) tight_loop_contents();
        }   
};

}

#endif
