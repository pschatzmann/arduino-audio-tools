#pragma once

#include <cctype>
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

    // read the chunk data - but not more then available
    int read_max = len < open_chunk_len ? len : open_chunk_len;
    int len_processed = client.read(str, read_max);
    if (len_processed == -1) {
      LOGI("HttpChunkReader: client.read result -1, open: %d",open_chunk_len);
      return 0;
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

 protected:
  int open_chunk_len = 0;
  bool has_ended = false;
  HttpReplyHeader* http_header_ptr = nullptr;
  int last_logged_stall_len = -1;

  // Every chunk's data is mandatorily followed by CR LF before the next
  // chunk's length line (RFC 7230 4.1). client.peek() is non-blocking, so
  // if those 2 bytes haven't physically arrived yet (common over TLS,
  // where data shows up as discrete decrypted records, or just plain
  // network jitter), a single peek() silently finds nothing and this
  // returns having consumed neither byte - readChunkLen() then reads
  // starting 1-2 bytes behind where it should, permanently desyncing
  // chunk boundary tracking for the rest of the connection (this is what
  // eventually surfaces as "readChunkLen: '' is not a valid chunk
  // length"). Wait briefly for the bytes to actually show up instead of
  // giving up on the first try.
  void removeCRLF(Client& client) {
    LOGD("HttpChunkReader: %s", "removeCRLF");
    waitAndConsume(client, '\r');
    waitAndConsume(client, '\n');
  }

  /// Waits (bounded by a short timeout) for the next byte to arrive and,
  /// if it matches `expected`, consumes it - logs (but does not throw)
  /// if a different byte or no byte at all shows up, since that means
  /// the stream is already desynced and readChunkLen() right after this
  /// will detect and report it.
  void waitAndConsume(Client& client, char expected) {
    uint32_t timeout = millis() + 2000;
    int peeked = client.peek();
    while (peeked < 0 && millis() < timeout) {
      delay(1);
      peeked = client.peek();
    }
    if (peeked == expected) {
      client.read();
    } else {
      LOGW("HttpChunkReader: removeCRLF expected '%d' but got '%d'", (int)expected, peeked);
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

    // strtol() silently returns 0 for a line that isn't a valid hex
    // number at all, which is indistinguishable from a legitimate
    // "0\r\n" last-chunk marker - if our read position has desynced from
    // the server's actual chunk boundaries (e.g. after a partial/short
    // read straddling a boundary), this line is actually raw audio
    // data, not a chunk-length line. Without this check, that garbage
    // gets accepted as "end of body", and the readExt() call below then
    // tries to parse the *following* audio bytes as HTTP trailer
    // headers too - cascading the corruption instead of surfacing it.
    bool looksLikeHex = len_str[0] != 0;
    for (uint8_t* p = len_str; *p != 0 && looksLikeHex; ++p) {
      if (!isxdigit(*p)) looksLikeHex = false;
    }
    if (!looksLikeHex) {
      LOGE("HttpChunkReader::readChunkLen: '%s' is not a valid chunk length - connection desynced", len_str);
      has_ended = true;
      open_chunk_len = 0;
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
