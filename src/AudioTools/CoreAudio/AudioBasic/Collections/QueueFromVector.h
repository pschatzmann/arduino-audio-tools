#pragma once
#include "AudioTools/CoreAudio/AudioBasic/Collections/List.h"

namespace audio_tools {

/**
 * @brief FIFO Queue which is based on a Vector
 * @ingroup collections
 * @author Phil Schatzmann
 * @copyright GPLv3
 * @tparam T 
 */
template <class T> 
class QueueFromVector {
    public:
        QueueFromVector(size_t size, T empty) { 
            null_value = empty;
            resize(size); 
        };

        bool enqueue(T& data){
            if (is_full()) 
                return false;
            vector[_end_pos++] = data;
            return true;
        }

        bool peek(T& data){
            if (_end_pos <= 0 ) {
                data = null_value;
                _end_pos = 0;
                return false;
            }
            data = vector[0];
            return true;
        }

        bool dequeue(T& data){
            if (_end_pos <= 0 ) {
                data = null_value;
                _end_pos = 0;
                return false;
            }
            // provide data at haed
            data = vector[0];
            // shift all data to the left by 1 position
            memmove(&vector[0], &vector[1], (_end_pos-1)*sizeof(T));
            vector[--_end_pos] = null_value;
            return true;
        }

        size_t size() {
            return _end_pos < 0 ? 0 : _end_pos;
        }

        bool resize(size_t size) {
            if (!vector.resize(size)){
                return false;
            } 
            return clear();
        }

        bool clear() {
            for (int j=0;j<vector.size();j++){
                vector[j] = null_value;
            }
            _end_pos = 0;
            return true;
        }

        bool empty() {
            return _end_pos == 0;
        }

        bool is_full() {
            return _end_pos >= vector.size(); 
        }
        
        size_t capacity() { return vector.capacity(); }

        void setAllocator(Allocator &allocator){
            vector.setAllocator(allocator);
        }

    protected:
        Vector<T> vector;
        int32_t _end_pos = 0;
        int empty_pos = 0;
        T null_value;
};

}