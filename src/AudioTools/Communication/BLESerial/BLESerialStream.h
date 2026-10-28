#pragma once

#include "AudioToolsConfig.h"
#include "AudioTools/CoreAudio/BaseStream.h"
#include "AudioTools/CoreAudio/Buffers.h"
#include "AudioTools/CoreAudio/AudioBasic/StrView.h"
#include "AudioTools/Concurrency/Mutex.h"
#include "AudioTools/Concurrency/LockGuard.h"

/// Bytes which are reserved in each BLE packet for the ATT protocol
#ifndef BLE_SERIAL_MTU_OVERHEAD
#define BLE_SERIAL_MTU_OVERHEAD 5
#endif

/// Size of the length header in the packets which are read by the client
#define BLE_SERIAL_HEADER_SIZE 2

/// Max length of a characteristic value defined by the BLE specification
#define BLE_SERIAL_MAX_VALUE_LEN 512

/// Default size of the receive and transmit buffers in bytes
#ifndef BLE_SERIAL_BUFFER_SIZE
#define BLE_SERIAL_BUFFER_SIZE 4096
#endif

/// Default max number of packets in the buffers in framed mode
#ifndef BLE_SERIAL_PACKET_COUNT
#define BLE_SERIAL_PACKET_COUNT 100
#endif

