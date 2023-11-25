#include "AudioConfig.h"
#include "AudioTools/Buffers.h"
#include <stdbool.h>
#include <stdint.h>

/// HDLC Asynchronous framing: The frame boundary octet is 01111110, (7E in
/// hexadecimal notation)
#define FRAME_BOUNDARY_OCTET 0x7E

/// A "control escape octet", has the bit sequence '01111101', (7D hexadecimal)
#define CONTROL_ESCAPE_OCTET 0x7D

/// If either of these two octets appears in the transmitted data, an escape
/// octet is sent, followed by the original data octet with bit 5 inverted
#define INVERT_OCTET 0x20

/// The frame check sequence (FCS) is a 16-bit CRC-CCITT
/// AVR Libc CRC function is _crc_ccitt_update()
/// Corresponding CRC function in Qt (www.qt.io) is qChecksum()
#define CRC16_CCITT_INIT_VAL 0xFFFF

/// 16bit low and high bytes copier
#define low(x) ((x)&0xFF)
#define high(x) (((x) >> 8) & 0xFF)
#define lo8(x) ((x)&0xff)
#define hi8(x) ((x) >> 8)

namespace audio_tools {

/**
 * @brief High-Level Data Link Control (HDLC) is a bit-oriented code-transparent
 * synchronous data link layer protocol
 * @ingroup communications
 */

class HDLCStream : public Stream {
public:
  /// Defines the output for the hdlc encoding
  HDLCStream(Print &out, uint16_t max_frame_length) {
    setOutput(out);
    this->max_frame_length = max_frame_length;
    begin();
  }

  /// Defines the input for the hdlc decoding
  HDLCStream(Stream &io, uint16_t max_frame_length) {
    setStream(io);
    this->max_frame_length = max_frame_length;
    begin();
  }

  bool begin() {
    this->frame_position = 0;
    this->frame_checksum = CRC16_CCITT_INIT_VAL;
    this->escape_character = false;
    if (frame_buffer.size() == 0) {
      frame_buffer.resize(max_frame_length + 1);
    }
    return p_out != nullptr || p_in != nullptr;
  }

  void end() { this->frame_buffer.resize(0); }

  int availableForWrite() override {
    return p_out == nullptr ? 0 : DEFAULT_BUFFER_SIZE;
  }

  /// Sends the encoded data to the defined output
  size_t write(const uint8_t *data, size_t len) override {
    LOGD("HDLCStream::write: %zu", len);

    for (int j = 0; j < len; j++) {
      bool ok = frame_buffer.write(data[j]);
      assert(ok);
      if (frame_buffer.available() == max_frame_length) {
        sendFrame(frame_buffer.data(), max_frame_length);
        frame_buffer.reset();
      }
    }
    return len;
  }

  int available() override {
    return p_in == nullptr ? 0 : max_frame_length;
  }

  /// Provides the decoded data
  size_t readBytes(uint8_t *data, size_t len) override {
    if (p_in == nullptr) {
      LOGI("No data source");
      return 0;
    }

    int result = 0;
    // process bytes from input
    while (result == 0) {
      int ch = p_in->read();
      // ch is -1 when no data
      if (ch >= 0){        
        result = charReceiver(ch);
        if (result > 0) {
          result = frame_buffer.readArray(data, result);
          break;
        }
      } else {
        break;
      }
    }
    LOGD("HDLCStream::readBytes: %zu -> %d", len, result);
    return result;
    ;
  }

  void setStream(Stream &io) {
    p_out = &io;
    p_in = &io;
  }

  void setStream(Print &out) { p_out = &out; }

  void setOutput(Print &out) { p_out = &out; }

  size_t write(uint8_t ch) override { return write(&ch, 1); };

  int read() override {
    uint8_t c[1];
    if (readBytes(c, 1) == 0) {
      return -1;
    }
    return c[0];
  }

  /// not supported
  int peek() override { return -1; }

private:
  Print *p_out = nullptr;
  Stream *p_in = nullptr;
  bool escape_character = false;
  SingleBuffer<uint8_t> frame_buffer{0};
  uint8_t frame_position = 0;
  // 16bit CRC sum for _crc_ccitt_update
  uint16_t frame_checksum;
  uint16_t max_frame_length;

