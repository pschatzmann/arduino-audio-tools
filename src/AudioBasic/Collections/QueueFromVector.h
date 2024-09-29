#pragma once
#include "AudioBasic/Collections/List.h"

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
            vector[_size++] = data;
            return true;
        }

        bool peek(T& data){
            if (_size <= 0 ) {
                data = null_value;
                return false;
            }
            data = vector[0];
            return true;
        }

        bool dequeue(T& data){
            if (_size <= 0 ) {
                data = null_value;
                return false;
            }
            data = vector[0];
            memmove(&vector[0], &vector[1], _size*sizeof(T));
            vector[_size--] = null_value;
            return true;
        }

        size_t size() {
            return _size;
        }

        bool resize(size_t size) {
            if (!vector.resize(size)){
                return false;
            } 
            _size = size;
            return clear();
        }

        bool clear() {
            for (int j=0;j<_size;j++){
                vector[j] = null_value;
            }
            _size = 0;
            return true;
        }

        bool empty() {
            return _size == 0;
        }

        bool is_full() {
            return _size >= vector.size(); 
        }
        
        size_t capacity() { return vector.capacity(); }

        void setAllocator(Allocator &allocator){
            vector.setAllocator(allocator);
        }

    protected:
        Vector<T> vector;
        size_t _size = 0;
        int empty_pos = 0;
        T null_value;
};

}