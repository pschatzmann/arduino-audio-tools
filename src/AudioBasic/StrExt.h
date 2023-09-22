#pragma once

#include "AudioBasic/Str.h"

namespace audio_tools {

/**
 * @brief Str which keeps the data on the heap. We grow the allocated 
 * memory only if the copy source is not fitting.
 * 
 * While it should be avoided to use a lot of heap allocatioins in 
 * embedded devices it is sometimes more convinent to allocate a string
 * once on the heap and have the insurance that it might grow
 * if we need to process an unexpected size.
 * 
 * We also need to use this if we want to manage a vecor of strings.
 *  
 * @ingroup string
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class StrExt : public Str {

    public:
        StrExt() = default;

        StrExt(int initialAllocatedLength) : Str() {
            maxlen = initialAllocatedLength;
            is_const = false;
        }

        StrExt(Str &source) : Str() {
            set(source);
        }
        
        StrExt(StrExt &source) : Str(){
            set(source);
        }

        StrExt(const char* str) : Str() {
            if (str!=nullptr){
                len = strlen(str);
                maxlen = len; 
                grow(maxlen);
                if (chars!=nullptr){
                    strcpy(chars, str);
                }
            }
        }

        // move constructor
        StrExt (StrExt &&obj) = default;

        // move assignment
        StrExt& operator = (StrExt &&obj) {
            set(obj.c_str());
            return *this;
        }

        // copy assingment
        StrExt& operator = (StrExt &obj) {
            set(obj.c_str());
            return *this;
        };


        ~StrExt() {    
        }

        bool isOnHeap() override {
            return true;
        }
        
        bool isConst() override {
            return false;
        }

        void operator=(const char* str)  {
            set(str);
        }
    
        void operator=(char* str)  {
            set(str);
        }

        void operator=(int v)  {
            set(v);
        }

        void operator=(double v)  {
            set(v);
        }

        size_t capacity() {
            return maxlen;
        }

        void setCapacity(size_t newLen){
            grow(newLen);
        }
    
        // make sure that the max size is allocated
        void allocate(int len=-1) {
            int new_size = len<0?maxlen:len;
            grow(new_size);
            this->len = new_size;
        }

        /// assigns a memory buffer
        void copyFrom(const char *source, int len, int maxlen=0){
            this->maxlen = maxlen==0? len : maxlen;
            grow(this->maxlen);
            if (this->chars!=nullptr){
                this->len = len;
                this->is_const = false;
                memmove(this->chars, source, len);
                this->chars[len] = 0;                
            }
        }

        /// Fills the string with len chars 
        void setChars(char c, int len){
            grow(this->maxlen);
            if (this->chars!=nullptr){
                for (int j=0;j<len;j++){
                    this->chars[j]=c;
                }  
                this->len = len;
                this->is_const = false;
                this->chars[len]=0;
            }
        }

    protected:
        Vector<char> vector;

        bool grow(int newMaxLen){
            bool grown = false;
            assert(newMaxLen<1024*10);
            
            if (chars==nullptr || newMaxLen > maxlen ){
                LOGD("grow(%d)",newMaxLen);

                grown = true;
                // we use at minimum the defined maxlen
                int newSize = newMaxLen > maxlen ? newMaxLen : maxlen;                
                vector.resize(newSize+1);
                chars = &vector[0];
                maxlen = newSize;
                
            }
            return grown;
        }
};

}

