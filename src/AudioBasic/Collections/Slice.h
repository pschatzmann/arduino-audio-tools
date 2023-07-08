#pragma once
#include <stddef.h>
#include <stdint.h>

namespace audio_tools {

/**
 * @brief Helps to split up a big memory array into smaller slices. There are no
 * additinal heap allocations!
 * @ingroup collections
 * @author Phil Schatzmann
 */
template <class T> 
class Slice {
 public:
  Slice(T* start, size_t len) {
    this->start = start;
    this->len = len;
  }

  /// Returns the data
  T* data() { return start; }

  /// Returns the (result) data size in bytes
  size_t size() { return len; }

  /// Returns the number of slices
  size_t slices(int sliceSize){
    int result = len / sliceSize;
    return len % sliceSize == 0 ? result : result+1;
  }

  operator bool() { return len > 0; }

  /// Returns the slice at the indicated index;
  Slice slice(int sliceSize, int idx) {
    int start_pos = idx * sliceSize;
    int end_pos = start_pos + sliceSize;
    if (end_pos > len) {
      end_pos = len;
    }

    if (start_pos < len) {
      assert(start!=nullptr);
      return Slice(start + start_pos, end_pos - start_pos);
    } else {
      return Slice(nullptr, 0);
    }
  }

 protected:
  T* start = nullptr;
  size_t len = 0;
};

}  // namespace audio_tools