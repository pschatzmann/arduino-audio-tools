#pragma once
#include "AudioTools/CoreAudio/Buffers.h"

namespace audio_tools {

/**
 * @brief Buffer implementation that stores and retrieves data from a Redis server using Arduino Client.
 *
 * This buffer uses a Redis list as a circular buffer and batches read/write operations for efficiency.
 * Individual write/read calls are buffered locally using SingleBuffer and only sent to Redis in bulk
 * when writeArray/readArray is called or when the buffer is full/empty. This reduces network overhead
 * and improves performance for streaming scenarios.
 *
 * - Uses RPUSH for writing and LRANGE/LTRIM for reading from Redis.
 * - All Redis commands are constructed using the RESP protocol and sent via the Arduino Client API.
 * - The buffer size for local batching can be configured via the constructor.
 * - Supports automatic expiration of the Redis key after a specified number of seconds.
 *
 * @tparam T Data type to buffer (e.g., uint8_t, int16_t)
 * @ingroup buffers
 */
template <typename T>
class RedisBuffer : public BaseBuffer<T> {
 public:
  /**
   * @brief Constructs a RedisBuffer.
   * @param client Reference to a connected Arduino Client (e.g., WiFiClient, EthernetClient).
   * @param key    Redis key to use for the buffer (list).
   * @param max_size Maximum number of elements in the buffer.
   * @param local_buf_size Size of the local buffer for batching (default: 32).
   * @param expire_seconds Number of seconds after which the Redis key should expire (0 = no expiration).
   */
  RedisBuffer(Client& client, const String& key, size_t max_size, size_t local_buf_size = 32, int expire_seconds = 0)
      : client(client), key(key), max_size(max_size), local_buf_size(local_buf_size), expire_seconds(expire_seconds),
        write_buf(local_buf_size), read_buf(local_buf_size) {}

  /**
   * @brief Sets the expiration time (in seconds) for the Redis key.
   *        The expiration will be refreshed on every write/flush.
   * @param seconds Expiration time in seconds (0 = no expiration).
   */
  void setExpire(int seconds) { expire_seconds = seconds; }

  /**
   * @brief Buffers a single value for writing to Redis.
   *        Data is only sent to Redis when the local buffer is full or writeArray is called.
   * @param data Value to write.
   * @return true if buffered successfully.
   */
  bool write(T data) override {
    write_buf.write(data);
    if (write_buf.isFull()) {
      flushWrite();
    }
    return true;
  }

  /**
   * @brief Writes multiple values to Redis in one batch.
   *        Flushes any pending writes before sending the new data.
   * @param data Array of values to write.
   * @param len  Number of values to write.
   * @return Number of values written.
   */
  int writeArray(const T data[], int len) override {
    flushWrite(); // flush any pending writes first
    int written = 0;
    for (int i = 0; i < len; ++i) {
      write_buf.write(data[i]);
      if (write_buf.isFull()) {
        flushWrite();
      }
      ++written;
    }
    flushWrite(); // flush remaining
    return written;
  }

  /**
   * @brief Reads a single value from the buffer.
   *        If the local read buffer is empty, fetches a batch from Redis.
   *        Flushes any pending writes before reading.
   * @param result Reference to store the read value.
   * @return true if a value was read, false otherwise.
   */
  bool read(T& result) override {
    if (read_buf.isEmpty()) {
      flushWrite(); // flush any pending writes before reading
      fillReadBuffer();
    }
    if (read_buf.isEmpty()) return false;
    read_buf.read(result);
    return true;
  }

  /**
   * @brief Reads multiple values from the buffer in one batch.
   *        Flushes any pending writes before reading.
   * @param data Array to store the read values.
   * @param len  Maximum number of values to read.
   * @return Number of values actually read.
   */
  int readArray(T data[], int len) override {
    flushWrite(); // flush any pending writes before reading
    int read_count = 0;
    while (read_count < len) {
      if (read_buf.isEmpty()) {
        fillReadBuffer();
        if (read_buf.isEmpty()) break; // nothing left in Redis
      }
      read_buf.read(data[read_count++]);
    }
    return read_count;
  }

  /**
   * @brief Peeks at the next value in the buffer without removing it.
   *        If the local read buffer is empty, fetches a batch from Redis.
   *        Flushes any pending writes before peeking.
   * @param result Reference to store the peeked value.
   * @return true if a value was available, false otherwise.
   */
  bool peek(T& result) override {
    if (read_buf.isEmpty()) {
      flushWrite();
      fillReadBuffer();
    }
    if (read_buf.isEmpty()) return false;
    return read_buf.peek(result);
  }

  /**
   * @brief Clears the buffer both locally and on the Redis server.
   *        Flushes any pending writes before clearing.
   */
  void reset() override {
    flushWrite();
    String cmd = redisCommand("DEL", key);
    sendCommand(cmd);
    read_buf.reset();
    write_buf.reset();
  }

  /**
   * @brief Returns the number of elements available to read (local + Redis).
   *        Flushes any pending writes before checking.
   * @return Number of available elements.
   */
  int available() override {
    flushWrite();
    String cmd = redisCommand("LLEN", key);
    if (!sendCommand(cmd)) return 0;
    String resp = readResponse();
    return resp.toInt() + read_buf.available();
  }

