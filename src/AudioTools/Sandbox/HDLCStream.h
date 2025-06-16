#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "AudioTools/CoreAudio/Buffers.h"
#include "AudioToolsConfig.h"

/// HDLC Asynchronous framing: The frame boundary octet is 01111110, (7E in
/// hexadecimal notation)
#define FRAME_BOUNDARY_OCTET 0x7E

/// A "control escape octet", has the bit sequence '01111101', (7D hexadecimal)
#define CONTROL_ESCAPE_OCTET 0x7D

/// If either of these two octets appears in the transmitted data, an escape
/// octet is sent, followed by the original data octet with bit 5 inverted
#define INVERT_OCTET 0x20

/// The frame check sequence (FCS) is a 16-bit CRC-CCITT
/// AVR Libc CRC function is crc_ccitt_update()
/// Corresponding CRC function in Qt (www.qt.io) is qChecksum()
#define CRC16_CCITT_INIT_VAL 0xFFFF

/// 16bit low and high bytes copier
#define low(x) ((x) & 0xFF)
#define high(x) (((x) >> 8) & 0xFF)
#define lo8(x) ((x) & 0xff)
#define hi8(x) ((x) >> 8)

namespace audio_tools {

/**
 * @enum HDLCWriteLogic
 * @brief Defines when the actual HDLC frame is written to the output stream
 */
enum class HDLCWriteLogic { 
  /** @brief Write frames when the internal buffer is full */
  OnBufferFull, 
  /** @brief Write frames when flush() is called */
  OnFlush, 
  /** @brief Write frames immediately on each write() call */
  OnWrite 
};

/**
 * @brief High-Level Data Link Control (HDLC) is a bit-oriented code-transparent
 * synchronous data link layer protocol for reliable, framed, and error-checked communication.
 *
 * This class implements HDLC framing with:
 * - Frame delimiter (0x7E) for marking frame boundaries
 * - Byte stuffing to escape special characters in the data
 * - 16-bit CRC-CCITT for error detection
 * - Transparent Stream interface for easy integration
 *
 * @ingroup communications
 * @author Phil Schatzmann
 */
class HDLCStream : public Stream {
 public:
  /**
   * @brief Constructor with write-only operation
   * 
   * @param out Print object to which encoded HDLC frames will be written
   * @param max_frame_length Maximum size of a single HDLC frame in bytes
   */
  HDLCStream(Print &out, uint16_t max_frame_length) {
    setOutput(out);
    if (getTimeout()==0) setTimeOut(1000); // default timeout of 1 second
    this->max_frame_length = max_frame_length;
    begin();
  }

  /**
   * @brief Constructor with read-write operation
   * 
   * @param io Stream object for bidirectional HDLC communication
   * @param max_frame_length Maximum size of a single HDLC frame in bytes
   */
  HDLCStream(Stream &io, uint16_t max_frame_length) {
    setStream(io);
    if (getTimeout()==0) setTimeOut(1000); // default timeout of 1 second
    this->max_frame_length = max_frame_length;
    begin();
  }

  /**
   * @brief Initialize the HDLC stream
   * 
   * Resets the frame position, checksum, and escape character state.
   * Allocates the frame buffer to the specified max_frame_length.
   * 
   * @return true if a valid input or output stream has been set
   * @return false if no valid stream is available
   */
  bool begin() {
    this->frame_position = 0;
    this->frame_checksum = CRC16_CCITT_INIT_VAL;
    this->escape_character = false;
    if (frame_buffer.size() != max_frame_length) {
      frame_buffer.resize(max_frame_length);
    }
    return p_out != nullptr || p_in != nullptr;
  }

  /**
   * @brief Clean up resources used by the HDLC stream
   * 
   * Releases the frame buffer memory by resizing it to zero.
   */
  void end() { this->frame_buffer.resize(0); }

  /**
   * @brief Get the number of bytes available in the write buffer
   * 
   * @return int Available space for writing or 0 if no output is defined
   */
  int availableForWrite() override {
    return p_out == nullptr ? 0 : max_frame_length;
  }

  /**
   * @brief Write data to be encoded as HDLC frames
   * 
   * Processes data according to the current HDLCWriteLogic setting:
   * - OnBufferFull: Stores data in buffer, sends when buffer is full
   * - OnFlush: Stores data in buffer, waits for flush() to send
   * - OnWrite: Immediately sends data as an HDLC frame
   * 
   * @param data Pointer to the data buffer to be written
   * @param len Number of bytes to write
   * @return size_t Number of bytes accepted (typically equals len)
   */
  size_t write(const uint8_t *data, size_t len) override {
    LOGD("HDLCStream::write: %zu", len);

    switch (write_logic) {
      case HDLCWriteLogic::OnFlush:
        for (int j = 0; j < len; j++) {
          bool ok = frame_buffer.write(data[j]);
          if (frame_buffer.isFull()) {
            LOGE("Buffer full - increase size!");
          }
        }
        break;
      case HDLCWriteLogic::OnBufferFull:
        for (int j = 0; j < len; j++) {
          bool ok = frame_buffer.write(data[j]);
          assert(ok);
          if (frame_buffer.isFull()) {
            sendFrame(frame_buffer.data(), max_frame_length);
            frame_buffer.reset();
          }
        }
        break;
      case HDLCWriteLogic::OnWrite:
        sendFrame(data, len);
        break;
    }

    return len;
  }

