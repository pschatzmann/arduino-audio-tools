#pragma once

#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "i2s_master_out.h"
#include "i2s_master_in.h"
#include <vector>

#define I2S_LOGE printf

namespace rp2040_i2s {

class IBuffer;

/**
 * @brief An individual entry into the I2S buffer
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class I2SBufferEntry {
    public:
        I2SBufferEntry(uint8_t *data) {this->data = data;}
        uint8_t *data = nullptr;
        int audioByteCount = 0; // effectively used bytes 
};


/**
 * @brief Public Abstract Interface for Writer: writing audio data into buffer
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class IWriter {
    public:
        void begin(IBuffer *buffer);
        size_t write(uint8_t* data, size_t len);
};

/**
 * @brief Public Abstract Interface for Reader: reading audio data from buffer
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class IReader {
    public:
        void begin(IBuffer *buffer);
        size_t read(uint8_t* data, size_t len);
        void setupPIO(PIO pio, uint sm, uint offset, uint data_pin, uint clock_pin_base, uint sample_rate, uint bits_per_sample);
};

/**
 * @brief Public Abstract Interface for I2S Buffer
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class IBuffer {
    public:
        /// Writes the data using the DMA
        virtual size_t write(uint8_t* data, size_t len);
        virtual size_t read(uint8_t *data, size_t len);
        virtual void begin(IWriter* writer, IReader *reader = nullptr);
        virtual I2SBufferEntry* getFreeBuffer() = 0;
        virtual I2SBufferEntry* getFilledBuffer() = 0;
        virtual void addFreeBuffer(I2SBufferEntry *buffer) = 0;
        virtual void addFilledBuffer(I2SBufferEntry *buffer) = 0;
        virtual size_t availableForWrite() = 0;
};


/**
 * @brief Standard implementation of IWriter w/o DMA
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class I2SDefaultWriter : public IWriter {
    public:
        I2SDefaultWriter() = default;

        void begin(IBuffer *buffer){
            p_buffer = buffer;
        }

        size_t write(uint8_t* data, size_t len) {
            if (len>p_buffer->availableForWrite()){
                return 0; 
            }
            I2SBufferEntry *p_actual_write_buffer = p_buffer->getFreeBuffer();
            if (p_actual_write_buffer==nullptr){
                return 0;
            }
            memmove(p_actual_write_buffer->data, data, len);
            p_actual_write_buffer-> audioByteCount = len;
            p_buffer->addFilledBuffer(p_actual_write_buffer);
        }

    protected:
        IBuffer * p_buffer;

};


class MasterWriteWriter;
MasterWriteWriter *SelfDMAWriter=nullptr;

/**
 * @brief Writing Data to the buffer using the DMA
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class MasterWriteWriter : public IWriter {
    public:
        MasterWriteWriter() = default;

        void begin(IBuffer *buffer){
            SelfDMAWriter = this;
            p_buffer = buffer;
            // Get a free channel, panic() if there are none
            dma_write_channel = dma_claim_unused_channel(true);

            // Configure DMA channel
            dma_write_channel_config = dma_channel_get_default_config(dma_write_channel);
            channel_config_set_transfer_data_size(&dma_write_channel_config, DMA_SIZE_8);
            channel_config_set_read_increment(&dma_write_channel_config, true);
            channel_config_set_write_increment(&dma_write_channel_config, true);

            // enable interrupt
            irq_set_exclusive_handler(DMA_IRQ_0, dma_write_handler);
            irq_set_enabled(DMA_IRQ_0, true);
            dma_channel_set_irq0_enabled(dma_write_channel, true);
        }

        /// Writes the data using the DMA
        size_t write(uint8_t* data, size_t len) {
            if (len>p_buffer->availableForWrite()){
                return 0; 
            }
            p_actual_write_buffer = p_buffer->getFreeBuffer();
            if (p_actual_write_buffer==nullptr){
                return 0;
            }

            p_actual_write_buffer->audioByteCount = len;
            dma_channel_configure(
                dma_write_channel,           // Channel to be configured
                &dma_write_channel_config,   // The configuration we just created
                p_actual_write_buffer->data, // The initial write address
                data,                  // The initial read address
                len,                   // Number of transfers; in this case each is 1 byte.
                true                   // Start immediately.
            );

            return len;
        }

    protected:
        IBuffer * p_buffer;
        int dma_write_channel;
        dma_channel_config dma_write_channel_config;
        I2SBufferEntry *p_actual_write_buffer = nullptr;

        void writeHandler(){
            if (p_actual_write_buffer!=nullptr){
                p_buffer->addFilledBuffer(p_actual_write_buffer);
            }
            irq_clear(DMA_IRQ_0);
        }

        /// Interrupt handler when dma write has finished
        static void __isr __time_critical_func(dma_write_handler)() {
            SelfDMAWriter->writeHandler();
        }

};

void * SelfMasterReadWriter = nullptr;

/**
 * @brief Pulling data from PIO into the buffer. DMA interrupt handler 
 * 
 */
class MasterReadWriter : public IWriter {
    public:
        MasterReadWriter(PIO pio, int state_machine) {
            SelfMasterReadWriter = this;
            this->pio = pio;
            this->state_machine = state_machine;
        };

