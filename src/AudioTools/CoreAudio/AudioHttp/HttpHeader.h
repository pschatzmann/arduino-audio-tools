#pragma once

#include "AudioTools/CoreAudio/AudioBasic/Collections.h"
#include "AudioTools/CoreAudio/AudioBasic/Str.h"
#include "AudioConfig.h"
#include "AudioClient.h" // for Client
#include "HttpLineReader.h"
#include "HttpTypes.h"
#include "Url.h"

namespace audio_tools {

// Class Configuration

// Define relevant header content
static const char* CONTENT_TYPE = "Content-Type";
static const char* CONTENT_LENGTH = "Content-Length";
static const char* CONNECTION = "Connection";
static const char* CON_CLOSE = "close";
static const char* CON_KEEP_ALIVE = "keep-alive";
static const char* TRANSFER_ENCODING = "Transfer-Encoding";
static const char* CHUNKED = "chunked";
static const char* ACCEPT = "Accept";
static const char* ACCEPT_ALL = "*/*";
static const char* SUCCESS = "Success";
static const char* USER_AGENT = "User-Agent";
static const char* DEFAULT_AGENT =
    "Mozilla/5.0 (compatible; Googlebot/2.1; +http://www.google.com/bot.html)";
static const char* HOST_C = "Host";
static const char* ACCEPT_ENCODING = "Accept-Encoding";
static const char* IDENTITY = "identity";
static const char* LOCATION = "Location";

// Http methods
static const char* methods[] = {"?",       "GET",    "HEAD",  "POST",
                                "PUT",     "DELETE", "TRACE", "OPTIONS",
                                "CONNECT", "PATCH",  nullptr};

/**
 * @brief A individual key - value header line
 *
 */
struct HttpHeaderLine {
  Str key;
  Str value;
  bool active = true;
  HttpHeaderLine(const char* k) { key = k; }
};

/**
 * @brief In a http request and reply we need to process header information.
 * With this API we can define and query the header information. The individual
 * header lines are stored in a vector. This is the common functionality for the
 * HttpRequest and HttpReplyHeader subclasses
 * @author Phil Schatzmann
 * @copyright GPLv3
 *
 */
class HttpHeader {
 public:
  HttpHeader() {
    LOGD("HttpHeader");
    // set default values
    protocol_str = "HTTP/1.1";
    url_path = "/";
    status_msg = "";
  }
  ~HttpHeader() {
    LOGD("~HttpHeader");
    clear();
  }

  // /// clears the data - usually we do not delete but we just set the active
  // flag HttpHeader& reset() {
  //     is_written = false;
  //     is_chunked = false;
  //     url_path = "/";
  //     for (auto it = lines.begin() ; it != lines.end(); ++it){
  //         (*it).active = false;
  //         (*it).value = "";
  //     }
  //     return *this;
  // }

  /// clears the data
  HttpHeader& clear() {
    is_written = false;
    is_chunked = false;
    url_path = "/";
    // delete HttpHeaderLine objects
    for (auto& ptr : lines){
      delete ptr;
    }
    lines.clear();
    return *this;
  }

  HttpHeader& put(const char* key, const char* value) {
    if (value != nullptr && strlen(value) > 0) {
      LOGD("HttpHeader::put %s %s", key, value);
      HttpHeaderLine* hl = headerLine(key);
      if (hl == nullptr) {
        if (create_new_lines){
          LOGE("HttpHeader::put - did not add HttpHeaderLine for %s", key);
        }
        return *this;
      }

      // log entry
      LOGD("HttpHeader::put -> '%s' : '%s'", key, value);

      hl->value = value;
      hl->active = true;

      if (StrView(key) == TRANSFER_ENCODING && StrView(value) == CHUNKED) {
        LOGD("HttpHeader::put -> is_chunked!!!");
        this->is_chunked = true;
      }
    } else {
      LOGD("HttpHeader::put - value ignored because it is null for %s", key);
    }
    return *this;
  }

  /// adds a new line to the header - e.g. for content size
  HttpHeader& put(const char* key, int value) {
    LOGD("HttpHeader::put %s %d", key, value);
    HttpHeaderLine* hl = headerLine(key);

    if (value > 1000) {
      LOGW("value is > 1000");
    }

    // add value
    hl->value = value;
    hl->active = true;
    LOGI("%s %s", key, hl->value.c_str());
    return *this;
  }

  /// adds a  received new line to the header
  HttpHeader& put(const char* line) {
    LOGD("HttpHeader::put -> %s", (const char*)line);
    StrView keyStr(line);
    int pos = keyStr.indexOf(":");
    char* key = (char*)line;
    key[pos] = 0;

    // usually there is a leading space - but unfurtunately not always
    const char* value = line + pos + 1;
    if (value[0] == ' ') {
      value = line + pos + 2;
    }
    return put((const char*)key, value);
  }

  // determines a header value with the key
  const char* get(const char* key) {
    for (auto& line_ptr : lines) {
      StrView trimmed{line_ptr->key.c_str()};
      trimmed.trim();
      line_ptr->key = trimmed.c_str();
      //line->key.rtrim();
      if (StrView(line_ptr->key.c_str()).equalsIgnoreCase(key)) {
        const char* result = line_ptr->value.c_str();
        return line_ptr->active ? result : nullptr;
      }
    }
    return nullptr;
  }

