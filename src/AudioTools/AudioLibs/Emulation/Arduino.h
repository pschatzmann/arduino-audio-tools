#pragma once
/**
 * @file Arduino.h
 * @author Phil Schatzmann
 * @brief If you want to use the framework w/o Arduino you need to provide the
 * implementation of a couple of classes and methods!
 * @version 0.1
 * @date 2022-09-19
 *
 * @copyright Copyright (c) 2022
 *
 */
// Used by logger: so we can not use any logging in this file
#ifdef IS_DESKTOP
#error We should not get here!
#endif
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IS_NOARDUINO

#ifndef PSTR
#define PSTR(fmt) fmt
#endif

#ifndef PI
#define PI 3.14159265359f
#endif

#ifndef INPUT
#define INPUT 0x0
#endif

#ifndef OUTPUT
#define OUTPUT 0x1
#endif

#ifndef INPUT_PULLUP
#define INPUT_PULLUP 0x2
#endif

#ifndef HIGH
#define HIGH 0x1
#endif
#ifndef LOW
#define LOW 0x0
#endif

// using namespace std;

enum PrintCharFmt { DEC = 10, HEX = 16 };

class Print {
 public:
#ifndef DOXYGEN
  virtual size_t write(uint8_t ch) {
    // not implememnted: to be overritten
    return 0;
  }

  virtual size_t write(const char* str) {
    return write((const uint8_t*)str, strlen(str));
  }

  virtual size_t write(const char* buffer, size_t size) {
    return write((const uint8_t*)buffer, size);
  }

  virtual size_t print(const char* msg) {
    int result = strlen(msg);
    return write(msg, result);
  }

  virtual size_t println(const char* msg = "") {
    int result = print(msg);
    write('\n');
    return result + 1;
  }

  virtual size_t println(float number) {
    char buffer[120];
    snprintf(buffer, 120, "%f", number);
    return println(buffer);
  }

  virtual size_t print(float number) {
    char buffer[120];
    snprintf(buffer, 120, "%f", number);
    return print(buffer);
  }

  virtual size_t print(int number) {
    char buffer[80];
    snprintf(buffer, 80, "%d", number);
    return print(buffer);
  }

  virtual size_t print(char c, PrintCharFmt spec) {
    char result[5];
    switch (spec) {
      case DEC:
        snprintf(result, 3, "%c", c);
        return print(result);
      case HEX:
        snprintf(result, 3, "%x", c);
        return print(result);
    }
    return -1;
  }

  size_t println(int value, PrintCharFmt fmt) {
    return print(value, fmt) + println();
  }

#endif

  virtual size_t write(const uint8_t* data, size_t len) {
    if (data == nullptr) return 0;
    for (size_t j = 0; j < len; j++) {
      write(data[j]);
    }
    return len;
  }

  virtual int availableForWrite() { return 1024; }

  virtual void flush() { /* Empty implementation for backward compatibility */ }

 protected:
  int _timeout = 10;
};

class Stream : public Print {
 public:
  virtual ~Stream() = default;
  virtual int available() { return 0; }
  virtual size_t readBytes(uint8_t* data, size_t len) { return 0; }
#ifndef DOXYGEN
  virtual int read() { return -1; }
  virtual int peek() { return -1; }
  virtual void setTimeout(size_t timeoutMs) {}
  size_t readBytesUntil(char terminator, char* buffer, size_t length) {
    for (size_t j = 0; j < length; j++) {
      int val = read();
      if (val == -1) return j - 1;
      if (val == terminator) return j;
      buffer[j] = val;
    }
    return length;
  };
  size_t readBytesUntil(char terminator, uint8_t* buffer, size_t length) {
    return readBytesUntil(terminator, (char*)buffer, length);
  }

#endif
  operator bool() { return true; }
};

class Client : public Stream {
 public:
  void stop() {};
  virtual int read(uint8_t* buffer, size_t len) { return 0; };
  virtual int read() { return 0; };
  bool connected() { return false; };
  bool connect(const char* ip, int port) { return false; }
  virtual operator bool() { return false; }
};

class HardwareSerial : public Stream {
 public:
  size_t write(uint8_t ch) override { return putchar(ch); }
  virtual operator bool() { return true; }
  bool begin(long baudrate, int config = 0) { return true; }
};

static HardwareSerial Serial;

