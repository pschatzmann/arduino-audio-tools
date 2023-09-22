#pragma once
#ifdef USE_INITIALIZER_LIST
#  include "InitializerList.h" 
#endif
#include <assert.h>

namespace audio_tools {

/**
 * @brief Vector implementation which provides the most important methods as defined by std::vector. This class it is quite handy 
 * to have and most of the times quite better then dealing with raw c arrays.
 * @ingroup collections
 * @author Phil Schatzmann
 * @copyright GPLv3
 **/

template <class T> 
class Vector {
  public:
  /**
   *   @brief Iterator for the Vector class
   *
   *   by Phil Schatzmann
   **/

    class iterator {
      protected:
        T *ptr;
        size_t pos_;
      public:
        inline iterator(){
        }
        inline iterator(T* parPtr, size_t pos){
          this->ptr = parPtr;
          this->pos_ = pos;
        }
        // copy constructor
        inline iterator(const iterator &copyFrom){
          ptr = copyFrom.ptr;
          pos_ = copyFrom.pos_;
        }
        inline iterator operator++(int n) {
          ptr++;
          pos_++;
          return *this;
        }
        inline iterator operator++() {
          ptr++;
          pos_++;
          return *this;
        }
        inline iterator operator--(int n) {
          ptr--;
          pos_--;
          return *this;
        }
        inline iterator operator--() {
          ptr--;
          pos_--;
          return *this;
        }
        inline iterator operator+(int offset) {
          pos_ += offset;
          return iterator(ptr+offset, offset);
        }
        inline bool operator==(iterator it) {
          return ptr == it.getPtr();
        }
        inline bool operator<(iterator it) {
          return ptr < it.getPtr();
        }
        inline bool operator<=(iterator it) {
          return ptr <= it.getPtr();
        }
        inline bool operator>(iterator it) {
          return ptr > it.getPtr();
        }
        inline bool operator>=(iterator it) {
          return ptr >= it.getPtr();
        }
        inline bool operator!=(iterator it) {
          return ptr != it.getPtr();
        }
        inline T &operator*() {
          return *ptr;
        }
        inline T *operator->() {
          return ptr;
        }
        inline T *getPtr() {
          return ptr;
        }
        inline size_t pos() {
          return pos_;
        }
        inline size_t operator-(iterator it) {
          return (ptr - it.getPtr());
        }

    };

#ifdef USE_INITIALIZER_LIST

    /// support for initializer_list
    inline Vector(std::initializer_list<T> iniList) {
      resize(iniList.size());
      int pos = 0;
      for (auto &obj : iniList){
        p_data[pos++] = obj;
      }
    } 

#endif

    /// default constructor
    inline Vector(size_t len = 20) {
      resize_internal(len, false);
    }

    /// allocate size and initialize array
    inline Vector(int size, T value) {
      resize(size);
      for (int j=0;j< size;j++){
        p_data[j] = value;
      }
    }

    inline Vector(Vector<T> &&moveFrom) = default;


    /// copy constructor
    inline Vector(Vector<T> &copyFrom) {
      resize_internal(copyFrom.size(), false);
      for (int j=0;j<copyFrom.size();j++){
        p_data[j] = copyFrom[j];
      }
      this->len = copyFrom.size();
    }

    /// legacy constructor with pointer range
    inline Vector(T *from, T *to) {
      this->len = to - from; 
      resize_internal(this->len, false);
      for (size_t j=0;j<this->len;j++){
        p_data[j] = from[j];
      }
    }

    /// Destructor
    virtual  ~Vector() {
      clear();
      shrink_to_fit();
      delete [] this->p_data;
    }

    inline void clear() {
      len = 0;
    }
    
    inline int size() {
      return len;
    }
    
    inline bool empty() {
        return size()==0;
    }

    inline void push_back(T value){
      resize_internal(len+1, true);
      p_data[len] =  value;
      len++;
    }

    void push_front(T value){
      resize_internal(len+1, true);
      //memmove(p_data,p_data+1,len*sizeof(T));
      for (int j=len; j >= 0; j--){
          p_data[j+1] = p_data[j];
      }
      p_data[0] = value;
      len++;
    }

