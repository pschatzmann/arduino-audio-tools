#pragma once

#include "HttpHeader.h"
#include "HttpLineReader.h"

namespace audio_tools {

/**
 * @brief Http might reply with chunks. So we need to dechunk the data.
 * see https://en.wikipedia.org/wiki/Chunked_transfer_encoding
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class HttpChunkReader : public HttpLineReader {
 public:
  /// default constructor
  HttpChunkReader() {
    open_chunk_len = 0;
    has_ended = false;
  }

  /// constructor for processing final header information
  HttpChunkReader(HttpReplyHeader& header) {
    http_header_ptr = &header;
    open_chunk_len = 0;
    has_ended = false;
  }

  /// opens the chunk reader and reads the first chunk length
  bool open(Client& client) {
    LOGD("HttpChunkReader: %s", "open");
    has_ended = false;
    return readChunkLen(client);
  }

  /// reads a block of data from the chunks
  int read(Client& client, uint8_t* str, int len) {
    LOGD("HttpChunkReader: %s", "read");
    if (has_ended && open_chunk_len == 0) return 0;

    // wait for data
    auto start_time = millis();
    client.setTimeout(timeout);
    int result = client.peek();
    auto time_eff = millis() - start_time;
    if (result == -1 && time_eff < timeout) {
      LOGE("Timout %d not working - waited: %d ms", timeout);
    }


    // read the chunk data - but not more then available
    int read_max = len < open_chunk_len ? len : open_chunk_len;
    int len_processed = client.read(str, read_max);
    if (len_processed == -1) {
      LOGE("HttpChunkReader: client.read result -1, open: %d for timeout %d",open_chunk_len, timeout);
      return -1;
    }
    // update current unprocessed chunk
    open_chunk_len -= len_processed;

    // remove traling CR LF from data
    if (open_chunk_len <= 0) {
      removeCRLF(client);
      readChunkLen(client);
    }

    return len_processed;
  }

  /// reads a single line from the chunks
  int readln(Client& client, uint8_t* str, int len,
            bool incl_nl = true) {
    LOGD("HttpChunkReader: %s", "readln");
    if (has_ended && open_chunk_len == 0) return 0;

    int read_max = len < open_chunk_len ? len : open_chunk_len;
    int len_processed = readlnInternal(client, str, read_max, incl_nl);
    if (len_processed == -1) {
      LOGD("HttpChunkReader: readln result -1");
      return -1;
    }
    open_chunk_len -= len_processed;

    // the chunks are terminated by a final CRLF
    if (open_chunk_len <= 0) {
      removeCRLF(client);
      readChunkLen(client);
    }

    return len_processed;
  }

  /// returns the number of bytes which are still available in the current chunk
  int available() {
    int result = has_ended ? 0 : open_chunk_len;
    LOGD("HttpChunkReader: available=>%d", result);
    return result;
  }

  /// Timout is just used for logging
  void setTimeout(int timeoutMs) {
    timeout = timeoutMs;
  }

 protected:
  int open_chunk_len = 0;
  bool has_ended = false;
  HttpReplyHeader* http_header_ptr;
  int timeout = 0;

  void removeCRLF(Client& client) {
    LOGD("HttpChunkReader: %s", "removeCRLF");

    // remove traling CR LF from data
    if (client.peek() == '\r') {
      LOGD("HttpChunkReader: %s", "removeCR");
      client.read();
    }
    if (client.peek() == '\n') {
      LOGD("HttpChunkReader: %s", "removeLF");
      client.read();
    }
  }

  // we read the chunk length which is indicated as hex value
  bool readChunkLen(Client& client) {
    LOGD("HttpChunkReader::readChunkLen");
    uint8_t len_str[HTTP_CHUNKED_SIZE_MAX_LEN + 1] = {0};
    if (readlnInternal(client, len_str, HTTP_CHUNKED_SIZE_MAX_LEN, false) < 0) {
      LOGD("HttpChunkReader::readChunkLen readlnInternal result -1");
      has_ended = true;
      return false;
    }
    open_chunk_len = strtol((char*)len_str, nullptr, 16);

    if (open_chunk_len == 0) {
      has_ended = true;
      LOGD("HttpChunkReader::readChunkLen %s", "last chunk received");
      // processing of additinal final headers after the chunk end
      if (http_header_ptr != nullptr) {
        http_header_ptr->readExt(client);
      }
    }
    return true;
  }
};

}  // namespace audio_tools
