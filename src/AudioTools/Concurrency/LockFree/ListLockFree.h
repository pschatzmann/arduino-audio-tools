#pragma once
#ifdef USE_INITIALIZER_LIST
#  include "InitializerList.h" 
#endif
#include <stddef.h>
#include <atomic>
#include <memory>
#include "AudioTools/CoreAudio/AudioBasic/Collections/Allocator.h"

namespace audio_tools {

/**
 * @brief Lock-free double linked list using atomic operations
 * @ingroup collections
 * @author Phil Schatzmann
 * @copyright GPLv3
 * @tparam T 
 * 
 * This implementation provides thread-safe operations without using locks.
 * It uses atomic pointers and compare-and-swap operations for synchronization.
 * Note: Some operations like size() may not be perfectly consistent in 
 * highly concurrent scenarios but will be eventually consistent.
 */
template <class T> 
class ListLockFree {
    public:
        struct Node {
            std::atomic<Node*> next{nullptr};
            std::atomic<Node*> prior{nullptr};
            T data;
            
            Node() = default;
            Node(const T& value) : data(value) {}
        };

        class Iterator {
            public:
                Iterator(Node* node) : node(node) {}
                
                inline Iterator operator++() {
                    if (node != nullptr && owner != nullptr) {
                        Node* next_node = node->next.load(std::memory_order_acquire);
                        if (next_node != nullptr && next_node != &owner->last) {
                            node = next_node;
                            is_eof = false;
                        } else {
                            // Advance onto the sentinel itself so that
                            // `it != owner->end()` (which compares by node
                            // pointer) actually becomes false - otherwise a
                            // for(it=begin(); it!=end(); ++it) loop never
                            // terminates once it reaches the last element.
                            node = &owner->last;
                            is_eof = true;
                        }
                    } else {
                        is_eof = true;
                    }
                    return *this;
                }
                
                inline Iterator operator++(int) {
                    Iterator tmp = *this;
                    ++(*this);
                    return tmp;
                }
                
                inline Iterator operator--() {
                    if (node != nullptr && owner != nullptr) {
                        Node* prior_node = node->prior.load(std::memory_order_acquire);
                        if (prior_node != nullptr && prior_node != &owner->first) {
                            node = prior_node;
                            is_eof = false;
                        } else {
                            // See operator++(): land on the sentinel itself
                            // so reverse iteration against rend() actually
                            // terminates instead of looping forever.
                            node = &owner->first;
                            is_eof = true;
                        }
                    } else {
                        is_eof = true;
                    }
                    return *this;
                }
                
                inline Iterator operator--(int) {
                    Iterator tmp = *this;
                    --(*this);
                    return tmp;
                }
                
                inline Iterator operator+(int offset) {
                    return getIteratorAtOffset(offset);
                }
                
                inline Iterator operator-(int offset) {
                    return getIteratorAtOffset(-offset);
                }
                
                inline bool operator==(const Iterator& it) const {
                    return node == it.node;
                }
                
                inline bool operator!=(const Iterator& it) const {
                    return node != it.node;
                }
                
                inline T& operator*() {
                    return node->data;
                }
                
                inline T* operator->() {
                    return &(node->data);
                }
                
                inline Node* get_node() {
                    return node;
                }
                
                inline operator bool() const {
                    return !is_eof;
                }

                void set_owner(ListLockFree* owner_ptr) {
                    owner = owner_ptr;
                }

            protected:
                Node* node = nullptr;
                bool is_eof = false;
                ListLockFree* owner = nullptr;

                Iterator getIteratorAtOffset(int offset) {
                    Node* tmp = node;
                    if (owner != nullptr) {
                        if (offset > 0) {
                            for (int j = 0; j < offset && tmp != nullptr; j++) {
                                Node* next_node = tmp->next.load(std::memory_order_acquire);
                                if (next_node == nullptr || next_node == &owner->last) {
                                    break;
                                }
                                tmp = next_node;
                            }
                        } else if (offset < 0) {
                            for (int j = 0; j < -offset && tmp != nullptr; j++) {
                                Node* prior_node = tmp->prior.load(std::memory_order_acquire);
                                if (prior_node == nullptr || prior_node == &owner->first) {
                                    break;
                                }
                                tmp = prior_node;
                            }
                        }
                    }
                    Iterator it(tmp);
                    it.set_owner(owner);
                    return it;
                }
        };

        /// Default constructor
        ListLockFree(Allocator &allocator = DefaultAllocator) : p_allocator(&allocator) { 
            link(); 
        }

        /// Copy constructor (not thread-safe for the source list)
        ListLockFree(const ListLockFree& ref) : p_allocator(ref.p_allocator) {
            link();
            // Copy elements (this is not thread-safe for the source)
            Node* current = ref.first.next.load(std::memory_order_acquire);
            while (current != &ref.last) {
                push_back(current->data);
                current = current->next.load(std::memory_order_acquire);
            }
        }

