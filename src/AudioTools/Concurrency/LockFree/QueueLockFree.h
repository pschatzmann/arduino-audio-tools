
#pragma once
#include <stdint.h>

#include <atomic>
#include <cstddef>
#include <utility>

namespace audio_tools {

/**
 * @brief Lock-free MPMC queue.
 *
 * T must be move-constructible; default-constructibility is NOT required
 * (element storage is raw bytes — no T object is created until enqueue).
 */
template <typename T>
class QueueLockFree {
 public:
  QueueLockFree(size_t capacity, Allocator& allocator = DefaultAllocator) {
    setAllocator(allocator);
    resize(capacity);
  }

  ~QueueLockFree() {
    size_t head = head_pos.load(std::memory_order_acquire);
    size_t tail = tail_pos.load(std::memory_order_acquire);
    for (size_t i = head; i != tail; ++i)
      p_node[i & capacity_mask].ptr()->~T();
  }

  void setAllocator(Allocator& allocator) { vector.setAllocator(allocator); }

  bool resize(size_t capacity) {
    if (capacity == 0) capacity = 1;

    // Destroy any live elements in the current queue before reinitialising.
    if (p_node) {
      size_t head = head_pos.load(std::memory_order_relaxed);
      size_t tail = tail_pos.load(std::memory_order_relaxed);
      for (size_t i = head; i != tail; ++i)
        p_node[i & capacity_mask].ptr()->~T();
    }

    // Round capacity up to the next power of two so that the bitmask index
    // wrapping always stays within the allocated array.
    capacity_mask = capacity - 1;
    for (size_t i = 1; i <= sizeof(void*) * 4; i <<= 1)
      capacity_mask |= capacity_mask >> i;
    capacity_value = capacity_mask + 1;

    vector.resize(capacity_value);
    p_node = vector.data();

    for (size_t i = 0; i < capacity_value; ++i) {
      p_node[i].tail.store(i, std::memory_order_relaxed);
      p_node[i].head.store(size_t(-1), std::memory_order_relaxed);
    }

    tail_pos.store(0, std::memory_order_relaxed);
    head_pos.store(0, std::memory_order_relaxed);
    return true;
  }

  size_t capacity() const { return capacity_value; }

  size_t availableForWrite() const {
    size_t head = head_pos.load(std::memory_order_seq_cst);
    size_t tail = tail_pos.load(std::memory_order_seq_cst);
    return capacity_value - (tail - head);
  } 

  bool empty() const { return size() == 0; }

  size_t size() const {
    size_t head = head_pos.load(std::memory_order_seq_cst);
    size_t tail = tail_pos.load(std::memory_order_seq_cst);
    return tail - head;
  }

  bool enqueue(const T& data) { return emplace(data); }
  bool enqueue(T&& data)      { return emplace(std::move(data)); }

  bool dequeue(T& result) {
    Node* node;
    size_t head = head_pos.load(std::memory_order_relaxed);
    for (;;) {
      node = &p_node[head & capacity_mask];
      // acquire pairs with enqueue's release store on node->head,
      // ensuring node->storage is visible before we read it.
      if (node->head.load(std::memory_order_acquire) != head) return false;
      if (head_pos.compare_exchange_weak(head, head + 1,
                                         std::memory_order_relaxed))
        break;
    }
    result = std::move(*node->ptr());
    node->ptr()->~T();
    node->tail.store(head + capacity_value, std::memory_order_release);
    return true;
  }

  void clear() {
    size_t head = head_pos.load(std::memory_order_acquire);
    size_t tail = tail_pos.load(std::memory_order_acquire);
    for (size_t i = head; i != tail; ++i) {
      Node* node = &p_node[i & capacity_mask];
      node->ptr()->~T();
      node->tail.store(i + capacity_value, std::memory_order_release);
    }
    head_pos.store(tail, std::memory_order_release);
  }

 protected:
  struct Node {
    alignas(T) unsigned char storage[sizeof(T)];
    std::atomic<size_t> tail;
    std::atomic<size_t> head;
    T*       ptr()       { return reinterpret_cast<T*>(storage); }
    const T* ptr() const { return reinterpret_cast<const T*>(storage); }
  };

  // Single enqueue implementation for both lvalue and rvalue paths.
  template <typename U>
  bool emplace(U&& val) {
    Node* node;
    size_t tail = tail_pos.load(std::memory_order_relaxed);
    for (;;) {
      node = &p_node[tail & capacity_mask];
      // acquire pairs with dequeue's release store on node->tail,
      // ensuring ~T() in the consumer is complete before we reuse the slot.
      if (node->tail.load(std::memory_order_acquire) != tail) return false;
      if (tail_pos.compare_exchange_weak(tail, tail + 1,
                                         std::memory_order_relaxed))
        break;
    }
    new (node->ptr()) T(std::forward<U>(val));
    node->head.store(tail, std::memory_order_release);
    return true;
  }

  Node*  p_node = nullptr;
  size_t capacity_mask  = 0;
  size_t capacity_value = 0;
  std::atomic<size_t> tail_pos{0};
  std::atomic<size_t> head_pos{0};
  Vector<Node> vector;
};

}  // namespace audio_tools