  // reads a single header line
  int readLine(Client& in, char* str, int len) {
    int result = reader.readlnInternal(in, (uint8_t*)str, len, false);
    LOGD("HttpHeader::readLine -> %s", str);
    return result;
  }

  // writes a lingle header line
  void writeHeaderLine(Client& out, HttpHeaderLine& header) {
    LOGD("HttpHeader::writeHeaderLine: %s", header.key.c_str());
    if (!header.active) {
      LOGD("HttpHeader::writeHeaderLine - not active");
      return;
    }
    if (header.value.c_str() == nullptr) {
      LOGD("HttpHeader::writeHeaderLine - ignored because value is null");
      return;
    }

    char* msg = tempBuffer();
    StrView msg_str(msg, HTTP_MAX_LEN);
    msg_str = header.key.c_str();
    msg_str += ": ";
    msg_str += header.value.c_str();
    msg_str += CRLF;
    out.print(msg_str.c_str());

    // remove crlf from log

    int len = strlen(msg);
    msg[len - 2] = 0;
    LOGI(" -> %s ", msg);

    // mark as processed
    // header->active = false;
  }

  const char* urlPath() { return url_path.c_str(); }

  MethodID method() { return method_id; }

  int statusCode() { return status_code; }

  const char* statusMessage() { return status_msg.c_str(); }

  bool isChunked() {
    // the value is automatically set from the reply
    return is_chunked;
  }

  /// reads the full header from the request (stream)
  bool read(Client& in) {
    LOGD("HttpHeader::read");
    // remove all existing value
    clear();

    char* line = tempBuffer();
    if (in.connected()) {
      if (in.available() == 0) {
        int count = 0;
        uint32_t timeout = millis() + timeout_ms;
        while (in.available() == 0) {
          delay(50);
          count++;
          if (count == 2) {
            LOGI("Waiting for data...");
          }
          // If we dont get an answer, we abort
          if (millis() > timeout) {
            LOGE("Request timed out after %d ms", (int)timeout_ms);
            status_code = 401;
            return false;
          }
        }
        LOGI("Data available: %d", in.available());
      }

      readLine(in, line, HTTP_MAX_LEN);
      parse1stLine(line);
      while (true) {
        int len = readLine(in, line, HTTP_MAX_LEN);
        if (len == 0 && in.available() == 0) break;
        if (isValidStatus() || isRedirectStatus()) {
          StrView lineStr(line);
          lineStr.ltrim();
          if (lineStr.isEmpty()) {
            break;
          }
          put(line);
        }
      }
    }
    return true;
  }

  /// writes the full header to the indicated HttpStreamedMultiOutput stream
  void write(Client& out) {
    LOGI("HttpHeader::write");
    write1stLine(out);
    for (auto& line_ptr : lines) {
      writeHeaderLine(out, *line_ptr);
    }
    // print empty line
    crlf(out);
    out.flush();
    is_written = true;
  }

  void setProcessed() {
    for (auto& line :lines) {
      line->active = false;
    }
  }

  /// automatically create new lines
  void setAutoCreateLines(bool is_auto_line) {
    create_new_lines = is_auto_line;
  }

  /// returns true if status code >=200 and < 300
  bool isValidStatus() { return status_code >= 200 && status_code < 300; }

  /// returns true if the status code is >=300 and < 400
  bool isRedirectStatus() { return status_code >= 300 && status_code < 400; }

  /// release static temp buffer
  void end() { temp_buffer.resize(0); }

  /// Set the timout
  void setTimeout(int timeoutMs) { timeout_ms = timeoutMs; }

  /// Provide the protocol
  const char* protocol() { return protocol_str.c_str(); }

  /// Defines the protocol: e.g. HTTP/1.1
  void setProtocol(const char* protocal) { protocol_str = protocal; }

  /// Resizes the internal read buffer
  void resize(int bufferSize){
    temp_buffer.resize(bufferSize);
  }

  /// Provides the http parameter lines
  List<HttpHeaderLine*> &getHeaderLines(){
    return lines;
  }

 protected:
  int status_code = UNDEFINED;
  bool is_written = false;
  bool is_chunked = false;
  bool create_new_lines = true;
  MethodID method_id;
  // we store the values on the heap. this is acceptable because we just have
  // one instance for the requests and one for the replys: which needs about
  // 2*100 bytes
  Str protocol_str{10};
  Str url_path{70};
  Str status_msg{20};
  List<HttpHeaderLine*> lines;
  HttpLineReader reader;
  const char* CRLF = "\r\n";
  int timeout_ms = URL_CLIENT_TIMEOUT;
  Vector<char> temp_buffer{HTTP_MAX_LEN};

  char* tempBuffer() {
    temp_buffer.clear();
    return temp_buffer.data();
  }

  // the headers need to delimited with CR LF
  void crlf(Client& out) {
    out.print(CRLF);
    LOGI(" -> %s ", "<CR LF>");
  }