    inline void pop_back(){
        if (len>0) {
          len--;
        }
    }

    inline void pop_front(){
        erase(0);          
    }


    inline void assign(iterator v1, iterator v2) {
        size_t newLen = v2 - v1; 
        resize_internal(newLen, false);
        this->len = newLen;
        int pos = 0;
        for (auto ptr = v1; ptr != v2; ptr++) {
            p_data[pos++] = *ptr;
        }    
    }

    inline void assign(size_t number, T value) {
        resize_internal(number, false);
        this->len = number;
        for (int j=0;j<number;j++){
            p_data[j]=value;
        }
    }

    inline void swap(Vector<T> &in){
      // save data
      T *dataCpy = p_data;
      int bufferLenCpy = bufferLen;
      int lenCpy = len;
      // swap this
      p_data = in.p_data;
      len = in.len;
      bufferLen = in.bufferLen;
      // swp in
      in.p_data = dataCpy;
      in.len = lenCpy;
      in.bufferLen = bufferLenCpy;
    }

    inline T &operator[](int index) {
      assert(p_data!=nullptr);
      return p_data[index];
    }

    inline Vector<T> &operator=(Vector<T> &copyFrom) {
      resize_internal(copyFrom.size(), false);
      for (int j=0;j<copyFrom.size();j++){
        p_data[j] = copyFrom[j];
      }
      this->len = copyFrom.size();
      return *this;
    }

    inline T &operator[] (const int index) const {
      return p_data[index];
    }

    inline bool resize(int newSize, T value){
      if (resize(newSize)){
        for (int j=0;j<newSize;j++){
          p_data[j]=value;
        }
        return true;
      }
      return false;
    }

    inline void shrink_to_fit() {
        resize_internal(this->len, true, true);
    }

    int capacity(){
      return this->bufferLen;
    }

    inline bool resize(int newSize){
        int oldSize = this->len;
        resize_internal(newSize, true);
        this->len = newSize;        
        return this->len!=oldSize;
    }
                       
    inline iterator begin(){
      return iterator(p_data, 0);
    }

    inline T& back(){
      return *iterator(p_data+(len-1), len-1);
    }

    inline iterator end(){
      return iterator(p_data+len, len);
    }

    // removes a single element
    inline void erase(iterator it) {
      return erase(it.pos());
    }

    // removes a single element
    inline void erase(int pos) {
      if (pos<len){
          int lenToEnd = len - pos - 1;
          // call destructor on data to be erased
          p_data[pos].~T();
          // shift values by 1 position
          //memmove((void*) &p_data[pos],(void*)(&p_data[pos+1]),lenToEnd*sizeof(T));
          for (int j=pos; j<len; j++){
              p_data[j] = p_data[j+1];
          }
          
          // make sure that we have a valid object at the end
          p_data[len-1] = T();
          len--;
      }
    }

    T* data(){
      return p_data;
    }

    operator bool() const {
      return p_data!=nullptr;
    }

  protected:
    int bufferLen=0;
    int len = 0;
    T *p_data = nullptr;

    inline void resize_internal(int newSize, bool copy, bool shrink=false)  {
      if (newSize<=0) return;
      //bool withNewSize = false;
      if (newSize>bufferLen || this->p_data==nullptr ||shrink){
        //withNewSize = true;            
        T* oldData = p_data;
        int oldBufferLen = this->bufferLen;
        this->p_data = new T[newSize+1];
        this->bufferLen = newSize;  
        if (oldData != nullptr) {
          if(copy && this->len > 0){
              memcpy((void*)p_data,(void*) oldData, this->len*sizeof(T));
          }
          if (shrink){
            cleanup(oldData, newSize, oldBufferLen);
          }
          delete [] oldData;            
        }  
      }
      assert(p_data!=nullptr);
    }

    void cleanup(T*data, int from, int to){
      for (int j=from;j<to;j++){
        data[j].~T();
      }
    }
};

}