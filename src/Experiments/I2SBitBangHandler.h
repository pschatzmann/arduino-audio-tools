#pragma once

#include "AudioTools/AudioLogger.h"
#include "AudioTools/Buffers.h"
#include "AudioI2S/I2SConfig.h"

namespace audio_tools {

/**
 * @brief Platform specifc functionality called by this module
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class I2SDriver {
    public:
        virtual bool lock() = 0;
        virtual bool trylock(int64_t timeoutMs=10) = 0;
        virtual void unlock() = 0;
        virtual uint32_t fastRead() = 0;
        virtual void fastWrite(bool value, bool bitClockValue=0, bool rlValue=0) = 0;
        virtual void clearInterrupt() = 0;
        virtual void clearBitClock() = 0;
        virtual bool startCore(void(*runLoop)())=0;
};

/**
 * @brief Abstract base class for I2S scenarios for master/client and input/output
 *
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class BitBangI2SHandler {
    public:
        BitBangI2SHandler() = default;
        BitBangI2SHandler(I2SDriver *driver, I2SConfig &cfg, RingBuffer<uint8_t> &buffer) {
            this->p_buffer = &buffer;
            this->p_cfg = &cfg;
            this->p_driver = driver;
        }

        virtual I2SDriver* driver() {
            return p_driver;
        }

        virtual void process() = 0;

    protected:
        I2SDriver *p_driver = nullptr;
        I2SConfig *p_cfg = nullptr;
        RingBuffer<uint8_t> *p_buffer = nullptr;

};


/**
 * @brief I2S Master Output Scenario
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */

class BitBangI2SScenarioMastertOutput :  public BitBangI2SHandler {
    public:
        BitBangI2SScenarioMastertOutput(I2SDriver *driver, I2SConfig &cfg, RingBuffer<uint8_t> &buffer) : BitBangI2SHandler(driver, cfg, buffer) {
            byte_count = sizeof(int16_t);
        };

        // TX and master
        void process(){
            bool new_data_bit = (audio >> counter) & 1;
            // update data pins
            p_driver->fastWrite(new_data_bit, true, lr_value);
            counter--;
            if (counter==0) {
                readAudio();
                counter = 16;
                lr_value = !lr_value;
            }         
        }   

    protected:
        uint8_t byte_count = 0;
        uint8_t counter = 16;
        uint16_t audio = 0; 
        bool lr_value = true;

        void readAudio() {
            p_driver->lock();
            if (p_buffer->available() >= byte_count){
                p_buffer->readArray((uint8_t*)&audio, byte_count);
                p_driver->unlock();
            } else {
                p_driver->unlock();
                audio=0;
            }
        }

};

/**
 * @brief I2S Master Input Scenario
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */

class BitBangI2SScenarioMastertInput :  public BitBangI2SHandler {
    public:
        BitBangI2SScenarioMastertInput(I2SDriver *driver, I2SConfig &cfg, RingBuffer<uint8_t> &buffer) : BitBangI2SHandler(driver, cfg, buffer) {
        };

        // TX and master
        void process(){
            uint32_t in = p_driver->fastRead();
            bool new_data_bit = (in>>p_cfg->pin_data) & 1;
            audio = audio<<1 | new_data_bit;

            // check if word pin has changed
            if ((in_old>>p_cfg->pin_ws)&1 == 0 && (in>>p_cfg->pin_ws)&1 == 1){
                p_driver->lock();
                p_buffer->writeArray((uint8_t*)&audio, sizeof(audio));
                p_driver->unlock();
                audio = 0;    
            }
            in_old = in;
        }           