  // gets or creates a header line by key
  HttpHeaderLine* headerLine(const char* key) {
    if (key != nullptr) {
      for (auto& line_ptr : lines) {
        if (line_ptr->key.c_str() != nullptr) {
          if (line_ptr->key.equalsIgnoreCase(key)) {
            line_ptr->active = true;
            return line_ptr;
          }
        }
      }
      if (create_new_lines || StrView(key).equalsIgnoreCase(CONTENT_LENGTH) ||
          StrView(key).equalsIgnoreCase(CONTENT_TYPE)) {
        HttpHeaderLine *new_line = new HttpHeaderLine(key);    
        lines.push_back(new_line);
        return new_line;
      }
    } else {
      LOGI("HttpHeader::headerLine %s", "The key must not be null");
    }
    return nullptr;
  }

  MethodID getMethod(const char* line) {
    // set action
    for (int j = 0; methods[j] != nullptr; j++) {
      const char* action = methods[j];
      int len = strlen(action);
      if (strncmp(action, line, len) == 0) {
        return (MethodID)j;
      }
    }
    return (MethodID)0;
  }

  virtual void write1stLine(Client& out) = 0;
  virtual void parse1stLine(const char* line) = 0;
};

/**
 * @brief Reading and writing of Http Requests
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class HttpRequestHeader : public HttpHeader {
 public:
  // Defines the action id, url path and http version for an request
  HttpHeader& setValues(MethodID id, const char* urlPath,
                        const char* protocol = nullptr) {
    this->method_id = id;
    this->url_path = urlPath;

    LOGD("HttpRequestHeader::setValues - path: %s", this->url_path.c_str());
    if (protocol != nullptr) {
      this->protocol_str = protocol;
    }
    return *this;
  }

  // action path protocol
  void write1stLine(Client& out) {
    LOGD("HttpRequestHeader::write1stLine");
    char* msg = tempBuffer();
    StrView msg_str(msg, HTTP_MAX_LEN);

    const char* method_str = methods[this->method_id];
    msg_str = method_str;
    msg_str += " ";
    msg_str += this->url_path.c_str();
    msg_str += " ";
    msg_str += this->protocol_str.c_str();
    msg_str += CRLF;
    out.print(msg);

    int len = strlen(msg);
    msg[len - 2] = 0;
    LOGI("-> %s", msg);
  }

  // parses the requestline
  // Request-Line = Method SP Request-URI SP HTTP-Version CRLF
  void parse1stLine(const char* line) {
    LOGD("HttpRequestHeader::parse1stLine %s", line);
    StrView line_str(line);
    int space1 = line_str.indexOf(" ");
    int space2 = line_str.indexOf(" ", space1 + 1);

    this->method_id = getMethod(line);
    this->protocol_str.substring(line_str, space2 + 1, line_str.length());
    this->url_path.substring(line_str, space1 + 1, space2);
    this->url_path.trim();

    LOGD("->method %s", methods[this->method_id]);
    LOGD("->protocol %s", protocol_str.c_str());
    LOGD("->url_path %s", url_path.c_str());
  }
};

/**
 * @brief Reading and Writing of Http Replys
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class HttpReplyHeader : public HttpHeader {
 public:
  // defines the values for the rely
  void setValues(int statusCode, const char* msg = "",
                 const char* protocol = nullptr) {
    LOGI("HttpReplyHeader::setValues");
    status_msg = msg;
    status_code = statusCode;
    if (protocol != nullptr) {
      this->protocol_str = protocol;
    }
  }

  // reads the final chunked reply headers
  void readExt(Client& in) {
    LOGI("HttpReplyHeader::readExt");
    char* line = tempBuffer();
    readLine(in, line, HTTP_MAX_LEN);
    while (strlen(line) != 0) {
      put(line);
      readLine(in, line, HTTP_MAX_LEN);
    }
  }

  // HTTP-Version SP Status-Code SP Reason-Phrase CRLF
  void write1stLine(Client& out) {
    LOGI("HttpReplyHeader::write1stLine");
    char* msg = tempBuffer();
    StrView msg_str(msg, HTTP_MAX_LEN);
    msg_str = this->protocol_str.c_str();
    msg_str += " ";
    msg_str += this->status_code;
    msg_str += " ";
    msg_str += this->status_msg.c_str();
    LOGI("-> %s", msg);
    out.print(msg);
    crlf(out);
  }

  // HTTP-Version SP Status-Code SP Reason-Phrase CRLF
  // we just update the pointers to point to the correct position in the
  // http_status_line
  void parse1stLine(const char* line) {
    LOGD("HttpReplyHeader::parse1stLine: %s", line);
    StrView line_str(line);
    int space1 = line_str.indexOf(' ', 0);
    int space2 = line_str.indexOf(' ', space1 + 1);

    // save http version
    protocol_str.substring(line_str, 0, space1);

    // find response status code after the first space
    char status_c[6];
    StrView status(status_c, 6);
    status.substring(line_str, space1 + 1, space2);
    status_code = atoi(status_c);

    // get reason-phrase after last SP
    status_msg.substring(line_str, space2 + 1, line_str.length());
  }
};

}  // namespace audio_tools