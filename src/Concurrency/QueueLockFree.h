
#pragma once
#include <stdint.h>

#include <atomic>
#include <cstddef>

namespace audio_tools {

/**
 * @brief A simple single producer, single consumer lock free queue
 */
template <typename T>
class QueueLockFree {
 public:
  QueueLockFree(size_t capacity, Allocator& allocator = DefaultAllocator) {
    setAllocator(allocator);
    resize(capacity);
  }

  ~QueueLockFree() {
    for (size_t i = head_pos; i != tail_pos; ++i)
      (&p_node[i & capacity_mask].data)->~T();

    delete[] (char*)p_node;
  }

  void setAllocator(Allocator& allocator) { vector.setAllocator(allocator); }

  void resize(size_t capacity) {
    capacity_mask = capacity - 1;
    for (size_t i = 1; i <= sizeof(void*) * 4; i <<= 1)
      capacity_mask |= capacity_mask >> i;
    capacity_value = capacity_mask + 1;

    vector.resize(capacity);
    p_node = vector.data();

    for (size_t i = 0; i < capacity; ++i) {
      p_node[i].tail.store(i, std::memory_order_relaxed);
      p_node[i].head.store(-1, std::memory_order_relaxed);
    }

    tail_pos.store(0, std::memory_order_relaxed);
    head_pos.store(0, std::memory_order_relaxed);
  }

  size_t capacity() const { return capacity_value; }

  size_t size() const {
    size_t head = head_pos.load(std::memory_order_acquire);
    return tail_pos.load(std::memory_order_relaxed) - head;
  }

  bool enqueue(const T&& data) {
    return enqueue(data);
  }

  bool enqueue(const T& data) {
    Node* node;
    size_t tail = tail_pos.load(std::memory_order_relaxed);
    for (;;) {
      node = &p_node[tail & capacity_mask];
      if (node->tail.load(std::memory_order_relaxed) != tail) return false;
      if ((tail_pos.compare_exchange_weak(tail, tail + 1,
                                          std::memory_order_relaxed)))
        break;
    }
    new (&node->data) T(data);
    node->head.store(tail, std::memory_order_release);
    return true;
  }

  bool dequeue(T& result) {
    Node* node;
    size_t head = head_pos.load(std::memory_order_relaxed);
    for (;;) {
      node = &p_node[head & capacity_mask];
      if (node->head.load(std::memory_order_relaxed) != head) return false;
      if (head_pos.compare_exchange_weak(head, head + 1,
                                         std::memory_order_relaxed))
        break;
    }
    result = node->data;
    (&node->data)->~T();
    node->tail.store(head + capacity_value, std::memory_order_release);
    return true;
  }

  void clear() {
    T tmp;
    while (dequeue(tmp));
  }

 protected:
  struct Node {
    T data;
    std::atomic<size_t> tail;
    std::atomic<size_t> head;
  };

  Node* p_node;
  size_t capacity_mask;
  size_t capacity_value;
  std::atomic<size_t> tail_pos;
  std::atomic<size_t> head_pos;
  Vector<Node> vector;
};
}  // namespace audio_tools