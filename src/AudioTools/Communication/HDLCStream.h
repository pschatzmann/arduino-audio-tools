#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "AudioTools/CoreAudio/Buffers.h"
#include "AudioToolsConfig.h"

namespace audio_tools {

/**
 * @brief High-Level Data Link Control (HDLC) is a bit-oriented code-transparent
 * synchronous data link layer protocol for reliable, framed, and error-checked
 * communication.
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
   * @brief Construct a new HDLCStream object using a Stream for input and output
   * 
   * @param stream The underlying Stream for both reading and writing
   * @param maxFrameSize Maximum size of a single HDLC frame
   */
  HDLCStream(Stream& stream, size_t maxFrameSize)
      : _maxFrameSize(maxFrameSize) {
    p_stream = &stream;
    p_print = &stream;
    tx_frame_buffer.resize(maxFrameSize);
    rx_frame_buffer.resize(maxFrameSize);
    _rxBuffer.resize(maxFrameSize);
  }

  /**
   * @brief Construct a new HDLCStream object using a Print for output only
   * 
   * @param stream The underlying Print for writing
   * @param maxFrameSize Maximum size of a single HDLC frame
   */
  HDLCStream(Print& stream, size_t maxFrameSize) : _maxFrameSize(maxFrameSize) {
    p_print = &stream;
    tx_frame_buffer.resize(maxFrameSize);
    rx_frame_buffer.resize(maxFrameSize);
    _rxBuffer.resize(maxFrameSize);
  }

  /**
   * @brief Get the number of bytes available to read from the frame buffer
   * 
   * @return int Number of bytes available
   */
  int available() override {
    if (rx_frame_buffer.available() == 0) _processInput();
    return rx_frame_buffer.available();
  }

  /**
   * @brief Not supported
   * 
   * @return -1 
   */
  int read() override { return -1; }

  /**
   * @brief Read a full frame from the stream into a buffer
   * 
   * @param buffer Destination buffer to hold the data
   * @param length Maximum number of bytes to read
   * @return size_t Actual number of bytes read
   */
  size_t readBytes(uint8_t* buffer, size_t length) override {
    size_t available_bytes = rx_frame_buffer.available();
    // get more data
    if (available_bytes == 0) {
      _processInput();
      available_bytes = rx_frame_buffer.available();
    }

    // check that we consume the full frame
    if (length < available_bytes) {
      LOGE("readBytes len too small %u instead of %u", (unsigned)length,
           (unsigned)available_bytes);
      return 0;
    }

    // provide the data
    memcpy(buffer, rx_frame_buffer.data(), available_bytes);
    rx_frame_buffer.clear();
    _frameReady = false;
    return available_bytes;
  }

  /**
   * @brief Not supported
   * 
   * @return -1 
   */
  int peek() override { return -1; }

  /**
   * @brief Flush the output buffer of the underlying stream
   */
  void flush() override { p_stream->flush(); }

  /**
   * @brief Not supported
   * 
   * @param b The byte to write
   * @return  0 
   */
  size_t write(uint8_t b) override { return 0; }

  /**
   * @brief Write multiple bytes to the stream
   * 
   * @param data Pointer to the data buffer
   * @param len Number of bytes to write
   * @return size_t Number of bytes written
   */
  size_t write(const uint8_t* data, size_t len) override {
    return writeFrame(data, len);
  }

 protected:
  Stream* p_stream = nullptr;
  Print* p_print = nullptr;
  const size_t _maxFrameSize;
  SingleBuffer<uint8_t> tx_frame_buffer;
  SingleBuffer<uint8_t> rx_frame_buffer;
  Vector<uint8_t> _rxBuffer;
  size_t _frameLen = 0;
  size_t _rxLen = 0;
  size_t _rxPos = 0;
  bool _frameReady = false;

  enum RxState { IDLE, RECEIVING, ESCAPED } _rxState = IDLE;

