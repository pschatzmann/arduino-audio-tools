#pragma once
#include <stdint.h>
#include "Vector.h"

/**
 * @brief Space optimized vector which stores the boolean values as bits
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class BitVector {
  public:
    BitVector(){
    }

    inline bool operator[](int index) {
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

    void set(int index, bool value){
        int offset = index / sizeof(uint64_t);
        int bit = index % sizeof(uint64_t);
        while(offset>vector.size()){
            vector.push_back(0l);
        }
        if (value) {
            // set bit
            vector[offset] |= 1UL << bit;
        } else {
            // clear bit
            vector[offset] &= ~(1UL << bit);
        }
    }

    void clear() {
        vector.clear();
        vector.shrink_to_fit();
    }

  protected:
    Vector<uint64_t> vector;

};