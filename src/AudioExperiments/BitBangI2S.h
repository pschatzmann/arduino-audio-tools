#pragma once

/**
 * @brief A 2 Channel Audio Frame
 * 
 */

class I2SOut;
void *p_I2SOut=nullptr;


class BitBangI2SBase {
    public:
        BitBangI2SBase(){
        }

        virtual void begin(I2SConfig cfg) {
            this->cfg = cfg;
            active = true;
        }

        /**
         * @brief Stops the processing
         * 
         */
        void end() {
            active = false;
        }

        /**
         * @brief Write some audio data
         * 
         * @param data 
         * @param len 
         * @return size_t 
         */
        size_t write(void* data, size_t len){
            return buffer.writeArray(data, len);
        }

        operator bool() {
            return active;
        }

    protected:
        I2SConfig cfg;
        bool active = false;
        RingBuffer<uint8_t> buffer(1024);

        void writePins(int16_t data, int8_t bit, bool rlValue){
            bool value = data >> (15 - bit) & 1;
            fastWrite(value, true, rlValue)
            delayMicroseconds(bitTimeUs/2);
            fastWrite(value, false, rlValue)
            delayMicroseconds(bitTimeUs/2);
        }

}

/**
 * @brief Some Processors have multiple cores. We use one core to bit bang the I2S functioinality
 * just using basic functionality like loops. This is a base abstract class which needs to implement
 * the processor specific functionality.
 */
class BitBangI2SToCore : public BitBangI2SBase {
    public:
        BitBangI2SToCore() {
            p_I2SOut = this;
        }

        /**
         * @brief Starts the I2S processing
         * 
         * @param cfg 
         */
        virtual void begin(I2SConfig cfg) {
            this->cfg = cfg;

            // calculate timings
            bitRate = cfg.sampleRate * cfg.bits_per_sample * 2;
            bitTimeUs = (1000000 / bitRate) * 2;
            size_t writeTimeUs = measureWriteTimes(1000) / 1000;
            // adjust Waiting time by time spend for write
            bitTimeUs -= writeTimeUs;

            if (bitTimeUs<0){
                LOGE("bitrate is too high - we use maximum possible value")
                bitTimeUs = 0;
            }

            startCore(runLoop);
            active = true;

        }

    protected:
        int bitRate;
        int bitTimeUs;

        /**
         * @brief Starts the loop on a separate core
         * 
         * @param runLoop 
         */
        virtual void startCore(void(*runLoop)()) = 0

        /**
         * @brief Fast output to the 3 I2S pins in one go
         * 
         * @param value 
         * @param bitClockValue 
         * @param rlValue 
         */
        virtual void fastWrite(bool value, bool bitClockValue, bool rlValue) = 0;


        /// mesures the pin write times
        size_t measureWriteTimes(size_t count){
            size_t start = micros();
            for (size_t j=0;j<count;j++){
                fastWrite(false, false, false);
                fastWrite(true, true, true);
            }
            return micros()-start();
        }

        /// Process output in endless loop
        static void runLoop() {
            I2SOutCore1 ptr = (I2SOutCore1*)p_I2SOut;
            while(ptr->active){
                uint8_t audio[cfg.bits_per_sample][2]; 
                int byte_count = cfg.bits_per_sample*2;
                if (ptr->buffer.available() >= byte_count){
                    ptr->buffer.readArray((uint8_t*)audio, byte_count);
                } else {
                    memset(audio,0,byte_count);
                }
                for (uint8_t ch=0; ch<2; ch++){
                    for (uint8_t j=0;j<cfg.bits_per_sample;j++){
                        ptr->writePins(audio[ch], j, ch);
                    }
                }
            }
        }
};

/**
 * @brief Bit Banging I2S using a timer
 */

class BitBangI2SOutTimer : public BitBangI2SBase {
    public:
        BitBangI2SOutTimer() : BitBangI2SBase() {
            p_I2SOutTimer = this;
        }

        virtual void begin(I2SConfig cfg) {
            this->cfg = cfg;
            this->getData = getData;
            startTimer(bitTimeUs/2);
            active = true;
        }

    protected:
        uint8_t counter = 0;
        uint8_t bit_counter = 0;
        bool bit_value = false;
        uint8_t audio[self->cfg.bits_per_sample][2]; 
        int byte_count = self->cfg.bits_per_sample*2;


        virtual void startTimer(int time) = 0;

        virtual void fastWrite(bool value, bool bitClockValue, bool rlValue) = 0;

        /**
         * @brief The timer is called at the bitrate*2 in order to switch the bit_value on and 
         * at the next call off again.
         * 
         * @param ptr 
         */
        static void repeating_timer_callback(void *ptr) {
            I2SOutTimer *self =(I2SOutTimer *)ptr;

            // bit value is on
            if (bit_value) {
                if (counter==cfg.bits_per_sample*2){
                    // we need to switch the lr channel and get more data
                    if (self->buffer.available() >= byte_count){
                        self->buffer.readArray((uint8_t*)audio, byte_count);
                    } else {
                        memset(audio,0,byte_count);
                    }
                }
                if (bit_counter==16){
                    bit_counter = 0;
                }
            } else {
                // bit value is off
            }

            self->fastWrite(audio[bit_value], bit_value, lr_value);

            // toggle bit value
            bit_value = !bit_value;

            if (bit_value) {
                counter++;
                bit_counter++;
                lr_value = !lr_value;
            }
        }
        
};


/**
 * @brief RP2040 implementation of BitBangI2SToCore
 * 
 */
class RP2040I2SOutCore1 : public BitBangI2SToCore {
    public:
        RP2040I2SOutCore1() {
        }

        virtual void begin(I2SConfig cfg) {
            this->cfg = cfg;
            mask &= 1<<cfg.dataPin;
            mask &= 1<<cfg.rlClockPin;
            mask &= 1<<cfg.bitClockPin;
        }

    protected:
        uint32_t mask=0;

        void startCore(void(*runLoop)()) override {
            multicore_launch_core1(runLoop);
        }

        void fastWrite(bool value, bool bitClockValue, bool rlValue) override {
            uint32_t value = static_cast<uint32_t<(value)<<dataPin 
                                & static_cast<uint32_t<(rlValue)<<lrClockPin 
                                & static_cast<uint32_t<(bitClockValue)<<bitClockPin
            gpio_put_masked(mask, value);
        }
}

/**
 * @brief RP2040 implementation of BitBangI2SOutTimer
 * 
 */
class RP2040BitBangI2SOutTimer : public BitBangI2SOutTimer {
    public:
        virtual void begin(I2SConfig cfg) {
            this->cfg = cfg;
            mask &= 1<<cfg.dataPin;
            mask &= 1<<cfg.rlClockPin;
            mask &= 1<<cfg.bitClockPin;
        }

    protected:
        uint32_t mask=0;
        struct repeating_timer timer;

        virtual void startTimer(int timeUs) override {
            add_repeating_timer_us(timeUs, repeating_timer_callback, this, &timer);
        }

        void fastWrite(bool value, bool bitClockValue, bool rlValue) override {
            uint32_t value = static_cast<uint32_t<(value)<<dataPin 
                                & static_cast<uint32_t<(rlValue)<<lrClockPin 
                                & static_cast<uint32_t<(bitClockValue)<<bitClockPin
            gpio_put_masked(mask, value);
        }
};