        /// Constructor using array
        template<size_t N>
        ListLockFree(const T (&a)[N], Allocator &allocator = DefaultAllocator) : p_allocator(&allocator) {
            link();
            for(int i = 0; i < N; ++i) {
                push_back(a[i]);
            }
        }

        ~ListLockFree() {
            clear();
            // Nothing can still be racing against a destructor call - drain
            // the recycle list and actually free the nodes now.
            Node* n = free_list.load(std::memory_order_relaxed);
            while (n != nullptr) {
                Node* next = n->next.load(std::memory_order_relaxed);
                releaseNode(n);
                n = next;
            }
        }

#ifdef USE_INITIALIZER_LIST
        ListLockFree(std::initializer_list<T> iniList, Allocator &allocator = DefaultAllocator) : p_allocator(&allocator) {
            link();
            for(const auto &obj : iniList) {
                push_back(obj);
            }
        } 
#endif        

        /// Swaps the contents of this list with ref by relinking the two
        /// chains' boundary pointers. Like the copy constructor, this is
        /// NOT safe against concurrent access on either list - callers must
        /// ensure exclusive access to both lists while swapping.
        bool swap(ListLockFree<T>& ref) {
            if (this == &ref) return true;

            Node* a_begin = first.next.load(std::memory_order_relaxed);
            Node* a_end   = last.prior.load(std::memory_order_relaxed);
            Node* b_begin = ref.first.next.load(std::memory_order_relaxed);
            Node* b_end   = ref.last.prior.load(std::memory_order_relaxed);
            bool a_empty  = (a_begin == &last);
            bool b_empty  = (b_begin == &ref.last);

            // Re-point this list's boundary at ref's chain.
            first.next.store(b_empty ? &last : b_begin, std::memory_order_relaxed);
            last.prior.store(b_empty ? &first : b_end, std::memory_order_relaxed);
            if (!b_empty) {
                b_begin->prior.store(&first, std::memory_order_relaxed);
                b_end->next.store(&last, std::memory_order_relaxed);
            }

            // Re-point ref's boundary at this list's original chain.
            ref.first.next.store(a_empty ? &ref.last : a_begin, std::memory_order_relaxed);
            ref.last.prior.store(a_empty ? &ref.first : a_end, std::memory_order_relaxed);
            if (!a_empty) {
                a_begin->prior.store(&ref.first, std::memory_order_relaxed);
                a_end->next.store(&ref.last, std::memory_order_relaxed);
            }

            size_t a_count = record_count.load(std::memory_order_relaxed);
            size_t b_count = ref.record_count.load(std::memory_order_relaxed);
            record_count.store(b_count, std::memory_order_relaxed);
            ref.record_count.store(a_count, std::memory_order_relaxed);

            return true;
        }

        bool push_back(const T& data) {
            Node* node = createNode();
            if (node == nullptr) return false;
            node->data = data;

            while (true) {
                Node* old_last_prior = last.prior.load(std::memory_order_acquire);
                
                // Try to link the new node
                node->next.store(&last, std::memory_order_relaxed);
                node->prior.store(old_last_prior, std::memory_order_relaxed);

                // Atomically update the prior node's next pointer
                Node* expected_next = &last;
                if (old_last_prior->next.compare_exchange_weak(
                    expected_next, node, std::memory_order_release, std::memory_order_relaxed)) {
                    
                    // Atomically update last's prior pointer
                    Node* expected_prior = old_last_prior;
                    if (last.prior.compare_exchange_weak(
                        expected_prior, node, std::memory_order_release, std::memory_order_relaxed)) {
                        
                        record_count.fetch_add(1, std::memory_order_relaxed);
                        return true;
                    }
                    
                    // Rollback the first change
                    old_last_prior->next.store(&last, std::memory_order_relaxed);
                }
            }
        }

        bool push_front(const T& data) {
            Node* node = createNode();
            if (node == nullptr) return false;
            node->data = data;

            while (true) {
                Node* old_first_next = first.next.load(std::memory_order_acquire);
                
                // Try to link the new node
                node->prior.store(&first, std::memory_order_relaxed);
                node->next.store(old_first_next, std::memory_order_relaxed);

                // Atomically update the next node's prior pointer
                Node* expected_prior = &first;
                if (old_first_next->prior.compare_exchange_weak(
                    expected_prior, node, std::memory_order_release, std::memory_order_relaxed)) {
                    
                    // Atomically update first's next pointer
                    Node* expected_next = old_first_next;
                    if (first.next.compare_exchange_weak(
                        expected_next, node, std::memory_order_release, std::memory_order_relaxed)) {
                        
                        record_count.fetch_add(1, std::memory_order_relaxed);
                        return true;
                    }
                    
                    // Rollback the first change
                    old_first_next->prior.store(&first, std::memory_order_relaxed);
                }
            }
        }

