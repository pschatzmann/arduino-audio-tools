#pragma once

#if defined(ARDUINO_ARCH_RP2040) 
#include "AudioConfig.h"
#ifdef USE_I2S
#include "Experiments/I2SBitBang.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"

namespace audio_tools {

/**
 * @brief Platform Specific functionality implementation for RP2040
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class RPDriver : public I2SDriver {
    public: 
        RPDriver(){
            sem_init(&sem, 1,1);
        }

        ~RPDriver(){
            sem_release(&sem);
        }

        void begin(I2SConfig cfg, bool ws_active=false) {
            this->cfg = cfg;
            //activate bit for used pins
            this->cfg = cfg;
            if (cfg.is_master){
                maskOutput |= 1<<cfg.pin_ws;
                if (ws_active) {
                    maskOutput |= 1<<cfg.pin_bck;
                }
            } else {
                maskInput |= 1<<cfg.pin_ws;
                maskInput |= 1<<cfg.pin_bck;
            }
            if (cfg.rx_tx_mode==TX_MODE){
                maskOutput |= 1<<cfg.pin_data;
            } else {
                maskInput |= 1<<cfg.pin_data;
            }
            LOGI("Active output pins: %s", toBits(sizeof(maskOutput), &maskOutput));
            LOGI(" Active input pins: %s", toBits(sizeof(maskInput), &maskInput));
        }

        void fastWrite(bool dataValue, bool bitClockValue, bool rlValue) override {
            uint32_t value = static_cast<uint32_t>(dataValue)<<cfg.pin_data 
                                | static_cast<uint32_t>(rlValue)<<cfg.pin_ws 
                                | static_cast<uint32_t>(bitClockValue)<<cfg.pin_bck;
            gpio_put_masked(maskOutput, value);
        }
        
        uint32_t fastRead() override {
            return gpio_get_all();
        }

        virtual bool lock() override {
            sem_acquire_blocking(&sem);
            return true;
        }
 
        virtual bool trylock(int64_t timeoutMs=100) override {
            return sem_acquire_timeout_ms(&sem, timeoutMs);
        }

        virtual void unlock() override {
            sem_release(&sem);
        }

        virtual void clearInterrupt() {
             pwm_clear_irq(pwm_gpio_to_slice_num(cfg.pin_bck));
        }

        virtual void clearBitClock(){
            gpio_put(cfg.pin_bck,0);
        }

        bool startCore(void(*runLoop)()) override {
            TRACED();
            multicore_launch_core1(runLoop);
            return true;
        }

    protected:
        I2SConfig cfg;
        uint32_t maskInput = 0;
        uint32_t maskOutput = 0;
        semaphore_t sem;

        // Assumes little endian
        const char* toBits(size_t const size, void* ptr) {
            static char str[65] = {0};
            unsigned char *b = (unsigned char*) ptr;
            unsigned char byte;
            int i, j, z=0;
            
            for (i = size-1; i >= 0; i--) {
                for (j = 7; j >= 0; j--) {
                    byte = (b[i] >> j) & 1;
                    str[z++]= byte ? '1': '0';
                }
            }
            str[z]=0;
            return str;
        }
};

/**
 * @brief RP2040 implementation of BitBangI2SToCore
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
class RP2040BitBangI2SCore1 : public BitBangI2SToCore{
    public:
        RP2040BitBangI2SCore1() {
            setI2ScenarioHandler(new BitBangI2SScenarioHandler(&driver, cfg, buffer));
        }

    protected:
        RPDriver driver;
        BitBangI2SScenarioHandler *handler;

};

/**
 * @brief RP2040 implementation of BitBangI2SWithInterrupts
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
class RP2040BitBangI2SWithInterrupts : public BitBangI2SWithInterrupts {
    public:
        RP2040BitBangI2SWithInterrupts() {
            setI2ScenarioHandler(new BitBangI2SScenarioHandler(&driver, cfg, buffer));
        };

        virtual bool begin(I2SConfig cfg) override {
            TRACEI();
            // setup the reader
            LOGI("The sample rate is %d hz", cfg.sample_rate);
            // call begin in parent class
            return BitBangI2SWithInterrupts::begin(cfg);
        }

        void end() {
            TRACEI();
            BitBangI2SWithInterrupts::end();
            pwm_set_enabled(slice_num, false);
            gpio_set_irq_enabled_with_callback(cfg.pin_bck, GPIO_IRQ_EDGE_RISE, false, &gpio_callback);
            active = false;
        }

    protected:
        RPDriver driver;
        uint slice_num;

        // We use a 50% pwm signal to generate the output for the pin_bck
        virtual void startClockOutSignal(unsigned long frequency) override {
            TRACEI();
            slice_num = pwm_gpio_to_slice_num(cfg.pin_bck);
            uint channel_num = pwm_gpio_to_channel(cfg.pin_bck);
            int max_counter = 10;
            // 125Mhz / 2 = 62.5MHz
            float divider = 125000000.0 / max_counter / frequency;
            LOGI("divider: %f -> %f hz", divider, 125000000.0 / divider );

            gpio_set_function(cfg.pin_bck, GPIO_FUNC_PWM);
            pwm_config config = pwm_get_default_config();
            pwm_config_set_clkdiv(&config, divider);
            pwm_config_set_wrap(&config, max_counter);
            pwm_config_set_phase_correct(&config, false);

            // Load the configuration into our PWM slice, and set it running.
            pwm_init(slice_num, &config, true);
            pwm_set_chan_level(slice_num, channel_num, config.top/2);      
        }

        virtual void startPinInterrupt() override {
            TRACEI();
            if (cfg.is_master){
                pwm_clear_irq(slice_num);
                pwm_set_irq_enabled(slice_num, true);
                irq_set_exclusive_handler(PWM_IRQ_WRAP, &pwm_callback);
                irq_set_enabled(PWM_IRQ_WRAP, true);
                //gpio_set_irq_enabled_with_callback(cfg.pin_bck, GPIO_IRQ_EDGE_RISE, true, &gpio_callback);

            } else {
                gpio_set_irq_enabled_with_callback(cfg.pin_bck, GPIO_IRQ_EDGE_RISE, true, &gpio_callback);

            }
        }

        static void pwm_callback() {
            BitBangI2SWithInterrupts::gpio_callback();
        }

        static void gpio_callback(uint gpio, uint32_t events) {
            BitBangI2SWithInterrupts::gpio_callback();
        }

};

}

#endif
#endif
