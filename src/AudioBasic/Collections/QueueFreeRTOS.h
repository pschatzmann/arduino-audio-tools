#pragma once
#include "AudioConfig.h"
#ifdef USE_CONCURRENCY
#include "freertos/FreeRTOS.h"

namespace audio_tools {

/**
 * @brief FIFO Queue which is based on a List
 * @ingroup collections
 * @author Phil Schatzmann
 * @copyright GPLv3
 * @tparam T 
 */
template <class T> 
class QueueFreeRTOS {
    public:
        QueueFreeRTOS(int size, TickType_t writeMaxWait=portMAX_DELAY, TickType_t readMaxWait=portMAX_DELAY) {
            TRACED();
            if (size>0){
                xQueue = xQueueCreate( size+1, sizeof(T) );
            }
            read_max_wait = readMaxWait;
            write_max_wait = writeMaxWait;
        };

        ~QueueFreeRTOS(){
            TRACED();
            vQueueDelete(xQueue);
        }

        void setReadMaxWait(TickType_t ticks){
            read_max_wait = ticks;
        }

        void setWriteMaxWait(TickType_t ticks){
            write_max_wait = ticks;
        }

        /// (Re-)defines the size
        void resize(int size){
            TRACED();
            if (xQueue!=NULL) vQueueDelete(xQueue);
            xQueue = xQueueCreate( size+1, sizeof(T) );
        }

        bool enqueue(T& data){
            TRACED();
            return xQueueSend( xQueue, ( void * ) &data, ( TickType_t ) write_max_wait );
        }

        bool peek(T& data){
            TRACED();
            return xQueuePeek(xQueue, &data, (TickType_t) read_max_wait );
        }

        bool dequeue(T& data){
            TRACED();
            return xQueueReceive(xQueue, &data, ( TickType_t ) read_max_wait );
        }

        size_t size() {
            return uxQueueMessagesWaiting(xQueue);
        }

        bool clear() {
            TRACED();
            T data;
            while(dequeue(data));
            return true;
        }

        bool empty() {
            return size()==0;
        }

    protected:
        QueueHandle_t xQueue = NULL;
        TickType_t write_max_wait=portMAX_DELAY;
        TickType_t read_max_wait=portMAX_DELAY;

};

}
#endif