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
#ifndef PI
#  define PI 3.14159265359f
#endif

using namespace std;

enum PrintCharFmt {DEC, HEX};

namespace audio_tools {

class Print {
public:
    virtual size_t write(uint8_t ch){
		// not implememnted: to be overritten
		return 0;
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
	operator bool(){
		return true;
	}
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
    size_t write(uint8_t ch) override {
		return putchar(ch);
	}
	virtual operator bool() {
        return true;
    }
};

static HardwareSerial Serial;

/// Waits for the indicated milliseconds
extern void delay(uint64_t ms);

/// Returns the milliseconds since the start
extern uint64_t millis();

/// Maps input to output values
inline long map(long x, long in_min, long in_max, long out_min, long out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}


} // namespace

using namespace audio_tools;




