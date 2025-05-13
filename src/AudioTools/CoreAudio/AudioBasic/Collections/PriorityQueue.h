#pragma once
#include "AudioTools/CoreAudio/AudioBasic/Collections/List.h"

namespace audio_tools {

/**
 * @brief Priority Queue which is based on a List. The order of the elements is
 * defined by a compare function which is provided in the constructor. If the
 * function returns > 0 if v1 > v2, the data will be provided in increasing
 * order.
 * @ingroup collections
 * @author Phil Schatzmann
 * @copyright GPLv3
 * @tparam T
 */
template <class T>
class PriorityQueue {
 public:
  PriorityQueue(int (*compare)(T &v1, T &v2)) { compare_cb = compare; };

  bool enqueue(T &&data) {
    for (auto it = l.begin(); it != l.end(); ++it) {
      if (compare_cb(*it, data) > 0) {
        l.insert(it, data);
        return true;
      }
    }
    return l.push_back(data);
  }

  bool peek(T &data) {
    if (l.end()->prior == nullptr) return false;
    data = *(l.end()->prior);
    return true;
  }

  bool dequeue(T &data) { return l.pop_front(data); }

  size_t size() { return l.size(); }

  bool clear() { return l.clear(); }

  bool empty() { return l.empty(); }

  void setAllocator(Allocator &allocator) { l.setAllocator(allocator); }

 protected:
  List<T> l;
  int (*compare_cb)(T &v1, T &v2);
};

}  // namespace audio_tools