        bool insert(Iterator it, const T& data) {
            Node* node = createNode();
            if (node == nullptr) return false;
            node->data = data;

            Node* current_node = it.get_node();
            if (current_node == nullptr) return false;

            while (true) {
                Node* prior = current_node->prior.load(std::memory_order_acquire);
                if (prior == nullptr) return false;

                // Set up new node links
                node->prior.store(prior, std::memory_order_relaxed);
                node->next.store(current_node, std::memory_order_relaxed);

                // Try to atomically update the prior node's next pointer
                Node* expected_next = current_node;
                if (prior->next.compare_exchange_weak(
                    expected_next, node, std::memory_order_release, std::memory_order_relaxed)) {
                    
                    // Try to atomically update current node's prior pointer
                    Node* expected_prior = prior;
                    if (current_node->prior.compare_exchange_weak(
                        expected_prior, node, std::memory_order_release, std::memory_order_relaxed)) {
                        
                        record_count.fetch_add(1, std::memory_order_relaxed);
                        return true;
                    }
                    
                    // Rollback the prior->next change
                    prior->next.store(current_node, std::memory_order_relaxed);
                }
            }
        }

        bool pop_front() {
            T tmp;
            return pop_front(tmp);
        }

        bool pop_back() {
            T tmp;
            return pop_back(tmp);
        }

        bool pop_front(T& data) {
            while (true) {
                Node* first_data = first.next.load(std::memory_order_acquire);
                if (first_data == &last) return false; // Empty list

                Node* next_node = first_data->next.load(std::memory_order_acquire);
                data = first_data->data;

                // Try to atomically update the links
                if (first.next.compare_exchange_weak(
                    first_data, next_node, std::memory_order_release, std::memory_order_relaxed)) {
                    
                    if (next_node->prior.compare_exchange_weak(
                        first_data, &first, std::memory_order_release, std::memory_order_relaxed)) {
                        
                        deleteNode(first_data);
                        record_count.fetch_sub(1, std::memory_order_relaxed);
                        return true;
                    }
                    
                    // Rollback
                    first.next.store(first_data, std::memory_order_relaxed);
                }
            }
        }

        bool pop_back(T& data) {
            while (true) {
                Node* last_data = last.prior.load(std::memory_order_acquire);
                if (last_data == &first) return false; // Empty list

                Node* prior_node = last_data->prior.load(std::memory_order_acquire);
                data = last_data->data;

                // Try to atomically update the links
                if (last.prior.compare_exchange_weak(
                    last_data, prior_node, std::memory_order_release, std::memory_order_relaxed)) {
                    
                    if (prior_node->next.compare_exchange_weak(
                        last_data, &last, std::memory_order_release, std::memory_order_relaxed)) {
                        
                        deleteNode(last_data);
                        record_count.fetch_sub(1, std::memory_order_relaxed);
                        return true;
                    }
                    
                    // Rollback
                    last.prior.store(last_data, std::memory_order_relaxed);
                }
            }
        }

        bool erase(Iterator it) {
            Node* p_delete = it.get_node();
            if (p_delete == nullptr || p_delete == &first || p_delete == &last) {
                return false;
            }

            while (true) {
                Node* prior_node = p_delete->prior.load(std::memory_order_acquire);
                Node* next_node = p_delete->next.load(std::memory_order_acquire);
                
                if (prior_node == nullptr || next_node == nullptr) return false;

                // Try to atomically update the links
                if (prior_node->next.compare_exchange_weak(
                    p_delete, next_node, std::memory_order_release, std::memory_order_relaxed)) {
                    
                    if (next_node->prior.compare_exchange_weak(
                        p_delete, prior_node, std::memory_order_release, std::memory_order_relaxed)) {
                        
                        deleteNode(p_delete);
                        record_count.fetch_sub(1, std::memory_order_relaxed);
                        return true;
                    }
                    
                    // Rollback
                    prior_node->next.store(p_delete, std::memory_order_relaxed);
                }
            }
        }

        Iterator begin() {
            // For an empty list first.next == &last, so this matches end()
            // exactly and the usual `for (it = begin(); it != end(); ++it)`
            // idiom correctly skips the loop body instead of dereferencing
            // a null node.
            Node* first_data = first.next.load(std::memory_order_acquire);
            Iterator it(first_data);
            it.set_owner(this);
            return it;
        }

        Iterator end() {
            Iterator it(&last);
            it.set_owner(this);
            return it;
        }