namespace audio_tools {

/**
 * @brief FIFO for BLE packets: in framed mode the packet boundaries are
 * kept, otherwise the data is handled as a byte stream. All methods are
 * protected by the optional mutex, so that the buffer can be filled and
 * consumed by different tasks.
 * @ingroup communications
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class BLESerialPacketBuffer {
 public:
  void setMutex(MutexBase *mutex) { p_mutex = mutex; }

  /// Allocates the buffer memory (if not already done)
  void resize(int bytes, int packets, bool framed) {
    LockGuard guard(p_mutex);
    is_framed = framed;
    if (data.size() != (size_t)bytes) data.resize(bytes);
    if (is_framed && sizes.size() != (size_t)packets) sizes.resize(packets);
  }

  bool isAllocated() { return data.size() > 0; }

  /// Removes all data
  void clear() {
    LockGuard guard(p_mutex);
    data.reset();
    sizes.reset();
  }

  /// Adds a packet: in framed mode only complete packets are added
  int write(const uint8_t *packet, int len) {
    LockGuard guard(p_mutex);
    if (!is_framed) return data.writeArray(packet, len);
    if (sizes.availableForWrite() == 0 || data.availableForWrite() < len)
      return 0;
    sizes.write(len);
    return data.writeArray(packet, len);
  }

  /// Provides the data: in framed mode the next complete packet if it fits
  /// into len bytes
  int read(uint8_t *result, int len) {
    LockGuard guard(p_mutex);
    if (!is_framed) return data.readArray(result, len);
    uint16_t size = 0;
    if (!sizes.peek(size)) return 0;
    if (size > len) {
      LOGE("BLESerial: read size %d too small: it must be >= %d", len, size);
      return 0;
    }
    sizes.read(size);
    return data.readArray(result, size);
  }

  /// Number of available bytes: in framed mode the size of the next packet
  int available() {
    LockGuard guard(p_mutex);
    if (!is_framed) return data.available();
    uint16_t size = 0;
    return sizes.peek(size) ? size : 0;
  }

  /// Number of bytes which can be written
  int availableForWrite() {
    LockGuard guard(p_mutex);
    if (is_framed && sizes.availableForWrite() == 0) return 0;
    return data.availableForWrite();
  }

 protected:
  RingBuffer<uint8_t> data{0};
  RingBuffer<uint16_t> sizes{0};
  MutexBase *p_mutex = nullptr;
  bool is_framed = false;
};

/**
 * @brief Common functionality of the BLE serial server and client: data is
 * exchanged via two characteristics like a serial connection. The audio
 * info can optionally be exchanged via a third characteristic.
 *
 * - setFramed(true) keeps the original write sizes (e.g. for encoded
 *   frames): each write() is transmitted as a separate BLE packet and
 *   readBytes() provides one packet at a time.
 * - setAudioInfoActive(true) informs the other side about changes of the
 *   audio info.
 *
 * @ingroup communications
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class BLESerialStream : public AudioStream {
 public:
  BLESerialStream(int mtu) { max_transfer_size = mtu; }

  virtual void end() = 0;

  /// Returns true if we are connected with the other side
  virtual bool connected() = 0;

  operator bool() override { return connected(); }

  /// Defines the audio info and informs the other side if
  /// setAudioInfoActive(true) was called
  void setAudioInfo(AudioInfo newInfo) override {
    is_audio_info_defined = true;
    AudioStream::setAudioInfo(newInfo);
    if (is_audio_info_active) {
      writeAudioInfo(toStr(newInfo));
    }
  }

  /// Defines the service UUID
  void setServiceUUID(const char *uuid) { BLE_SERIAL_SERVICE_UUID = uuid; }

  /// Defines the UUID of the characteristic which is written by the client
  void setRxUUID(const char *uuid) { BLE_CH2_UUID = uuid; }

  /// Defines the UUID of the characteristic which is read by the client
  void setTxUUID(const char *uuid) { BLE_CH1_UUID = uuid; }

  /// Defines the UUID of the audio info characteristic
  void setAudioInfoUUID(const char *uuid) { BLE_INFO_UUID = uuid; }

  /// Activates the exchange of the audio info
  void setAudioInfoActive(bool flag) { is_audio_info_active = flag; }

  /// Keeps the write sizes: each write is one BLE packet
  void setFramed(bool flag) { is_framed = flag; }

  /// Defines the requested MTU (must be called before begin)
  void setMTU(int mtu) { max_transfer_size = mtu; }

  /// Defines the size of the receive buffer and (in framed mode) the max
  /// number of packets: must be called before begin
  void setRxBufferSize(int bytes, int packets = BLE_SERIAL_PACKET_COUNT) {
    rx_buffer_size = bytes;
    rx_packet_count = packets;
  }

  /// Defines the size of the transmit buffer and (in framed mode) the max
  /// number of packets: must be called before begin
  void setTxBufferSize(int bytes, int packets = BLE_SERIAL_PACKET_COUNT) {
    tx_buffer_size = bytes;
    tx_packet_count = packets;
  }

  /// Max number of payload bytes in one BLE packet: the packets read by the
  /// client also contain the length header and a value can not be longer
  /// than 512 bytes
  int packetSize() {
    int result = getMTU() - BLE_SERIAL_MTU_OVERHEAD;
    int max = BLE_SERIAL_MAX_VALUE_LEN - BLE_SERIAL_HEADER_SIZE;
    return result < max ? result : max;
  }

  /// Converts the audio info to the string which is used in the info
  /// characteristic
  StrView toStr(AudioInfo info) {
    snprintf(audio_info_str, sizeof(audio_info_str), "%d:%d:%d",
             (int)info.sample_rate, (int)info.channels,
             (int)info.bits_per_sample);
    return StrView(audio_info_str);
  }

  /// Converts the string from the info characteristic to the audio info
  AudioInfo toInfo(const uint8_t *data, size_t len) {
    AudioInfo result;
    char str[40] = {0};
    memcpy(str, data, len < sizeof(str) - 1 ? len : sizeof(str) - 1);
    int sample_rate = 0, channels = 0, bits = 0;
    if (sscanf(str, "%d:%d:%d", &sample_rate, &channels, &bits) == 3) {
      result.sample_rate = sample_rate;
      result.channels = channels;
      result.bits_per_sample = bits;
    }
    return result;
  }

 protected:
  // disable copy constructor
  BLESerialStream(BLESerialStream const &other) = delete;
  // disable assign constructor
  void operator=(BLESerialStream const &other) = delete;
  int max_transfer_size = 0;
  int rx_buffer_size = BLE_SERIAL_BUFFER_SIZE;
  int rx_packet_count = BLE_SERIAL_PACKET_COUNT;
  int tx_buffer_size = BLE_SERIAL_BUFFER_SIZE;
  int tx_packet_count = BLE_SERIAL_PACKET_COUNT;
  bool is_audio_info_active = false;
  bool is_audio_info_defined = false;
  bool is_framed = false;
  char audio_info_str[40];

  // Bluetooth LE GATT UUIDs: change them with the setters if required
  const char *BLE_SERIAL_SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
  // read by the client: server -> client
  const char *BLE_CH1_UUID = "6e400002-b5a3-f393-e0a9-e50e24dcca9e";
  // written by the client: client -> server
  const char *BLE_CH2_UUID = "6e400003-b5a3-f393-e0a9-e50e24dcca9e";
  const char *BLE_INFO_UUID = "6e400004-b5a3-f393-e0a9-e50e24dcca9e";

  /// Effective MTU
  virtual int getMTU() { return max_transfer_size; }

  /// Provides the audio info to the other side
  virtual void writeAudioInfo(StrView info) = 0;

  /// Process the audio info which was received from the other side
  void receiveAudioInfo(const uint8_t *data, size_t size) {
    if (!is_audio_info_active || size == 0) return;
    AudioInfo ai = toInfo(data, size);
    if (!ai) {
      LOGW("BLESerial: invalid audio info");
      return;
    }
    // no echo to the other side
    AudioStream::setAudioInfo(ai);
  }

  /// Copies the payload length into the 2 byte header
  static void setHeader(uint8_t *packet, int len) {
    packet[0] = len & 0xFF;
    packet[1] = (len >> 8) & 0xFF;
  }

  /// Determines the payload length from the 2 byte header
  static int getHeader(const uint8_t *packet, int packetLen) {
    if (packetLen < BLE_SERIAL_HEADER_SIZE) return 0;
    int len = packet[0] | (packet[1] << 8);
    int max = packetLen - BLE_SERIAL_HEADER_SIZE;
    if (len > max) {
      LOGW("BLESerial: incomplete packet: %d of %d bytes", max, len);
      len = max;
    }
    return len;
  }
};

/**
 * @brief Generic BLE serial server logic which is independent of the BLE
 * API. In BLE terminology the server is a Peripheral: the client writes the
 * data which is received with readBytes() and reads the data which was
 * provided with write().
 *
 * The data which is read by the client contains a 2 byte length header.
 * @ingroup communications
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class BLESerialServerBase : public BLESerialStream {
 public:
  BLESerialServerBase(int mtu) : BLESerialStream(mtu) {}

  /// Starts the BLE server with the indicated name
  virtual bool begin(const char *name) = 0;

  size_t readBytes(uint8_t *data, size_t len) override {
    poll();
    return rx_buffer.read(data, len);
  }

  int available() override {
    poll();
    return rx_buffer.available();
  }

  size_t write(const uint8_t *data, size_t len) override {
    LOGD("BLESerialServer::write: %d", (int)len);
    if (!connected()) return 0;
    setupBuffers();
    if (is_framed && (int)len > packetSize()) {
      LOGE("BLESerial: frame of %d bytes > max %d", (int)len, packetSize());
      return 0;
    }
    return tx_buffer.write(data, len);
  }

  int availableForWrite() override {
    if (!connected()) return 0;
    setupBuffers();
    int result = tx_buffer.availableForWrite();
    if (is_framed) return result < packetSize() ? 0 : packetSize();
    // make sure we copy always a consistent amount of data
    return result < DEFAULT_BUFFER_SIZE ? 0 : result;
  }

 protected:
  BLESerialPacketBuffer rx_buffer;
  BLESerialPacketBuffer tx_buffer;
  /// max time in ms to wait for free space in the receive buffer
  int receive_timeout_ms = 0;

  /// Process the BLE events (if necessary)
  virtual void poll() {}

  void setupBuffers() {
    if (!rx_buffer.isAllocated()) {
      LOGI("BLESerial: buffers %d / %d bytes", rx_buffer_size, tx_buffer_size);
      rx_buffer.resize(rx_buffer_size, rx_packet_count, is_framed);
      tx_buffer.resize(tx_buffer_size, tx_packet_count, is_framed);
    }
  }

  /// Called by the implementation when the client has written data
  void receiveData(const uint8_t *data, size_t len) {
    if (len == 0) return;
    setupBuffers();
    uint32_t start = millis();
    while (rx_buffer.availableForWrite() < (int)len) {
      if (millis() - start >= (uint32_t)receive_timeout_ms) {
        LOGW("BLESerial: receive buffer full: %d bytes lost", (int)len);
        return;
      }
      // wait for the buffer to get freed up
      delay(5);
    }
    rx_buffer.write(data, len);
  }

  /// Called by the implementation to fill the next packet which is read by
  /// the client: the result contains the length header
  int provideData(uint8_t *packet, int maxLen) {
    setupBuffers();
    int max = maxLen - BLE_SERIAL_HEADER_SIZE;
    if (max > packetSize()) max = packetSize();
    int len = 0;
    if (max > 0 && tx_buffer.available() > 0) {
      len = tx_buffer.read(packet + BLE_SERIAL_HEADER_SIZE, max);
    }
    setHeader(packet, len);
    return len + BLE_SERIAL_HEADER_SIZE;
  }

  /// Called by the implementation when the client has disconnected
  void onDisconnected() {
    // do not send outdated data to the next client
    tx_buffer.clear();
  }
};

/**
 * @brief Generic BLE serial client logic which is independent of the BLE
 * API. In BLE terminology the client is a Central: it scans for a server
 * that provides the service UUID and connects to it.
 * @ingroup communications
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class BLESerialClientBase : public BLESerialStream {
 public:
  BLESerialClientBase(int mtu) : BLESerialStream(mtu) {}

  /// Starts the BLE client and waits at most the indicated seconds for the
  /// connection to the server. With 0 seconds we connect in the background.
  bool begin(const char *localName, int seconds) {
    TRACEI();
    if (!startClient(localName)) {
      LOGE("BLESerial: starting BLE failed");
      return false;
    }
    is_started = true;
    uint32_t start = millis();
    while (!connected() && millis() - start < 1000ul * seconds) {
      delay(10);
    }
    return seconds == 0 || connected();
  }

  size_t readBytes(uint8_t *data, size_t len) override {
    TRACED();
    if (rx_buffer.available() == 0) {
      if (!connected()) return 0;
      receivePacket();
    }
    return rx_buffer.read(data, len);
  }

  int available() override {
    int result = rx_buffer.available();
    if (result > 0) return result;
    // we need to read from the server to know more
    return connected() ? packetSize() : 0;
  }

  size_t write(const uint8_t *data, size_t len) override {
    TRACED();
    if (!connected()) return 0;
    if (is_framed) {
      if ((int)len > packetSize()) {
        LOGE("BLESerial: frame of %d bytes > max %d", (int)len, packetSize());
        return 0;
      }
      return sendPacket(data, len) ? len : 0;
    }
    // send only packets with the max size
    for (size_t j = 0; j < len; j++) {
      write_buffer.write(data[j]);
      if (write_buffer.isFull()) {
        if (!sendPacket(write_buffer.data(), write_buffer.available())) {
          write_buffer.reset();
          return j + 1;
        }
        write_buffer.reset();
      }
    }
    return len;
  }

  /// Sends the buffered data which does not fill a complete packet
  void flush() override {
    if (write_buffer.available() > 0 && connected()) {
      sendPacket(write_buffer.data(), write_buffer.available());
    }
    write_buffer.reset();
  }

  int availableForWrite() override {
    if (!connected()) return 0;
    return is_framed ? packetSize() : DEFAULT_BUFFER_SIZE;
  }

  bool connected() override {
    if (!is_started) return false;
    if (isConnected()) return true;
    if (!connect()) return false;
    onConnected();
    return true;
  }

  /// Waits the indicated ms after each write
  void setWriteThrottle(int ms) { write_throttle = ms; }

  /// Writes with response: slower but with flow control
  void setConfirmWrite(bool flag) { write_confirmation_flag = flag; }

 protected:
  BLESerialPacketBuffer rx_buffer;
  SingleBuffer<uint8_t> write_buffer{0};
  Vector<uint8_t> read_packet{0};
  int write_throttle = 0;
  bool write_confirmation_flag = false;
  bool is_started = false;

  /// Starts BLE and the scanning for the server
  virtual bool startClient(const char *localName) = 0;
  /// Returns true if we are connected
  virtual bool isConnected() = 0;
  /// Connects to the server if it was found: returns true if successful
  virtual bool connect() = 0;
  /// Reads the next packet (incl. header) from the server: returns the size
  virtual int readPacket(uint8_t *data, int maxLen) = 0;
  /// Writes a packet to the server
  virtual bool writePacket(const uint8_t *data, int len) = 0;
  /// Requests the audio info from the server: returns true if successful
  virtual bool readAudioInfo() = 0;

  /// Common setup after the connection was established
  void onConnected() {
    TRACEI();
    rx_buffer.resize(rx_buffer_size, rx_packet_count, is_framed);
    rx_buffer.clear();
    write_buffer.resize(packetSize());
    write_buffer.reset();
    // the server might use a bigger MTU than we requested
    read_packet.resize(BLE_SERIAL_MAX_VALUE_LEN);
    if (is_audio_info_active) {
      // the side that defined the audio info is the source
      if (is_audio_info_defined) {
        writeAudioInfo(toStr(audioInfo()));
      } else {
        readAudioInfo();
      }
    }
  }

  /// Reads the next packet from the server into the receive buffer
  void receivePacket() {
    int len = readPacket(read_packet.data(), read_packet.size());
    int payload = getHeader(read_packet.data(), len);
    if (payload > 0) {
      rx_buffer.write(read_packet.data() + BLE_SERIAL_HEADER_SIZE, payload);
    }
  }

  bool sendPacket(const uint8_t *data, size_t len) {
    bool rc = writePacket(data, len);
    if (write_throttle > 0) delay(write_throttle);
    return rc;
  }
};

}  // namespace audio_tools