        void begin(IBuffer *buffer){
            p_buffer = buffer;
            dma_write_channel = dma_claim_unused_channel(true);
            if (dma_write_channel!=-1){
                dma_channel_config dma_write_channel_config = dma_channel_get_default_config(dma_write_channel);

                channel_config_set_dreq(&dma_write_channel_config, pio_get_dreq(pio, state_machine, true));
                channel_config_set_transfer_data_size(&dma_write_channel_config, transfer_size);
                dma_channel_configure(dma_write_channel,
                                    &dma_write_channel_config,
                                    NULL,  // dest TODO
                                    &pio->rxf[state_machine], // src
                                    0, // count
                                    false // trigger
                );

                irq_add_shared_handler(DMA_IRQ_1 + PICO_AUDIO_I2S_DMA_IRQ, dma_write_handler, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
                dma_irqn_set_channel_enabled(PICO_AUDIO_I2S_DMA_IRQ, dma_write_channel, 1);
            }
        }

        /// not supported
        size_t read(uint8_t* data, size_t len){
            return 0;
        }

    protected:
        IBuffer* p_buffer = nullptr;
        I2SBufferEntry *p_actual_write_buffer = nullptr;
        int state_machine;
        PIO pio;
        int dma_write_channel;
        const dma_channel_transfer_size transfer_size = DMA_SIZE_32;


        void writeHandler() {
            uint dma_channel = 1;
            if (dma_irqn_get_channel_status(PICO_AUDIO_I2S_DMA_IRQ, dma_write_channel)) {
                dma_irqn_acknowledge_channel(PICO_AUDIO_I2S_DMA_IRQ, dma_write_channel);
                // make last buffer we just finished available
                if (p_actual_write_buffer!=nullptr) {
                    p_buffer->addFilledBuffer(p_actual_write_buffer);
                    p_actual_write_buffer = nullptr;
                }

                // get next free buffer
                p_actual_write_buffer = p_buffer->getFreeBuffer();
                if (p_actual_write_buffer==nullptr){
                    // we overwrite the oldest data
                    p_actual_write_buffer = p_buffer->getFilledBuffer();
                }

                if (p_actual_write_buffer!=nullptr){
                    dma_channel_config config = dma_get_channel_config(dma_write_channel);
                    channel_config_set_write_increment(&config, true);
                    dma_channel_set_config(dma_write_channel, &config, false);
                    dma_channel_transfer_from_buffer_now(dma_write_channel, p_actual_write_buffer->data, p_buffer->availableForWrite());
                } 
            }
        }

        /// Interrupt handler when PIO needs more data 
        static void __isr __time_critical_func(dma_write_handler)() {
            static_cast<MasterReadWriter*>(SelfMasterReadWriter)->writeHandler();
        }

};

/**
 * @brief Reading Audio Data from the Buffer
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class I2SDefaultReader : public IReader {
    public:
        I2SDefaultReader() = default;

        void begin(IBuffer *buffer){
            p_buffer=buffer;
        }

        size_t read(uint8_t* data, size_t len){
            size_t result_len = 0;
            if (p_actual_read_buffer == nullptr){
                // get next buffer
                actual_read_pos = 0;
                p_actual_read_buffer = p_buffer->getFilledBuffer();
                actual_read_open = p_actual_read_buffer->audioByteCount;
                result_len = p_actual_read_buffer->audioByteCount < len ? p_actual_read_buffer->audioByteCount : len;
            } else {
                // get next data from actual buffer
                result_len = min(len, actual_read_open);
            }

            memmove(data, p_actual_read_buffer, result_len);
            actual_read_pos += result_len;
            actual_read_open -= result_len;

            // make buffer available again
            if (actual_read_open<=0){
                p_buffer->addFreeBuffer(p_actual_read_buffer);
                p_actual_read_buffer = nullptr;
            }
            return result_len;            
        }

    protected:
        IBuffer * p_buffer;
        // Regular read support
        I2SBufferEntry *p_actual_read_buffer = nullptr;
        size_t actual_read_open;
        size_t actual_read_pos;

};

void *SelfDMARederPIO=nullptr;

/**
 * @brief DMA Reader to get the data from the buffer to the PIO
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class MasterWriteReader : public  IReader {
    public:
        MasterWriteReader(PIO pio, int state_machine) {
            SelfDMARederPIO = this;
            this->pio = pio;
            this->state_machine = state_machine;
        };

        void begin(IBuffer *buffer){
            SelfDMARederPIO = this;
            p_buffer = buffer;
            dma_read_channel = dma_claim_unused_channel(true);
            if (dma_read_channel!=-1){
                dma_channel_config dma_read_channel_config = dma_channel_get_default_config(dma_read_channel);

                channel_config_set_dreq(&dma_read_channel_config, pio_get_dreq(pio, state_machine, true));
                channel_config_set_transfer_data_size(&dma_read_channel_config, transfer_size);
                dma_channel_configure(dma_read_channel,
                                    &dma_read_channel_config,
                                    &pio->txf[state_machine],  // dest TODO
                                    NULL, // src
                                    0, // count
                                    false // trigger
                );

                irq_add_shared_handler(DMA_IRQ_1 + PICO_AUDIO_I2S_DMA_IRQ, dma_read_handler, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
                dma_irqn_set_channel_enabled(PICO_AUDIO_I2S_DMA_IRQ, dma_read_channel, 1);
            }
        }

        /// not supported
        size_t read(uint8_t* data, size_t len){
            return 0;
        }

    protected:
        IBuffer* p_buffer = nullptr;
        I2SBufferEntry *p_actual_read_buffer = nullptr;
        int state_machine;
        PIO pio;
        int dma_read_channel;
        const dma_channel_transfer_size transfer_size = DMA_SIZE_32;


        void readHandler() {
            uint dma_channel = 1;
            if (dma_irqn_get_channel_status(PICO_AUDIO_I2S_DMA_IRQ, dma_read_channel)) {
                dma_irqn_acknowledge_channel(PICO_AUDIO_I2S_DMA_IRQ, dma_read_channel);
                // free the buffer we just finished
                if (p_actual_read_buffer!=nullptr) {
                    p_buffer->addFreeBuffer(p_actual_read_buffer);
                    p_actual_read_buffer = nullptr;
                }
                p_actual_read_buffer = p_buffer->getFilledBuffer();
                if (p_actual_read_buffer!=nullptr){
                    dma_channel_config config = dma_get_channel_config(dma_read_channel);
                    channel_config_set_read_increment(&config, true);
                    dma_channel_set_config(dma_read_channel, &config, false);
                    dma_channel_transfer_from_buffer_now(dma_read_channel, p_actual_read_buffer->data, p_actual_read_buffer->audioByteCount);
                } else {
                    // we dont have any data yet - we we just write zeros
                    int32_t no_data = 0;
                    dma_channel_config config = dma_get_channel_config(dma_read_channel);
                    channel_config_set_read_increment(&config, true);
                    dma_channel_set_config(dma_read_channel, &config, false);
                    dma_channel_transfer_from_buffer_now(dma_read_channel, &no_data, sizeof(no_data));
                }
            }
        }

        /// Interrupt handler when PIO needs more data 
        static void __isr __time_critical_func(dma_read_handler)() {
            static_cast<MasterWriteReader*>(SelfDMARederPIO)->readHandler();
        }

};


/**
 * @brief I2SBuffer to write audio data to a buffer and read the audio data back from the buffer. 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class I2SBuffer : public IBuffer {
    public:
        I2SBuffer(int count, int size) {
            buffer_size = size;       
            buffer_count = count;      
            // allocate buffers
            for (int j=0;j<count;j++){
                uint8_t *buffer = new uint8_t[size];
                if (buffer!=nullptr){
                    I2SBufferEntry* p_entry = new I2SBufferEntry(buffer);
                    freeBuffer.push_back(p_entry);
                }
            }   
        }

        /// the max size of an individual buffer entry
        size_t size() {
            return buffer_size * buffer_count;
        }

        size_t availableForWrite() {
            return buffer_size;
        }

        /// begin to support the regular read function
        void begin(IWriter* writer, IReader *reader = nullptr){
            p_reader = reader;
            if (p_reader!=nullptr){
                p_reader->begin(this);
            }
            p_writer = writer;
            if (p_writer!=nullptr){
                p_writer->begin(this);
            }
        }

        /// Writes the data using the DMA
        size_t write(uint8_t* data, size_t len) override {
            return p_writer->write(data, len);
        }

        /// reads data w/o DMA
        size_t read(uint8_t *data, size_t len) override {
            return p_reader->read(data, len);
        }

        /// Get the next empty buffer entry
        I2SBufferEntry* getFreeBuffer() {
            if (freeBuffer.size()==0){
                return nullptr;
            }
            I2SBufferEntry* result = freeBuffer.back();
            freeBuffer.pop_back();
            return result;
        }

        /// Make entry available again
        void addFreeBuffer(I2SBufferEntry *buffer){
            freeBuffer.push_back(buffer);
        }

        /// Add audio data to buffer
        void addFilledBuffer(I2SBufferEntry *buffer){
            filledBuffer.push_back(buffer);
        }

        /// Provides the next buffer with audio data
        I2SBufferEntry* getFilledBuffer() {
            if (filledBuffer.size()==0){
                return nullptr;
            }
            I2SBufferEntry* result = filledBuffer.back();
            filledBuffer.pop_back();
            return result;
        }

    protected:  
        std::vector<I2SBufferEntry*> freeBuffer;
        std::vector<I2SBufferEntry*> filledBuffer;
        size_t buffer_size;
        size_t buffer_count;
        // DMA support
        IWriter *p_writer = nullptr;
        IReader *p_reader = nullptr;

};

/**
 * @brief Setup Logic For a specific PIO 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class ISetupPIO {
    public:
        virtual void begin(PIO pio, uint state_machine,  uint data_pin, uint clock_pin_base, uint sample_rate, uint bits_per_sample);
};

/**
 * @brief Initialization for for i2s_master_out
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class PIOMasterOut : public ISetupPIO {
    public:
        PIOMasterOut() = default;
        void begin(PIO pio, uint state_machine, uint data_pin, uint clock_pin_base, uint sample_rate, uint bits_per_sample) override {
            pio_sm_claim(pio, state_machine);
            uint offset = pio_add_program(pio, &i2s_master_out_program);
            pio_sm_config cfg = i2s_master_out_program_get_default_config(offset);
            i2s_master_out(pio, state_machine, cfg, offset, data_pin,  clock_pin_base,  sample_rate, bits_per_sample);
        }
};

/**
 * @brief Initialization for for i2s_master_in
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class PIOMasterIn : public ISetupPIO {
    public:
        PIOMasterIn() = default;
        void begin(PIO pio, uint state_machine, uint data_pin, uint clock_pin_base, uint sample_rate, uint bits_per_sample) override {
            pio_sm_claim(pio, state_machine);
            uint offset = pio_add_program(pio, &i2s_master_in_program);
            pio_sm_config cfg = i2s_master_in_program_get_default_config(offset);
            i2s_master_in(pio, state_machine, cfg, offset, data_pin,  clock_pin_base,  sample_rate, bits_per_sample);
        }
};

/**
 * @brief Initialization for for i2s_slave_in
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class PIOSlaveIn : public ISetupPIO {
    public:
        PIOSlaveIn() = default;
        void begin(PIO pio, uint state_machine, uint data_pin, uint clock_pin_base, uint sample_rate, uint bits_per_sample) override {
        }
};

/**
 * @brief Defines the I2S Operation as either Read or Write
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
enum I2SOperation {I2SWrite, I2SRead };

/**
 * @brief I2S for the RP2040 using the PIO
 * The default pins are: BCLK: GPIO26, LRCLK:GPIO27, DOUT: GPIO28. The LRCLK can not be defined separately and is BCLK+1!
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class I2SClass  : public Stream {
    public:
        /// Default constructor
        I2SClass(PIO pio=pio0, uint sm=0) {
            this->state_machine = sm;
            this->pio = pio;
        };

        /// Destructor
        ~I2SClass() {
            if (p_buffer!=nullptr) delete p_buffer;
        }

        /// Defines the sample rate
        void setSampleRate(uint16_t sampleRate){
            sample_rate = sampleRate;
        }

        uint16_t sampleRate() {
            return sample_rate;
        }

        /// Defines the bits per samples (supported values: 8, 16, 32)
        void setBitsPerSample(uint16_t bits){
            bits_per_sample = bits;
        }

        uint16_t bitsPerSample() {
            return bits_per_sample;
        }

        /// defines the I2S Buffer cunt and size (default is 10 * 512 bytes)
        void setBufferSize(int count, int size){
            buffer_count = count;
            buffer_size = size;
            if (p_buffer!=nullptr){
                delete p_buffer;
            }
        }

        /// Defines if the I2S is running as master (default is master)
        void setMaster(bool master){
            is_master = master;
        }

        bool isMaster() {
            return is_master;
        }

        /// Defines the data pin
        void setDataPin(int pin){
            data_pin = pin;
        }

        int dataPin() {
            return data_pin;
        }

        /// Defines the clock pin (pin) and ws pin (=pin+1)
        void setClockBasePin(int pin){
            clock_pin_base = pin;
        }

        int clockBasePin() {
            return clock_pin_base;
        }

        /// Starts the processing
        void begin(I2SOperation mode){
            op_mode = mode;
            if (is_master){
                switch(mode){
                    case I2SWrite:
                        setup(new PIOMasterOut(), new I2SDefaultWriter(), new MasterWriteReader(pio, state_machine));
                       // begin(new PIOMasterOut(), new MasterWriteWriter(), new MasterWriteReader(pio, state_machine));
                        break;
                    case I2SRead:
                        setup(new PIOMasterIn(), new MasterReadWriter(pio, state_machine), new I2SDefaultReader());
                        break;                    
                }
            } else {
                switch(mode){
                    case I2SWrite:
                        // not supported
                        I2S_LOGE("Client mode does not support write");
                        break;
                    case I2SRead:
                        //begin(new PIOSlaveIn(), new MasterWriteWriter(), new MasterWriteReader());
                        break;                    
                }
            }
        }

        /// Not supported
        virtual int available() override {
            return op_mode == I2SRead && p_buffer!=nullptr ? p_buffer->availableForWrite() :  -1;
        }
        /// Not supported
        virtual int read() override {
            return -1;
        }
        /// Not supported
        virtual int peek() override {
            return -1;
        }

        virtual size_t write(uint8_t) {
            return 0;
        }

        /// Not supported
        virtual int availableForWrite() override {
            return op_mode == I2SWrite && p_buffer!=nullptr ? p_buffer->availableForWrite() :  -1;
        }

        /// Write data to the buffer (to the buffer)
        size_t write(uint8_t *data, size_t len){
            if (p_buffer==nullptr) return 0;
            return p_buffer->write(data, len);
        }

        /// Read data with filled audio data (from the buffer)
        size_t readBytes(uint8_t *data, size_t len){
            if (p_buffer==nullptr) return 0;
            return p_buffer->read(data, len);
        }

    protected:
        PIO pio;
        IBuffer* p_buffer = nullptr;
        bool is_master = true;
        uint16_t sample_rate = 44100;
        uint16_t bits_per_sample = 16;
        uint16_t buffer_count = 10;
        uint16_t buffer_size = 512;
        uint state_machine;
        uint data_pin = 28; // 
        uint clock_pin_base = 26; //pin bclk
        I2SOperation op_mode;

        /// Starts the processing
        void setup(ISetupPIO *pioSetup, IWriter* writer, IReader *reader = nullptr) {
            if (p_buffer=nullptr){
                p_buffer = new I2SBuffer(buffer_count, buffer_size);
            }

            // begin(PIO pio, uint state_machine, uint data_pin, uint clock_pin_base, uint sample_rate, uint bits_per_sample) 
            pioSetup->begin(pio, state_machine, data_pin, clock_pin_base, sample_rate, bits_per_sample);

            // setup buffer
            p_buffer->begin(writer, reader);
        }

};

}
