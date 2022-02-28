#pragma once

namespace audio_tools {

/**
 * @brief Vector implementation which provides the most important methods as defined by std::vector. This class it is quite handy 
 * to have and most of the times quite better then dealing with raw c arrays.
 *
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

    // default constructor
    inline Vector(int len = 20) {
      resize_internal(len, false);
    }

    // allocate size and initialize array
    inline Vector(int size, T value) {
      resize(size);
      for (int j=0;j< size;j++){
        data[j] = value;
      }
    }

    // copy constructor
    inline Vector( Vector<T> &copyFrom) {
      resize_internal(copyFrom.size(), false);
      for (int j=0;j<copyFrom.size();j++){
        data[j] = copyFrom[j];
      }
      this->len = copyFrom.size();
    }
    
    // legacy constructor with pointer range
    inline Vector(T *from, T *to) {
      this->len = to - from; 
      resize_internal(this->len, false);
      for (size_t j=0;j<this->len;j++){
        data[j] = from[j];
      }
    }

    inline  ~Vector() {
      clear();
      shrink_to_fit();
      delete [] this->data;
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
      data[len] = value;
      len++;
    }

    inline void push_front(T value){
      resize_internal(len+1, true);
      memmove(data,data+1,len*sizeof(T));
      data[0] = value;
      len++;
    }

    inline void pop_back(){
        if (len>0) {
          len--;
        }
    }

    inline void pop_front(){
        if (len>0) {
          len--;
          if (len>0){
            memmove(data, data+1,len*sizeof(T));
          }
        }
    }


    inline void assign(iterator v1, iterator v2) {
        size_t newLen = v2 - v1; 
        resize_internal(newLen, false);
        this->len = newLen;
        int pos = 0;
        for (auto ptr = v1; ptr != v2; ptr++) {
            data[pos++] = *ptr;
        }    
    }

    inline void assign(size_t number, T value) {
        resize_internal(number, false);
        this->len = number;
        for (int j=0;j<number;j++){
            data[j]=value;
        }
    }

    inline void swap(Vector<T> &in){
      // save data
      T *dataCpy = data;
      int bufferLenCpy = bufferLen;
      int lenCpy = len;
      // swap this
      data = in.data;
      len = in.len;
      bufferLen = in.bufferLen;
      // swp in
      in.data = dataCpy;
      in.len = lenCpy;
      in.bufferLen = bufferLenCpy;
    }

    inline T &operator[](int index) {
      return data[index];
    }

    inline Vector<T> &operator=(Vector<T> &copyFrom) {
      resize_internal(copyFrom.size(), false);
      for (int j=0;j<copyFrom.size();j++){
        data[j] = copyFrom[j];
      }
      this->len = copyFrom.size();
    }

    inline T &operator[] (const int index) const {
      return data[index];
    }

    inline bool resize(int newSize, T value){
      if (resize(newSize)){
        for (int j=0;j<newSize;j++){
          data[j]=value;
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
      return iterator(data, 0);
    }

    inline T& back(){
      return *iterator(data+(len-1), len-1);
    }

    inline iterator end(){
      return iterator(data+len, len);
    }

    // removes a single element
    inline void erase(iterator it) {
      int pos = it.pos();
      if (pos<len){
          int lenToEnd = len - pos - 1;
          // call destructor on data to be erased
          data[pos].~T();
          // shift values by 1 position
          memmove((void*) &data[pos],(void*)(&data[pos+1]),lenToEnd*sizeof(T));
          // make sure that we have a valid object at the end
          data[len-1] = T();
          len--;
      }
    }

  protected:
    int bufferLen;
    int len = 0;
    T *data = nullptr;

    inline void resize_internal(int newSize, bool copy, bool shrink=false)  {
      //bool withNewSize = false;
      if (newSize>bufferLen || this->data==nullptr ||shrink){
        //withNewSize = true;            
        T* oldData = data;
        int oldBufferLen = this->bufferLen;
        this->data = new T[newSize+1];
        this->bufferLen = newSize;  
        if (oldData != nullptr) {
          if(copy && this->len > 0){
              memcpy((void*)data,(void*) oldData, this->len*sizeof(T));
          }
          if (shrink){
            cleanup(oldData, newSize, oldBufferLen);
          }
          delete [] oldData;            
        }  
      }
    }

    void cleanup(T*data, int from, int to){
      for (int j=from;j<to;j++){
        data[j].~T();
      }
    }
};

}