  /**
   * @brief Flush any buffered data as an HDLC frame
   * 
   * If data is pending in the buffer, encodes and sends it as an HDLC frame.
   * Also flushes the underlying output stream if available.
   */
  void flush() override {
    LOGD("HDLCStream::flush");
    if (frame_buffer.available() > 0) {
      sendFrame(frame_buffer.data(), frame_buffer.available());
      frame_buffer.reset();
    }
    if (p_out != nullptr) {
      p_out->flush();
    }
  }

  /**
   * @brief Get number of bytes available for reading
   * 
   * @return int Maximum frame length if input stream is available, 0 otherwise
   */
  int available() override { return p_in == nullptr ? 0 : max_frame_length; }

  /**
   * @brief Read decoded data from the next valid HDLC frame
   * 
   * Processes incoming bytes through the HDLC protocol decoder until a
   * complete valid frame is found or timeout occurs.
   * 
   * @param data Buffer to store the decoded data
   * @param len Maximum number of bytes to read
   * @return size_t Number of bytes actually read (0 if no valid frame found)
   */
  size_t readBytes(uint8_t *data, size_t len) override {
    if (p_in == nullptr) {
      LOGI("No data source");
      return 0;
    }

    int result = 0;
    unsigned long startTime = millis();

    // process bytes from input with timeout
    while (result == 0 && (millis() - startTime < timeout_ms)) {
      // Check if data is available before blocking on read
      if (p_in->available() <= 0) {
        continue;
      }

      int ch = p_in->read();
      if (ch >= 0) {
        result = charReceiver(ch);
        if (result > 0) {
          // Respect the requested length limit
          result = min(result, (int)len);
          result = frame_buffer.readArray(data, result);
          break;
        }
      } else {
        break;
      }
    }

    LOGD("HDLCStream::readBytes: %zu -> %d", len, result);
    return result;
  }
  
  /**
   * @brief Set both input and output streams to the same object
   * 
   * @param io Stream object for bidirectional HDLC communication
   */
  void setStream(Stream &io) {
    p_out = &io;
    p_in = &io;
  }

  /**
   * @brief Set output stream only for write-only operation
   * 
   * @param out Stream object for HDLC output
   */
  void setStream(Print &out) { p_out = &out; }

  /**
   * @brief Set output destination for encoded HDLC frames
   * 
   * @param out Print object to receive encoded HDLC data
   */
  void setOutput(Print &out) { p_out = &out; }

  /**
   * @brief Write a single byte to be encoded in an HDLC frame
   * 
   * @param ch Byte to write
   * @return size_t Always returns 1 if successful
   */
  size_t write(uint8_t ch) override { return write(&ch, 1); };

  /**
   * @brief Read a single byte from the decoded HDLC data
   * 
   * @return int Byte value (0-255) or -1 if no data available
   */
  int read() override {
    uint8_t c[1];
    if (readBytes(c, 1) == 0) {
      return -1;
    }
    return c[0];
  }

  /**
   * @brief Not supported in this implementation
   * 
   * @return int Always returns -1
   */
  int peek() override { return -1; }

  /**
   * @brief Set the framing logic for writing data
   * 
   * Controls when data is actually encoded and sent as HDLC frames:
   * - OnBufferFull: When internal buffer is full
   * - OnFlush: When flush() is called
   * - OnWrite: Immediately on each write
   * 
   * @param logic The framing logic to use
   */
  void setWriteLogic(HDLCWriteLogic logic) { write_logic = logic; }

 private:
  Print *p_out = nullptr;
  Stream *p_in = nullptr;
  bool escape_character = false;
  SingleBuffer<uint8_t> frame_buffer{0};
  uint8_t frame_position = 0;
  // 16bit CRC sum for crc_ccitt_update
  uint16_t frame_checksum;
  uint16_t max_frame_length;
  HDLCWriteLogic write_logic = HDLCWriteLogic::OnWrite;

  /// Function to find valid HDLC frame from incoming data: returns the
  /// available result bytes in the buffer
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
      frame_checksum = crc_ccitt_update(frame_checksum,
                                         frame_buffer_data[frame_position - 2]);
    }

    frame_position++;

    if (frame_position == max_frame_length) {
      LOGE("buffer overflow: %d", frame_position);
      frame_position = 0;
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
      fcs = HDLCStream::crc_ccitt_update(fcs, data);
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

  /**
   * @brief Calculate CRC-CCITT checksum
   * 
   * Implementation of CRC-CCITT (0xFFFF) calculation used for HDLC frames
   * 
   * @param crc Current CRC value
   * @param data Byte to include in CRC calculation
   * @return uint16_t Updated CRC value
   */
  static uint16_t crc_ccitt_update(uint16_t crc, uint8_t data) {
    data ^= lo8(crc);
    data ^= data << 4;
    return ((((uint16_t)data << 8) | hi8(crc)) ^ (uint8_t)(data >> 4) ^
            ((uint16_t)data << 3));
  }
};

}  // namespace audio_tools