        Iterator rbegin() {
            // See begin(): for an empty list last.prior == &first, matching
            // rend() so reverse iteration over an empty list is a no-op.
            Node* last_data = last.prior.load(std::memory_order_acquire);
            Iterator it(last_data);
            it.set_owner(this);
            return it;
        }

        Iterator rend() {
            Iterator it(&first);
            it.set_owner(this);
            return it;
        }

        size_t size() {
            return record_count.load(std::memory_order_relaxed);
        }

        bool empty() {
            return size() == 0;
        }

        bool clear() {
            while (pop_front()) {
                // Keep removing elements
            }
            return true;
        }

        inline T& operator[](int index) {
            // Note: This is not thread-safe and may give inconsistent results
            // in highly concurrent scenarios
            Node* n = first.next.load(std::memory_order_acquire);
            for (int j = 0; j < index && n != &last; j++) {
                n = n->next.load(std::memory_order_acquire);
                if (n == nullptr) {
                    // Fall back to this list's own dummy sentinel storage
                    // rather than a function-local static, which would be
                    // shared (and racily written to) by every instance.
                    return last.data;
                }
            }
            return n != &last ? n->data : last.data;
        }

        void setAllocator(Allocator& allocator) {
            p_allocator = &allocator;
        }

        /// Provides the last element
        T& back() {
            Node* last_data = last.prior.load(std::memory_order_acquire);
            return last_data != &first ? last_data->data : last.data;
        }

        /// Provides the first element
        T& front() {
            Node* first_data = first.next.load(std::memory_order_acquire);
            return first_data != &last ? first_data->data : first.data;
        }

    protected:
        Node first;  // empty dummy first node
        Node last;   // empty dummy last node
        std::atomic<size_t> record_count{0};
        Allocator* p_allocator = &DefaultAllocator;
        // Retired nodes are recycled here instead of being returned to the
        // allocator - see deleteNode()/createNode() for why.
        std::atomic<Node*> free_list{nullptr};

        Node* createNode() {
            // Prefer a recycled node over a fresh allocation: this keeps
            // unlinked-but-still-referenced nodes "type-stable" memory (see
            // deleteNode()).
            Node* head = free_list.load(std::memory_order_acquire);
            while (head != nullptr) {
                Node* next = head->next.load(std::memory_order_relaxed);
                if (free_list.compare_exchange_weak(
                        head, next, std::memory_order_acquire,
                        std::memory_order_relaxed)) {
                    head->next.store(nullptr, std::memory_order_relaxed);
                    head->prior.store(nullptr, std::memory_order_relaxed);
                    return head;
                }
            }
#if USE_ALLOCATOR
            Node* node = (Node*) p_allocator->allocate(sizeof(Node));
            if (node != nullptr) {
                new (node) Node(); // Placement new to call constructor
            }
#else
            Node* node = new Node();
#endif
            return node;
        }

        // Unlinking a node (pop/erase) races against any other thread that
        // read a pointer to it *before* it was unlinked and is about to
        // dereference/CAS through that stale pointer (see push_back()'s
        // `old_last_prior->next.compare_exchange_weak(...)` and the
        // equivalent spots in push_front()/erase()). Without hazard
        // pointers/epochs we cannot know when it's safe to actually free
        // the node, so instead of releasing it back to the allocator we
        // recycle it onto free_list: the memory stays a live, correctly
        // typed Node forever (as long as this list exists), so a racing
        // thread's stale access reads/CASes against recycled-but-valid
        // storage instead of freed/reused memory. A CAS through a stale
        // pointer then simply fails (the expected value no longer matches)
        // rather than corrupting unrelated heap memory. This does not
        // eliminate the classic ABA case (the node fully recycled and
        // re-linked before the stale CAS retries) - a complete fix needs
        // hazard pointers or epoch-based reclamation - but it removes the
        // undefined-behavior/use-after-free hazard of touching memory that
        // may have been returned to the allocator and reused for something
        // else entirely.
        void deleteNode(Node* p_delete) {
            if (p_delete == nullptr) return;
            Node* old_head = free_list.load(std::memory_order_relaxed);
            do {
                p_delete->next.store(old_head, std::memory_order_relaxed);
            } while (!free_list.compare_exchange_weak(
                old_head, p_delete, std::memory_order_release,
                std::memory_order_relaxed));
        }

        // Actually releases a node's memory - only safe once no other
        // thread can possibly still be operating on the list (e.g. from
        // the destructor).
        void releaseNode(Node* p_delete) {
#if USE_ALLOCATOR
            if (p_delete != nullptr) {
                p_delete->~Node(); // Explicit destructor call
                p_allocator->free(p_delete);
            }
#else
            delete p_delete;
#endif
        }

        void link() {
            first.next.store(&last, std::memory_order_relaxed);
            last.prior.store(&first, std::memory_order_relaxed);
        }

        friend class Iterator;
};

}
