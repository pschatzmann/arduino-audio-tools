#pragma once
#include "AudioTools/AudioStreams.h"

namespace audio_tools {

class GT38Config {
public:
  HardwareSerial *serial = &Serial;
  uint8_t power = 8;             // 1 - 8
  uint8_t transmission_mode = 3; // 1 - 4
  uint8_t channel = 100;         // 0 - 254
  uint32_t baud_rate = 115200;   // 1200, 2400, 4800, 9600, 19200, 57600,115200
  SerialConfig serial_format = SERIAL_8N1;
};

/**
 * @brief A communications class which uses the Serial GT-38 (SI4438/4463) RF
 * Transceiver which operates at 433 MHz.
 * @ingroup communications
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class GT38Stream : public AudioStream {
public:
  GT38Config defaultConfig() {
    GT38Config cfg;
    return cfg;
  }

  bool begin(GT38Config cfg) {
    config = cfg;
    return begin();
  }

  bool begin() {
    bool ok = false;
    // set serial to default settings of module
    config.serial->begin(9600, SERIAL_8N1);
    LOGI("Version: %s", version());

    // change format
    ok = setSerialFormat(config.serial_format);
    if (!ok)
      return false;
    config.serial->begin(9600, config.serial_format);

    // change baud rate
    ok = setBaudRate(config.baud_rate);
    if (!ok)
      return ok;

    // change rate & format of Serial!
    config.serial->begin(config.baud_rate, config.serial_format);

    ok = setPower(config.power);
    if (!ok)
      return false;

    ok = setTransmissionMode(config.transmission_mode);
    if (!ok)
      return false;

    ok = setChannel(config.channel);
    return ok;
  }

  void end() { setSleep(); }

  int available() { return config.serial->available(); }

  int availableForWrite() { return config.serial->availableForWrite(); }

  size_t write(const uint8_t *data, size_t len) {
    return config.serial->write(data, len);
  }

  size_t readBytes(uint8_t *data, size_t len) {
    return config.serial->readBytes(data, len);
  }

  bool setDefault() { return at("DEFAULT"); }

  bool setSleep() { return at("SLEEP"); }

  const char *version() { return at2Str("V"); }

  bool test() { return at(""); }

protected:
  enum class Parity { NO = 'N', ODD = 'O', EVEN = 'E' };
  GT38Config config;

  bool setBaudRate(uint16_t rate) {
    char tmp[20];
    snprintf(tmp, 5, "B%d", rate);
    return at(tmp);
  }

  bool setPower(uint8_t power) {
    char tmp[5];
    snprintf(tmp, 5, "P%d", power);
    return at(tmp);
  }

  bool setTransmissionMode(uint8_t mode) {
    char tmp[5];
    snprintf(tmp, 5, "FU%d", mode);
    return at(tmp);
  }

  bool setChannel(uint8_t ch) {
    char tmp[5];
    snprintf(tmp, 5, "C%03d", ch);
    return at(tmp);
  }

  bool at(const char *cmd) {
    char *str = at2Str(cmd);
    bool ok = memcmp(str, "OK", 2) == 0;
    if (!ok) {
      LOGE("at: %s -> %s", cmd, str);
    }
    return ok;
  }

  char *at2Str(const char *cmd) {
    static char tmp[80];
    snprintf(tmp, 80, "AT+%s\r\n", cmd);
    config.serial->print(tmp);
    config.serial->readBytesUntil('\n', tmp, 80);
    return tmp;
  }

  bool setSerialFormat(SerialConfig fmt) {
    switch (fmt) {
    case SERIAL_5N1: // Parity::NO parity
      return setSerialFormat(5, Parity::NO, 1);
    case SERIAL_6N1:
      return setSerialFormat(6, Parity::NO, 1);
    case SERIAL_7N1:
      return setSerialFormat(7, Parity::NO, 1);
    case SERIAL_8N1: // -> (the default)
      return setSerialFormat(8, Parity::NO, 1);
    case SERIAL_5N2:
      return setSerialFormat(5, Parity::NO, 2);
    case SERIAL_6N2:
      return setSerialFormat(6, Parity::NO, 2);
    case SERIAL_7N2:
      return setSerialFormat(7, Parity::NO, 2);
    case SERIAL_8N2:
      return setSerialFormat(8, Parity::NO, 2);
    case SERIAL_5E1: // Parity::EVEN parity
      return setSerialFormat(5, Parity::EVEN, 1);
    case SERIAL_6E1:
      return setSerialFormat(6, Parity::EVEN, 1);
    case SERIAL_7E1:
      return setSerialFormat(7, Parity::EVEN, 1);
    case SERIAL_8E1:
      return setSerialFormat(8, Parity::EVEN, 1);
    case SERIAL_5E2:
      return setSerialFormat(5, Parity::EVEN, 2);
    case SERIAL_6E2:
      return setSerialFormat(6, Parity::EVEN, 2);
    case SERIAL_7E2:
      return setSerialFormat(7, Parity::EVEN, 2);
    case SERIAL_8E2:
      return setSerialFormat(8, Parity::EVEN, 2);
    case SERIAL_5O1: // Parity::ODD parity
      return setSerialFormat(5, Parity::ODD, 1);
    case SERIAL_6O1:
      return setSerialFormat(6, Parity::ODD, 1);
    case SERIAL_7O1:
      return setSerialFormat(7, Parity::ODD, 1);
    case SERIAL_8O1:
      return setSerialFormat(8, Parity::ODD, 1);
    case SERIAL_5O2:
      return setSerialFormat(5, Parity::ODD, 2);
    case SERIAL_6O2:
      return setSerialFormat(6, Parity::ODD, 2);
    case SERIAL_7O2:
      return setSerialFormat(7, Parity::ODD, 2);
    case SERIAL_8O2:
      return setSerialFormat(8, Parity::ODD, 2);
    }
    return false;
  }

  bool setSerialFormat(uint8_t digits, Parity parity, uint8_t stop_bits) {
    char tmp[5];
    snprintf(tmp, 5, "U%d%c%d", digits, parity, stop_bits);
    return at(tmp);
  }
};

} // namespace audio_tools