  /// Function to find valid HDLC frame from incoming data: returns the available result bytes in the buffer 
  int charReceiver(uint8_t data) {
    int result = 0;
    uint8_t *frame_buffer_data = frame_buffer.address();
    LOGD("charReceiver: %c", data);

    if (data == FRAME_BOUNDARY_OCTET) {
      if (escape_character == true) {
        escape_character = false;
      } else if ((frame_position >= 2) &&
                 (frame_checksum ==
                  ((frame_buffer_data[frame_position - 1] << 8) |
                   (frame_buffer_data[frame_position - 2] & 0xff)))) {
        // trigger processing of result data
        frame_buffer.setAvailable(frame_position - 2);
        result = frame_buffer.available();
        LOGD("==> Result: %d", result);
      }
      frame_position = 0;
      frame_checksum = CRC16_CCITT_INIT_VAL;
      return result;
    }

    if (escape_character) {
      escape_character = false;
      data ^= INVERT_OCTET;
    } else if (data == CONTROL_ESCAPE_OCTET) {
      escape_character = true;
      return result;
    }

    frame_buffer_data[frame_position] = data;

    if (frame_position - 2 >= 0) {
      frame_checksum = _crc_ccitt_update(frame_checksum,
                                         frame_buffer_data[frame_position - 2]);
    }

    frame_position++;

    if (frame_position == max_frame_length) {
      frame_position = 0;
      frame_checksum = CRC16_CCITT_INIT_VAL;
    }
    return result;
  }

  /// Wrap given data in HDLC frame and send it out byte at a time
  void sendFrame(const uint8_t *framebuffer, size_t frame_length) {
    LOGD("HDLCStream::sendFrame: %zu", frame_length);
    uint8_t data;
    uint16_t fcs = CRC16_CCITT_INIT_VAL;

    p_out->write((uint8_t)FRAME_BOUNDARY_OCTET);

    while (frame_length) {
      data = *framebuffer++;
      fcs = HDLCStream::_crc_ccitt_update(fcs, data);
      if ((data == CONTROL_ESCAPE_OCTET) || (data == FRAME_BOUNDARY_OCTET)) {
        p_out->write((uint8_t)CONTROL_ESCAPE_OCTET);
        data ^= INVERT_OCTET;
      }
      p_out->write((uint8_t)data);
      frame_length--;
    }
    data = low(fcs);
    if ((data == CONTROL_ESCAPE_OCTET) || (data == FRAME_BOUNDARY_OCTET)) {
      p_out->write((uint8_t)CONTROL_ESCAPE_OCTET);
      data ^= (uint8_t)INVERT_OCTET;
    }
    p_out->write((uint8_t)data);
    data = high(fcs);
    if ((data == CONTROL_ESCAPE_OCTET) || (data == FRAME_BOUNDARY_OCTET)) {
      p_out->write(CONTROL_ESCAPE_OCTET);
      data ^= INVERT_OCTET;
    }
    p_out->write(data);
    p_out->write(FRAME_BOUNDARY_OCTET);
    p_out->flush();
  }

  static uint16_t crc16_update(uint16_t crc, uint8_t a) {
    int i;
    crc ^= a;
    for (i = 0; i < 8; ++i) {
      if (crc & 1)
        crc = (crc >> 1) ^ 0xA001;
      else
        crc = (crc >> 1);
    }
    return crc;
  }

  static uint16_t crc_xmodem_update(uint16_t crc, uint8_t data) {
    int i;

    crc = crc ^ ((uint16_t)data << 8);
    for (i = 0; i < 8; i++) {
      if (crc & 0x8000)
        crc = (crc << 1) ^ 0x1021;
      else
        crc <<= 1;
    }

    return crc;
  }

  static uint16_t _crc_ccitt_update(uint16_t crc, uint8_t data) {
    data ^= lo8(crc);
    data ^= data << 4;
    return ((((uint16_t)data << 8) | hi8(crc)) ^ (uint8_t)(data >> 4) ^
            ((uint16_t)data << 3));
  }

  static uint8_t _crc_ibutton_update(uint8_t crc, uint8_t data) {
    uint8_t i;
    crc = crc ^ data;
    for (i = 0; i < 8; i++) {
      if (crc & 0x01)
        crc = (crc >> 1) ^ 0x8C;
      else
        crc >>= 1;
    }
    return crc;
  }
};

} // namespace audio_tools