    protected:
        I2SDriver *p_driver = nullptr;
        I2SConfig *p_cfg = nullptr;
        RingBuffer<uint8_t> *p_buffer = nullptr;
        uint16_t audio = 0; 
        int audio_size = 32;
        uint32_t in_old = 0;

};

/**
 * @brief I2S Client Input Scenario
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
class BitBangI2SScenarioClientInput : public BitBangI2SHandler {

    public:
        BitBangI2SScenarioClientInput(I2SDriver *driver, I2SConfig &cfg, RingBuffer<uint8_t> &buffer) : BitBangI2SHandler(driver, cfg, buffer) {
        };

        void process() override {
            // raed all pins
            uint64_t in = p_driver->fastRead(); // e.g. gpio_get_all();
            // we need to process a new bit
            count++;
            // compare clock in_old with in -> process value with buffer if it has changed
            if (in_old>>p_cfg->pin_ws&1 == 0 && in>>p_cfg->pin_ws&1 == 1){
                // scale if necessary
                if (count>16){
                    actual_data_value = actual_data_value>>(count-16);
                }
                // write existing int16_t value
                writeBuffer(actual_data_value);
                // start new actual_data_value
                actual_data_value = new_data_bit;
                count = 0;
            } else {
                // just add a bit to value
                actual_data_value = (actual_data_value<<1) & new_data_bit;
            }
            // update old value
            in_old = in;
        }        

    protected:
        I2SDriver *p_driver = nullptr;
        I2SConfig *p_cfg = nullptr;
        RingBuffer<uint8_t> *p_buffer = nullptr;
        uint64_t in_old = 0;
        uint64_t actual_data_value = 0;
        bool new_data_bit = false;
        int count = 0;

        void writeBuffer(int16_t value){
            p_driver->lock();
            p_buffer->writeArray((uint8_t*)&value, sizeof(value));
            p_driver->unlock();
        }

        I2SDriver* driver() override {
            return p_driver;
        }

};

/**
 * @brief I2S Client Output Scenario
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */

class BitBangI2SScenarioClientOutput : public BitBangI2SHandler{
    public:
        BitBangI2SScenarioClientOutput(I2SDriver *driver, I2SConfig &cfg, RingBuffer<uint8_t> &buffer) : BitBangI2SHandler(driver, cfg, buffer) {
        };
     
        void process() override {
            // raed all active pins
            uint64_t in = p_driver->fastRead(); // e.g. gpio_get_all();
            // data latched -> we need to write a new bit
            if (count>0){
                new_data_bit =  (actual_data_value>>count) & 1; 
                p_driver->fastWrite(new_data_bit,true, lr_value);
                count--;
            }
            // ws needs to change -> read new value from buffer
            if (count == 0){
                lr_value = !lr_value; 
                // write existing int16_t value
                readBuffer();
                // we need to process 16 bits
                count = 16;
            }
        }
        
    protected:
        I2SDriver *p_driver = nullptr;
        I2SConfig *p_cfg = nullptr;
        RingBuffer<uint8_t> *p_buffer = nullptr;
        uint64_t in_old = 0;
        bool new_data_bit = false;
        int count = 16;
        bool lr_value = true;
        int16_t actual_data_value = 0;

        void readBuffer() {
            int len = sizeof(actual_data_value);
            p_driver->lock();
            if (p_buffer->available()>=len){
                p_buffer->readArray((uint8_t*)&actual_data_value, len);
             } else {
                 // write silence;
                 actual_data_value = 0;
             }
             p_driver->unlock();
        }

};



/**
 * @brief Consolidated class which forwards the requet to the proper implementation scenario
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class BitBangI2SScenarioHandler : public BitBangI2SHandler {
    public:
        BitBangI2SScenarioHandler(I2SDriver *driver, I2SConfig &cfg, RingBuffer<uint8_t> &buffer) {
            if (cfg.is_master && cfg.rx_tx_mode == TX_MODE){
                handler = new BitBangI2SScenarioMastertOutput(driver,cfg,buffer);
            } else if (cfg.is_master && cfg.rx_tx_mode == RX_MODE){
                handler = new BitBangI2SScenarioMastertInput(driver,cfg,buffer);
            } else if (!cfg.is_master && cfg.rx_tx_mode == TX_MODE){
                handler = new BitBangI2SScenarioClientOutput(driver,cfg,buffer);
            } else if (!cfg.is_master && cfg.rx_tx_mode == RX_MODE){
                handler = new BitBangI2SScenarioClientInput(driver,cfg,buffer);
            }
        };

        void process() override {
            handler->process();
        }

        I2SDriver* driver() override {
            return handler->driver();
        }
    
    protected:
        BitBangI2SHandler *handler;

};

}
