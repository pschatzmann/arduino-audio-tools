#pragma once
#ifdef USE_INITIALIZER_LIST
#  include "InitializerList.h" 
#endif
#include <stddef.h>

namespace audio_tools {

/**
 * @brief Double linked list
 * @ingroup collections
 * @author Phil Schatzmann
 * @copyright GPLv3
 * @tparam T 
 */
template <class T> 
class List {
    public:
        struct Node {
            Node* next = nullptr;
            Node* prior = nullptr;
            T data;

            Node() = default;
        };

        class Iterator {
            public:
                Iterator(Node*node){
                    this->node = node;
                }
                inline Iterator operator++() {
                    if (node->next!=nullptr){
                        node = node->next;
                        is_eof = false;
                    } else is_eof=true;
                    return *this;
                }
                inline Iterator operator++(int) {
                    return ++*this;
                }
                inline Iterator operator--() {
                    if (node->prior!=nullptr){
                        node = node->prior;
                        is_eof = false;
                    } else is_eof=true;
                    return *this;
                }
                inline Iterator operator--(int) {
                    return --*this;
                }
                inline Iterator operator+(int offset) {
                    return getIteratorAtOffset(offset);
                }
                inline Iterator operator-(int offset) {
                    return getIteratorAtOffset(-offset);
                }
                inline bool operator==(Iterator it) {
                    return node == it.get_node();
                }
                inline bool operator!=(Iterator it) {
                    return node != it.get_node();
                }
                inline T &operator*() {
                    return node->data;
                }
                inline T *operator->() {
                    return &(node->data);
                }
                inline Node *get_node() {
                    return node;
                }
                inline operator bool() {
                    return is_eof;
                }
            protected:
                Node* node=nullptr;
                bool is_eof = false;

                Iterator getIteratorAtOffset(int offset){
                    Node *tmp = node;
                    if (offset>0){
                        for (int j=0;j<offset;j++){
                            if (tmp->next==nullptr){
                                return Iterator(tmp);
                            }
                            tmp = tmp->next;
                        }
                    } else if (offset<0){
                        for (int j=0;j<-offset;j++){
                            if (tmp->prior==nullptr){
                                return Iterator(tmp);
                            }
                            tmp = tmp->prior;
                        }
                    }
                    Iterator it(tmp);
                    return it;
                }

        };

        /// Default constructor
        List() { link(); };
        /// copy constructor
        List(List&ref) = default;

        /// Constructor using array
        template<size_t N>
        List(const T (&a)[N]) {
            link();
            for(int i = 0; i < N; ++i) 
  	    	    push_back(a[i]);
     	}

#ifdef USE_INITIALIZER_LIST

        List(std::initializer_list<T> iniList) {
            link();
            for(auto &obj : iniList) 
  	    	    push_back(obj);
        } 
#endif        
        bool swap(List<T>&ref){
            List<T> tmp(*this);
            validate();

            first = ref.first;
            last = ref.last;
            record_count = ref.record_count;

            ref.first = tmp.first;
            ref.last = tmp.last;
            ref.record_count = tmp.record_count;

            validate();
            return true;
        }

        bool push_back(T data){
            Node *node = new Node();
            if (node==nullptr) return false;
            node->data = data;

            // update links
            Node *old_last_prior = last.prior;
            node->next = &last;
            node->prior = old_last_prior;
            old_last_prior->next = node;
            last.prior = node;

            record_count++;
            validate();
            return true;
        }

        bool push_front(T data){
            Node *node = new Node();
            if (node==nullptr) return false;
            node->data = data;

            // update links
            Node *old_begin_next = first.next;
            node->prior = &first;
            node->next = old_begin_next;
            old_begin_next->prior = node;
            first.next = node;

            record_count++;
            validate();
            return true;
        }

        bool insert(Iterator it, const T& data){
            Node *node = new Node();
            if (node==nullptr) return false;
            node->data = data;

            // update links
            Node *current_node = it.get_node();
            Node *prior = current_node->prior;

            prior->next = node;
            current_node->prior = node;
            node->prior = prior;
            node->next = current_node;

            record_count++;
            validate();
            return true;
        }


        bool pop_front(){
            T tmp;
            return pop_front(tmp);
        }

        bool pop_back(){
            T tmp;
            return pop_back(tmp);
        }

        bool pop_front(T &data){
            if (record_count==0) return false;
            // get data
            Node *p_delete = firstDataNode();
            Node *p_prior = p_delete->prior;
            Node *p_next = p_delete->next;

            data = p_delete->data;

            // remove last node
            p_prior->next = p_next;
            p_next->prior = p_prior;

            delete p_delete;
            record_count--;    

            validate();
            return true;
        }

        bool pop_back(T &data){
            if (record_count==0) return false;
            Node *p_delete = lastDataNode();
            Node *p_prior = p_delete->prior;
            Node *p_next = p_delete->next;

            // get data
            data = p_delete->data;

            // remove last node
            p_prior->next = p_next;
            p_next->prior = p_prior;

            delete p_delete;
            record_count--;

            validate();
            return true;
        }

        bool erase (Iterator it){
            Node *p_delete = it.get_node();
            // check for valid iterator 
            if (empty() || p_delete==&first || p_delete==&last){
                return false;
            }
            Node *p_prior = p_delete->prior;
            Node *p_next = p_delete->next;

            // remove last node
            p_prior->next = p_next;
            p_next->prior = p_prior;

            delete p_delete;
            record_count--;    
            return true;
        }


        Iterator begin() {
            Iterator it(firstDataNode());
            return it;
        }

        Iterator end(){
             Iterator it(&last);
             return it;
        }

        Iterator rbegin() {
            Iterator it(lastDataNode());
            return it;
        }

        Iterator rend(){
            Iterator it(&first);
            return it;
        }

        size_t size() {
            return record_count;
        }

        bool empty() {
            return size()==0;
        }

        bool clear() {
            while(pop_front())
                ;
            validate();
            return true;
        }

        inline T &operator[](int index) {
            Node *n = firstDataNode();
            for (int j=0;j<index;j++){
                n = n->next;
                if (n==nullptr){
                    return last.data;
                }
            }
            return n->data;
        }


    protected:
        Node first; // empty dummy first node which which is always before the first data node 
        Node last; // empty dummy last node which which is always after the last data node 
        size_t record_count=0;

        void link(){
            first.next = &last;
            last.prior = &first;
        }

        Node* lastDataNode(){
            return last.prior;
        }
        Node* firstDataNode(){
            return first.next;
        }

        void validate() {
            assert(first.next!=nullptr);
            assert(last.prior!=nullptr);
            if (empty()){
                assert(first.next = &last);
                assert(last.prior = &first);
            }
        }

};

}