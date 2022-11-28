#pragma once
/**
 * @file NoArduino.h
 * @author Phil Schatzmann
 * @brief If you want to use the framework w/o Arduino you need to provide the implementation
 * of a couple of classes and methods!
 * @version 0.1
 * @date 2022-09-19
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include <stdint.h>
#include <algorithm>    // std::max
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <chrono>

#define PSTR(fmt) fmt
#define PI 3.14159265359f

using namespace std;

enum PrintCharFmt {DEC, HEX};

namespace audio_tools {

class Print {
public:
    virtual size_t write(uint8_t ch){
		return write(&ch, 1);
	}

    virtual size_t write(const char *str) {
      return write((const uint8_t *)str, strlen(str));
    }

    virtual size_t write(const uint8_t *buffer, size_t size){
        if (buffer == nullptr) return 0;
    	for (size_t j=0;j<size;j++){
    		write(buffer[j]);
    	}
    	return size;
    }

    virtual size_t write(const char *buffer, size_t size) {
      return write((const uint8_t *)buffer, size);
    }

    // default to zero, meaning "a single write may block"
    // should be overridden by subclasses with buffering
    virtual int availableForWrite() { return 1024; }


    virtual int print(const char* msg){
		int result = strlen(msg);
		return write(msg, result);
    }

	virtual int println(const char* msg="") {
		int result = print(msg);
		write('\n');
		return result+1;
	}

	virtual int print(int number){
		char buffer[80];
		snprintf(buffer,80,"%d", number);
		return print(buffer);
	}

	virtual int print(char c, PrintCharFmt spec){
		char result[3];
		switch(spec){
		case DEC:
			 snprintf(result, 3,"%c", c);
			 return print(result);
		case HEX:
			 snprintf(result, 3,"%x", c & 0xff);
			 return print(result);
		}
		return -1;
	}

	virtual void flush() { /* Empty implementation for backward compatibility */ }

};

class Stream : public Print {
public:
	virtual int available(){return 0;}
	virtual size_t readBytes(uint8_t* buffer, size_t len) {return 0;}
	virtual int read(){return -1;}
	virtual int peek(){return -1;}
	virtual void setTimeout(size_t t){}
};

class Client : public Stream {
public:
	void stop();
	virtual int read(uint8_t* buffer, size_t len);
	virtual int read();
	bool connected();
	bool connect(const char* ip, int port);
	virtual operator bool();
};

class HardwareSerial : public Stream {
public:
	virtual operator bool() {
        return true;
    }
    virtual size_t write(uint8_t ch) {
        putchar(ch);
        return 1;
    }
    virtual size_t write(const uint8_t *buffer, size_t size){
        for (int j=0;j<size;j++){
            write(buffer[j]);
        }
        return size;
    }
};

static HardwareSerial Serial;

/// Waits for the indicated milliseconds
void delay(uint64_t ms) {
    //std::this_thread::sleep_for(std::chrono::milliseconds(ms));    
}

/// Returns the milliseconds since the start
uint64_t millis(){
    using namespace std::chrono;
    // Get current time with precision of milliseconds
    auto now = time_point_cast<milliseconds>(system_clock::now());
    // sys_milliseconds is type time_point<system_clock, milliseconds>
    using sys_milliseconds = decltype(now);
    // Convert time_point to signed integral type
    return now.time_since_epoch().count();
}

/// Maps input to output values
inline long map(long x, long in_min, long in_max, long out_min, long out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}



} // namespace

using namespace audio_tools;