  /**
   * @brief Returns the number of elements that can be written before reaching max_size.
   * @return Number of available slots for writing.
   */
  int availableForWrite() override {
    return max_size - available();
  }

  /**
   * @brief Returns the address of the start of the physical read buffer (not supported).
   * @return nullptr.
   */
  T* address() override { return nullptr; }

  /**
   * @brief Returns the maximum capacity of the buffer.
   * @return Maximum number of elements.
   */
  size_t size() override { return max_size; }

  /**
   * @brief Resizes the maximum buffer size.
   * @param size New maximum size.
   * @return true if resized.
   */
  bool resize(int size) override { 
    max_size = size;
    return true;
  }

 protected:
  Client& client;                ///< Reference to the Arduino Client for Redis communication.
  String key;                    ///< Redis key for the buffer.
  size_t max_size;               ///< Maximum number of elements in the buffer.
  size_t local_buf_size;         ///< Local buffer size for batching.
  int expire_seconds = 0;        ///< Expiration time in seconds (0 = no expiration).
  SingleBuffer<T> write_buf;     ///< Local buffer for pending writes.
  SingleBuffer<T> read_buf;      ///< Local buffer for pending reads.

  /**
   * @brief Constructs a Redis command in RESP format.
   * @param cmd  Redis command (e.g., "RPUSH").
   * @param arg1 First argument.
   * @param arg2 Second argument.
   * @param arg3 Third argument.
   * @return Command string in RESP format.
   */
  String redisCommand(const String& cmd, const String& arg1 = "", const String& arg2 = "", const String& arg3 = "") {
    String out = "*" + String(1 + (arg1.length() > 0) + (arg2.length() > 0) + (arg3.length() > 0)) + "\r\n";
    out += "$" + String(cmd.length()) + "\r\n" + cmd + "\r\n";
    if (arg1.length()) out += "$" + String(arg1.length()) + "\r\n" + arg1 + "\r\n";
    if (arg2.length()) out += "$" + String(arg2.length()) + "\r\n" + arg2 + "\r\n";
    if (arg3.length()) out += "$" + String(arg3.length()) + "\r\n" + arg3 + "\r\n";
    return out;
  }

  /**
   * @brief Sends a command to the Redis server.
   * @param cmd Command string in RESP format.
   * @return true if sent successfully.
   */
  bool sendCommand(const String& cmd) {
    client.print(cmd);
    client.flush();
    return true;
  }

  /**
   * @brief Reads a single line response from the Redis server.
   * @return Response string.
   */
  String readResponse() {
    String line = "";
    unsigned long start = millis();
    while (client.connected() && (millis() - start < 1000)) {
      if (client.available()) {
        char c = client.read();
        if (c == '\r') continue;
        if (c == '\n') break;
        line += c;
      }
    }
    // Remove RESP prefix if present
    if (line.length() && (line[0] == ':' || line[0] == '$' || line[0] == '+')) {
      int idx = 1;
      while (idx < line.length() && (line[idx] < '0' || line[idx] > '9')) ++idx;
      return line.substring(idx);
    }
    return line;
  }

  /**
   * @brief Flushes buffered writes to Redis using RPUSH and sets expiration if configured.
   */
  void flushWrite() {
    if (write_buf.isEmpty()) return;
    // Use RPUSH with multiple arguments
    String cmd = "*" + String(2 + write_buf.available()) + "\r\n";
    cmd += "$5\r\nRPUSH\r\n";
    cmd += "$" + String(key.length()) + "\r\n" + key + "\r\n";
    T value;
    for (int i = 0; i < write_buf.available(); ++i) {
      write_buf.peek(value); // always peeks the first
      String sval = String(value);
      cmd += "$" + String(sval.length()) + "\r\n" + sval + "\r\n";
      write_buf.read(value); // remove after sending
    }
    sendCommand(cmd);

    // Set expiration if needed
    if (expire_seconds > 0) {
      String expireCmd = redisCommand("EXPIRE", key, String(expire_seconds));
      sendCommand(expireCmd);
    }
  }

  /**
   * @brief Fills the local read buffer from Redis using LRANGE.
   *        After reading, removes the items from Redis using LTRIM.
   */
  void fillReadBuffer() {
    // Read up to local_buf_size items from Redis
    String cmd = redisCommand("LRANGE", key, "0", String(local_buf_size - 1));
    if (!sendCommand(cmd)) return;
    // Parse RESP array
    int count = 0;
    String line;
    while (client.connected() && count < local_buf_size) {
      line = "";
      unsigned long start = millis();
      while (client.connected() && (millis() - start < 1000)) {
        if (client.available()) {
          char c = client.read();
          if (c == '\r') continue;
          if (c == '\n') break;
          line += c;
        }
      }
      if (line.length() == 0) break;
      if (line[0] == '$') {
        int len = line.substring(1).toInt();
        if (len <= 0) break;
        String value = "";
        while (value.length() < len) {
          if (client.available()) value += (char)client.read();
        }
        client.read(); // \r
        client.read(); // \n
        read_buf.write((T)value.toInt());
        ++count;
      }
    }
    // Remove the read items from Redis
    if (count > 0) {
      String ltrimCmd = redisCommand("LTRIM", key, String(count), "-1");
      sendCommand(ltrimCmd);
    }
  }
};

}