#pragma once
#include "AudioTools/CoreAudio/AudioBasic/Collections/List.h"

namespace audio_tools {

/**
 * @brief FIFO Queue which is based on a List
 * @ingroup collections
 * @author Phil Schatzmann
 * @copyright GPLv3
 * @tparam T 
 */
template <class T> 
class Queue {
    public:
        Queue() = default;

        bool enqueue(T& data){
            return l.push_front(data);
        }

        bool peek(T& data){
            if (l.empty()) return false;
            data = l.back();
            return true;
        }

        bool dequeue(T& data){
            return l.pop_back(data);
        }

        size_t size() {
            return l.size();
        }

        bool clear() {
            return l.clear();
        }

        bool empty() {
            return l.empty();
        }

        void setAllocator(Allocator &allocator){
            l.setAllocator(allocator);
        }

    protected:
        List<T> l;
};

}