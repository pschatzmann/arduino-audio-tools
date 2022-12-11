#pragma once
#include <stdint.h>
#include "Vector.h"

namespace audio_tools {

/**
 * @brief Space optimized vector which stores the boolean values as bits
 * @ingroup collections
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class BitVector {
  public:
    BitVector() = default;

    inline bool operator[](size_t index) {
        return get(index);
    }

    bool get(size_t index){
        bool result = false;
        int offset = index / sizeof(uint64_t);
        int bit = index % sizeof(uint64_t);
        if (offset < vector.size()){
            uint64_t value = vector[offset];
            // get bit
            result = (value >> bit) & 1U;
        }
        return result;
    }

    void set(size_t index, bool value){
        max_idx = max(max_idx, index+1);
        int offset = index / sizeof(uint64_t);
        int bit = index % sizeof(uint64_t);
        while(offset>=vector.size()){
            vector.push_back(0l);
        }
        // check if we need to updte the value
        if (((vector[offset] >> bit) & 1U) != value) {
            // needs update
            if (value) {
                // set bit
                vector[offset] |= 1UL << bit;
            } else {
                // clear bit
                vector[offset] &= ~(1UL << bit);
            }
            // call change handler to notify about change
            if (changeHandler!=nullptr){
                changeHandler(index, value, ref);
            }
        }
    }

    void clear() {
        max_idx = 0;
        vector.clear();
        vector.shrink_to_fit();
    }

    int size() {
        return max_idx;
    }

    /// Defines a callback method which is called when a value is updated
    void setChangeCallback(void (*hdler)(int idx, bool value, void*ref), void*ref=nullptr){
        changeHandler = hdler;
        this->ref = ref;
    }

  protected:
    Vector<uint64_t> vector;
    void (*changeHandler)(int idx, bool value, void*ref) = nullptr;
    void *ref;
    size_t max_idx=0;

};

}