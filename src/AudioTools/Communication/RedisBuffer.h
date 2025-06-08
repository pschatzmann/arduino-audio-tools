#pragma once
#include "AudioTools/CoreAudio/Buffers.h"

namespace audio_tools {

/**
 * @brief Buffer implementation that stores and retrieves data from a Redis
 * server using Arduino Client.
 *
 * This buffer uses a Redis list as a circular buffer and batches read/write
 * operations for efficiency. Individual write/read calls are buffered locally
 * using SingleBuffer and only sent to Redis in bulk when writeArray/readArray
 * is called or when the buffer is full/empty. This reduces network overhead and
 * improves performance for streaming scenarios.
 *
 * - Uses RPUSH for writing and LRANGE/LTRIM for reading from Redis.
 * - All Redis commands are constructed using the RESP protocol and sent via the
 * Arduino Client API.
 * - The buffer size for local batching can be configured via the constructor.
 * - Supports automatic expiration of the Redis key after a specified number of
 * seconds.
 *
 * @tparam T Data type to buffer (e.g., uint8_t, int16_t)
 * @ingroup buffers
 */
template <typename T>
class RedisBuffer : public BaseBuffer<T> {
 public:
  /**
   * @brief Constructs a RedisBuffer.
   * @param client Reference to a connected Arduino Client (e.g., WiFiClient,
   * EthernetClient).
   * @param key    Redis key to use for the buffer (list).
   * @param max_size Maximum number of elements in the buffer.
   * @param local_buf_size Size of the local buffer for batching (default: 32).
   * @param expire_seconds Number of seconds after which the Redis key should
   * expire (0 = no expiration).
   */
  RedisBuffer(Client& client, const char* key, size_t max_size,
              size_t local_buf_size = 512, int expire_seconds = 10 * 60)
      : client(client),
        key(key),
        max_size(max_size),
        local_buf_size(local_buf_size),
        expire_seconds(expire_seconds),
        write_buf(local_buf_size),
        read_buf(local_buf_size) {}

  /**
   * @brief Sets the expiration time (in seconds) for the Redis key.
   *        The expiration will be refreshed on every write/flush.
   * @param seconds Expiration time in seconds (0 = no expiration).
   */
  void setExpire(int seconds) { expire_seconds = seconds; }