  static constexpr uint8_t HDLC_FLAG = 0x7E;
  static constexpr uint8_t HDLC_ESC = 0x7D;
  static constexpr uint8_t HDLC_ESC_XOR = 0x20;

  /**
   * @brief Calculate CRC-CCITT (16-bit)
   * 
   * @param data Byte to include in CRC calculation
   * @param crc Current CRC value
   * @return uint16_t Updated CRC value
   */
  uint16_t _crc16(uint8_t data, uint16_t crc) {
    crc ^= (uint16_t)data << 8;
    for (int i = 0; i < 8; i++)
      crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : (crc << 1);
    return crc;
  }

  /**
   * @brief Write a byte with proper HDLC byte stuffing if needed
   * 
   * @param b Byte to write
   */
  void _writeEscaped(uint8_t b) {
    if (b == HDLC_FLAG || b == HDLC_ESC) {
      tx_frame_buffer.write(HDLC_ESC);
      tx_frame_buffer.write(b ^ HDLC_ESC_XOR);
    } else {
      tx_frame_buffer.write(b);
    }
  }

  /**
   * @brief Write a complete HDLC frame with proper framing and CRC
   * 
   * @param data Data to be framed
   * @param len Length of data
   * @return size_t Number of bytes in the original data
   */
  size_t writeFrame(const uint8_t* data, size_t len) {
    if (!data || len == 0) return 0;

    uint16_t crc = 0xFFFF;
    tx_frame_buffer.write(HDLC_FLAG);

    for (size_t i = 0; i < len; ++i) {
      crc = _crc16(data[i], crc);
      _writeEscaped(data[i]);
    }

    _writeEscaped(crc >> 8);
    _writeEscaped(crc & 0xFF);
    tx_frame_buffer.write(HDLC_FLAG);
    p_print->write(tx_frame_buffer.data(), tx_frame_buffer.available());
    p_print->flush();
    tx_frame_buffer.clear();
    return len;
  }

  /**
   * @brief Process incoming bytes, detect frames, validate CRC and prepare data
   *        for reading
   */
  void _processInput() {
    while (!_frameReady && p_stream->available()) {
      uint8_t b = p_stream->read();

      if (b == HDLC_FLAG) {
        if (_rxLen >= 3) {
          uint16_t recvCrc =
              (_rxBuffer[_rxLen - 2] << 8) | _rxBuffer[_rxLen - 1];
          uint16_t calcCrc = 0xFFFF;
          for (size_t i = 0; i < _rxLen - 2; ++i)
            calcCrc = _crc16(_rxBuffer[i], calcCrc);

          if (calcCrc == recvCrc) {
            for (int j = 0; j < _rxLen - 2; j++) {
              rx_frame_buffer.write(_rxBuffer[j]);
            }

            _frameLen = _rxLen - 2;
            _rxPos = 0;
            _frameReady = true;
          }
        }
        _rxState = IDLE;
        _rxLen = 0;
        continue;
      }

      switch (_rxState) {
        case IDLE:
          _rxLen = 0;
          if (b == HDLC_ESC) {
            _rxState = ESCAPED;
          } else {
            _rxState = RECEIVING;
            if (_rxLen < _maxFrameSize) _rxBuffer[_rxLen++] = b;
          }
          break;

        case RECEIVING:
          if (b == HDLC_ESC) {
            _rxState = ESCAPED;
          } else if (_rxLen < _maxFrameSize) {
            _rxBuffer[_rxLen++] = b;
          }
          break;

        case ESCAPED:
          if (_rxLen < _maxFrameSize) {
            _rxBuffer[_rxLen++] = b ^ HDLC_ESC_XOR;
          }
          _rxState = RECEIVING;
          break;
      }

      if (_rxLen >= _maxFrameSize) {
        _rxState = IDLE;  // overflow
        _rxLen = 0;
      }
    }
  }
};

}  // namespace audio_tools