/// Maps input to output values
inline long map(long x, long in_min, long in_max, long out_min, long out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

#if defined(ESP32_CMAKE)

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"  // needed for ESP Arduino < 2.0
#include "freertos/FreeRTOSConfig.h"

#define GPIO_NONE -1
#define IS_GPIO(pin) (pin >= 0)
#define GPIO_TO_INT(pin) pin
#define GPIO_TO_STR(pin) std::to_string(pin).c_str()

using digital_pin_t = int;

/// e.g. for AudioActions
inline int digitalRead(digital_pin_t pin) {
  printf("digitalRead:%d\n", pin);
  return gpio_get_level((gpio_num_t)pin);
}

inline void digitalWrite(digital_pin_t pin, int value) {
  gpio_set_level((gpio_num_t)pin, value);
}

inline void pinMode(digital_pin_t pin, int mode) {
  gpio_num_t gpio_pin = (gpio_num_t)pin;
  printf("pinMode(%d,%d)\n", pin, mode);

  gpio_reset_pin(gpio_pin);
  switch (mode) {
    case INPUT:
      gpio_set_direction(gpio_pin, GPIO_MODE_INPUT);
      break;
    case OUTPUT:
      gpio_set_direction(gpio_pin, GPIO_MODE_OUTPUT);
      break;
    case INPUT_PULLUP:
      gpio_set_direction(gpio_pin, GPIO_MODE_INPUT);
      gpio_set_pull_mode(gpio_pin, GPIO_PULLUP_ONLY);
      break;
    default:
      gpio_set_direction(gpio_pin, GPIO_MODE_INPUT_OUTPUT);
      break;
  }
}

inline void delay(uint32_t ms) { vTaskDelay(ms / portTICK_PERIOD_MS); }
inline void yield() { taskYIELD(); }
inline uint32_t millis() { return (xTaskGetTickCount() * portTICK_PERIOD_MS); }
inline void delayMicroseconds(uint32_t ms) { esp_rom_delay_us(ms); }
inline uint64_t micros() {
  return xTaskGetTickCount() * portTICK_PERIOD_MS * 1000;
}

// delay and millis has been defined
#define DESKTOP_MILLIS_DEFINED

#endif

#if defined(IS_ZEPHYR)
#include <zephyr/drivers/gpio.h>

#define DESKTOP_MILLIS_DEFINED
#define IS_GPIO(pin) (pin.port != nullptr)
#define GPIO_TO_INT(pin) pin.pin
#define GPIO_TO_STR(pin) std::to_string(GPIO_TO_INT(pin)).c_str()
#define ZLOGE(...) printk(__VA_ARGS__)

namespace audio_tools {

inline void delay(uint32_t ms) { k_msleep(ms); }
inline uint32_t millis() { return k_uptime_get_32(); }
inline void delayMicroseconds(uint32_t us) {
  if (us == 0) return;
  k_busy_wait(us);
}
inline uint64_t micros() { return k_cyc_to_us_floor64(k_cycle_get_64()); }
inline void yield() { k_yield(); }


/// Zephyr GPIO spec as digital_pin_t
using digital_pin_t = gpio_dt_spec;

/// GPIO_NONE is no pin defined
static gpio_dt_spec GPIO_NONE = {nullptr, 0, 0};

/// Support for pin compare
static inline bool operator==(audio_tools::digital_pin_t& a, audio_tools::digital_pin_t& b) {
  return (a.port == b.port) && (a.pin == b.pin);
}

/// Support for pin compare
static inline bool operator!=(audio_tools::digital_pin_t& a, audio_tools::digital_pin_t& b) {
  return !(a == b);
}


void pinMode(digital_pin_t pin, int mode) {
  if (pin == GPIO_NONE || !gpio_is_ready_dt(&pin)) {
    ZLOGE("GPIO pin not ready");
    return;
  }

  gpio_flags_t flags;
  switch (mode) {
    case OUTPUT:
      flags = GPIO_OUTPUT;
      break;
    case INPUT:
      flags = GPIO_INPUT;
      break;
    case INPUT_PULLUP:
      flags = GPIO_INPUT | GPIO_PULL_UP;
      break;
    // case INPUT_PULLDOWN:
    //   flags = GPIO_INPUT | GPIO_PULL_DOWN;
    //   break;
    default:
      flags = GPIO_OUTPUT;
      break;
  }

  int rc = gpio_pin_configure_dt(&pin, flags);
  if (rc != 0) {
    ZLOGE("Failed to configure GPIO pin: %d", rc);
  }
}

void digitalWrite(digital_pin_t pin, bool value) {
  if (pin == GPIO_NONE || !gpio_is_ready_dt(&pin)) {
    ZLOGE("GPIO pin not ready");
  }

  int rc = gpio_pin_set_dt(&pin, value ? 1 : 0);
  if (rc != 0) {
    ZLOGE("Failed to write GPIO pin: %d", rc);
  }
}

int digitalRead(digital_pin_t pin) {
  if (pin == GPIO_NONE || !gpio_is_ready_dt(&pin)) {
    ZLOGE("GPIO pin not ready");
    return false;
  }

  int rc = gpio_pin_get_dt(&pin);
  if (rc < 0) {
    ZLOGE("Failed to read GPIO pin: %d", rc);
  }
  return rc;
}

} // namespace audio_tools
#endif