  /**
   * @brief Buffers a single value for writing to Redis.
   *        Data is only sent to Redis when the local buffer is full or
   * writeArray is called.
   * @param data Value to write.
   * @return true if buffered successfully.
   */
  bool write(T data) override {
    has_written = true;
    write_buf.write(data);
    if (write_buf.isFull()) {
      flushWrite();  // flush any pending writes first
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
    LOGI("RedisBuffer:writeArray: %d", len);
    has_written = true;
    int written = 0;
    for (int i = 0; i < len; ++i) {
      write(data[i]);
      ++written;
    }
    return written;
  }

  /**
   * @brief Reads a single value from Redis directly (no local buffer).
   *        Flushes any pending writes before reading.
   * @param result Reference to store the read value.
   * @return true if a value was read, false otherwise.
   */
  bool read(T& result) override {
    flushWrite();  // flush any pending writes before reading

    // Use LPOP to read a single value directly from Redis
    String cmd = redisCommand("LPOP", key);
    int val = sendCommand(cmd);
    if (val == 0) return false;
    result = (T)val;
    LOGI("Redis LPOP: %d", val);
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
    flushWrite();  // flush any pending writes before reading
    int read_count = 0;
    while (read_count < len) {
      if (read_buf.isEmpty()) {
        fillReadBuffer();
        if (read_buf.isEmpty()) break;  // nothing left in Redis
      }
      read_buf.read(data[read_count++]);
    }
    return read_count;
  }

  /**
   * @brief Peeks at the next value in Redis directly (no local buffer).
   *        Flushes any pending writes before peeking.
   * @param result Reference to store the peeked value.
   * @return true if a value was available, false otherwise.
   */
  bool peek(T& result) override {
    flushWrite();  // flush any pending writes before peeking

    // Use LINDEX to peek at the first value in Redis without removing it
    String cmd = redisCommand("LINDEX", key, "0");
    int val = sendCommand(cmd);
    if (val == 0) return false;
    result = (T)val;
    return true;
  }

  /**
   * @brief Clears the buffer both locally and on the Redis server.
   *        Flushes any pending writes before clearing.
   */
  void reset() override {
    flushWrite();
    String cmd = redisCommand("DEL", key);
    int rc = sendCommand(cmd);
    LOGI("Redis DEL: %d", rc);
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
    int val = sendCommand(cmd);
    LOGI("LLEN: %d", val);
    return val + read_buf.available();
  }

  /**
   * @brief Returns the number of elements that can be written before reaching
   * max_size. There are are no checks in place that would prevent that the
   * size values is exeeded: This is for information only!
   * @return Number of available slots for writing.
   */
  int availableForWrite() override { return max_size - available(); }

  /**
   * @brief Returns the address of the start of the physical read buffer (not
   * supported).
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
   *        This operation is only allowed before any write has occurred.
   * @param size New maximum size.
   * @return true if resized, false if any write has already occurred.
   */
  bool resize(int size) override {
    if (has_written) return false;
    LOGI("RedisBuffer::resize: %d", size);
    max_size = size;
    return true;
  }

 protected:
  Client& client;  ///< Reference to the Arduino Client for Redis communication.
  const char* key;         ///< Redis key for the buffer.
  size_t max_size;         ///< Maximum number of elements in the buffer.
  size_t local_buf_size;   ///< Local buffer size for batching.
  int expire_seconds = 0;  ///< Expiration time in seconds (0 = no expiration).
  SingleBuffer<T> write_buf;  ///< Local buffer for pending writes.
  SingleBuffer<T> read_buf;   ///< Local buffer for pending reads.
  bool has_written = false;   ///< True if any write operation has occurred.

  void clearResponse() {
    while (client.available()) {
      client.read();  // clear any remaining data in the buffer
    }
  }

  /**
   * @brief Constructs a Redis command in RESP format.
   * @param cmd  Redis command (e.g., "RPUSH").
   * @param arg1 First argument.
   * @param arg2 Second argument.
   * @param arg3 Third argument.
   * @return Command string in RESP format.
   */
  String redisCommand(const String& cmd, const String& arg1 = "",
                      const String& arg2 = "", const String& arg3 = "") {
    String out = "*" +
                 String(1 + (arg1.length() > 0) + (arg2.length() > 0) +
                        (arg3.length() > 0)) +
                 "\r\n";
    out += "$" + String(cmd.length()) + "\r\n" + cmd + "\r\n";
    if (arg1.length())
      out += "$" + String(arg1.length()) + "\r\n" + arg1 + "\r\n";
    if (arg2.length())
      out += "$" + String(arg2.length()) + "\r\n" + arg2 + "\r\n";
    if (arg3.length())
      out += "$" + String(arg3.length()) + "\r\n" + arg3 + "\r\n";
    return out;
  }

  /**
   * @brief Sends a command to the Redis server and returns the integer
   * response.
   * @param cmd Command string in RESP format.
   * @return Integer value from the Redis response, or -1 on error.
   */
  int sendCommand(const String& cmd) {
    if (!client.connected()) {
      LOGE("Redis not connected");
      return -1;
    }
    client.print(cmd);
    client.flush();
    return readResponse();
  }

  /**
   * @brief Reads a single line response from the Redis server and returns it as
   * an integer.
   * @return Response as int. Returns 0 if no valid integer is found.
   */
  int readResponse() {
    uint8_t buffer[128] = {};
    int n = 0;
    while (n <= 0 ) {
      n = client.read(buffer, sizeof(buffer));
    }
    buffer[n] = 0;

    // Serial.println("----");
    // Serial.println((char*)buffer);
    // Serial.println("----");

    StrView line((char*)buffer, sizeof(buffer), n);

    // We get 2 lines for commands like LPOP, so we need to skip the first
    // line
    if (line.startsWith("$")) {
      int end = line.indexOf("\n");
      line.substring(line.c_str(), end, line.length());
    }

    /// Remove any leading or trailing whitespace
    if (line.startsWith(":")) {
      line.replace(":", "");
    }

    // Serial.println("----");
    // Serial.println(line.c_str());
    // Serial.println("----");

    if (line.isEmpty()) return -1;

    return line.toInt();
  }

  /**
   * @brief Flushes buffered writes to Redis using RPUSH and sets expiration
   * if configured.
   */
  void flushWrite() {
    if (write_buf.isEmpty()) return;
    int write_size = write_buf.available();
    // Use RPUSH with multiple arguments
    String cmd = "*" + String(2 + write_size) + "\r\n";
    cmd += "$5\r\nRPUSH\r\n";
    cmd += "$" + String(strlen(key)) + "\r\n" + key + "\r\n";
    T value;
    while (!write_buf.isEmpty()) {
      write_buf.read(value);  // remove after sending
      String sval = String(value);
      cmd += "$" + String(sval.length()) + "\r\n" + sval + "\r\n";
    }
    write_buf.clear();
    int resp = sendCommand(cmd);
    LOGI("Redis RPUSH %d entries: %d", write_size, resp);

    // Set expiration if needed
    if (expire_seconds > 0) {
      String expireCmd = redisCommand("EXPIRE", key, String(expire_seconds));
      int resp = sendCommand(expireCmd);
      LOGI("Redis EXPIRE: %d", resp);
    }
  }

  /**
   * @brief Fills the local read buffer from Redis using LRANGE.
   *        After reading, removes the items from Redis using LTRIM.
   */
  void fillReadBuffer() {
    // Read up to local_buf_size items from Redis
    String cmd = redisCommand("LRANGE", key, "0", String(local_buf_size - 1));
    if (sendCommand(cmd) < 0) return;
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
        client.read();  // \r
        client.read();  // \n
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

}  // namespace audio_tools