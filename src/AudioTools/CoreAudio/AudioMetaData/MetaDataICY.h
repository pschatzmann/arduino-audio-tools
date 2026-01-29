#pragma once
#include "AbstractMetaData.h"
#include "AudioTools/Communication/HTTP/AbstractURLStream.h"
#include "AudioTools/CoreAudio/AudioBasic/StrView.h"
#include "AudioToolsConfig.h"

#ifndef AUDIOTOOLS_METADATA_ICY_ASCII_ONLY
#define AUDIOTOOLS_METADATA_ICY_ASCII_ONLY true
#endif

namespace audio_tools {

/**
 * @defgroup metadata-icy ICY
 * @ingroup metadata
 * @brief Icecast/Shoutcast Metadata
 **/

/**
 * @brief Icecast/Shoutcast Metadata Handling.
 * Metadata class which splits the data into audio and metadata. The result is
 * provided via callback methods. see
 * https://www.codeproject.com/Articles/11308/SHOUTcast-Stream-Ripper
 * @ingroup metadata-icy
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class MetaDataICY : public AbstractMetaData {
  enum Status { ProcessData, ProcessMetaData, SetupSize };

 public:
  MetaDataICY() = default;

  /// We just process the Metadata and ignore the audio info
  MetaDataICY(int metaint) { setIcyMetaInt(metaint); }

  virtual ~MetaDataICY() {}

  /// Defines the ICE metaint value which is provided by the web call!
  virtual void setIcyMetaInt(int value) override {
    this->mp3_blocksize = value;
  }

  /// Defines the metadata callback function
  virtual void setCallback(void (*fn)(MetaDataType info, const char* str,
                                      int len)) override {
    callback = fn;
  }

  /// Defines the audio callback function
  virtual void setAudioDataCallback(void (*fn)(const uint8_t* str, int len),
                                    int bufferLen = 1024) {
    dataBuffer = new uint8_t[bufferLen];
    dataCallback = fn;
    dataLen = 0;
    dataPos = 0;
  }

  /// Resets all counters and restarts the prcessing
  virtual bool begin() override {
    clear();
    LOGI("mp3_blocksize: %d", mp3_blocksize);
    return true;
  }

  /// Resets all counters and restarts the prcessing
  virtual void end() override { clear(); }

  /// Writes the data in order to retrieve the metadata and perform the
  /// corresponding callbacks
  virtual size_t write(const uint8_t* data, size_t len) override {
    if (callback != nullptr) {
      for (size_t j = 0; j < len; j++) {
        processChar((char)data[j]);
      }
    }
    return len;
  }

  /// Returns the actual status of the state engine for the current byte
  virtual Status status() { return currentStatus; }

  /// returns true if the actual bytes is an audio data byte (e.g.mp3)
  virtual bool isData() { return currentStatus == ProcessData; }

  /// Returns true if the ICY stream contains metadata
  virtual bool hasMetaData() { return this->mp3_blocksize > 0; }

  /// provides the metaint
  virtual int metaInt() { return mp3_blocksize; }

  /// character based state engine
  virtual void processChar(char ch) {
    switch (nextStatus) {
      case ProcessData:
        currentStatus = ProcessData;
        processData(ch);

        // increment data counter and determine next status
        ++totalData;
        if (totalData >= mp3_blocksize) {
          LOGI("Data ended")
          totalData = 0;
          nextStatus = SetupSize;
        }
        break;

      case SetupSize:
        currentStatus = SetupSize;
        totalData = 0;
        metaDataPos = 0;
        metaDataLen = metaSize(ch);
        LOGI("metaDataLen: %d", metaDataLen);
        if (metaDataLen > 0) {
          if (metaDataLen > 200) {
            LOGI("Unexpected metaDataLen -> processed as data");
            nextStatus = ProcessData;
          } else {
            LOGI("Metadata found");
            setupMetaData(metaDataLen);
            nextStatus = ProcessMetaData;
          }
        } else {
          LOGI("Data found");
          nextStatus = ProcessData;
        }
        break;

      case ProcessMetaData:
        currentStatus = ProcessMetaData;
        metaData[metaDataPos++] = ch;
        if (metaDataPos >= metaDataLen) {
          processMetaData(metaData.data(), metaDataLen);
          LOGI("Metadata ended")
          nextStatus = ProcessData;
        }
        break;
    }
  }

  /// Sets whether to only accept ASCII characters in metadata (default is true)
  void setAsciiOnly(bool value) { is_ascii = value; }

 protected:
  Status nextStatus = ProcessData;
  Status currentStatus = ProcessData;
  void (*callback)(MetaDataType info, const char* str, int len) = nullptr;
  Vector<char> metaData{0};
  int totalData = 0;
  int mp3_blocksize = 0;
  int metaDataMaxLen = 0;
  int metaDataLen = 0;
  int metaDataPos = 0;
  bool is_data;  // indicates if the current byte is a data byte
  // data
  uint8_t* dataBuffer = nullptr;
  void (*dataCallback)(const uint8_t* str, int len) = nullptr;
  int dataLen = 0;
  int dataPos = 0;
  bool is_ascii = AUDIOTOOLS_METADATA_ICY_ASCII_ONLY;

  virtual void clear() {
    nextStatus = ProcessData;
    totalData = 0;
    metaData.resize(0);
    metaDataLen = 0;
    metaDataPos = 0;
    dataLen = 0;
    dataPos = 0;
  }

  /// determines the meta data size from the size byte
  virtual int metaSize(uint8_t metaSize) { return metaSize * 16; }

  //// checks whether the string is printable
  virtual bool isPrintable(const char* str, int l) {
    int remain = 0;
    for (int j = 0; j < l; j++) {
      uint8_t ch = str[j];
      if (remain) {
        if (ch < 0x80 || ch > 0xbf) {
          LOGD("Invalid UTF-8 continuation byte: 0x%02X at pos %d", ch, j);
          return false;
        }
        remain--;
      } else {
        if (ch < 0x80) {  // ASCII
          // Allow '\0' as printable (do not treat as control char)
          if (ch != '\n' && ch != '\r' && ch != '\t' && ch != 0 && (ch < 32 || ch == 127)) {
            LOGD("Non-printable ASCII character: 0x%02X at pos %d", ch, j);
            return false;  // control chars except allowed ones
          }
        } else if (!is_ascii) {
          if (ch >= 0xc2 && ch <= 0xdf)
            remain = 1;
          else if (ch >= 0xe0 && ch <= 0xef)
            remain = 2;
          else if (ch >= 0xf0 && ch <= 0xf4)
            remain = 3;
          else {
            LOGD("Invalid UTF-8 lead byte: 0x%02X at pos %d", ch, j);
            return false;
          }
        } else {
          LOGD("Non-ASCII character not allowed: 0x%02X at pos %d", ch, j);
          return false;
        }
      }
    }
    if (remain != 0) {
      LOGD("Incomplete UTF-8 sequence at end of string");
    }
    return remain == 0;
  }

  /// allocates the memory to store the metadata / we support changing sizes
  virtual void setupMetaData(int meta_size) {
    TRACED();
    metaData.resize(meta_size + 1);
    metaDataMaxLen = meta_size;
    memset(metaData.data(), 0, meta_size + 1);
  }

  /// e.g. StreamTitle=' House Bulldogs - But your love (Radio
  /// Edit)';StreamUrl='';
  virtual void processMetaData(char* metaData, int len) {
    // CHECK_MEMORY();
    TRACED();
    metaData[len] = 0;
    if (isPrintable(metaData, len)) {
      LOGI("%s", metaData);
      StrView meta(metaData, len + 1, len);
      int start = meta.indexOf("StreamTitle=");
      if (start >= 0) {
        start += 12;
      }
      int end = meta.indexOf("';");
      if (start >= 0 && end > start) {
        metaData[end] = 0;
        if (callback != nullptr) {
          callback(Title, (const char*)metaData + start + 1, end - start);
        }
      }
      // CHECK_MEMORY();
    } else {
      // CHECK_MEMORY();
      // Don't print corrupted binary data - could contain terminal control
      // codes
      LOGW("Unexpected Data: corrupted metadata block rejected (len=%d)", len);
      // Signal corruption to application so it can disconnect/reconnect
      if (callback != nullptr) {
        callback(Corrupted, nullptr, len);
      }
    }
  }

  /// Collects the data in a buffer and executes the callback when the buffer is
  /// full
  virtual void processData(char ch) {
    if (dataBuffer != nullptr) {
      dataBuffer[dataPos++] = ch;
      // output data via callback
      if (dataPos >= dataLen) {
        dataCallback(dataBuffer, dataLen);
        dataPos = 0;
      }
    }
  }
};

/**
 * @brief Resolve icy-metaint from HttpRequest and execute metadata callbacks
 * @ingroup metadata-icy
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class ICYUrlSetup {
 public:
  /// Fills the metaint from the Http Request and executes metadata callbacks on
  /// http reply parameters
  int setup(AbstractURLStream& url) {
    TRACED();
    p_url = &url;
    const char* iceMetaintStr = url.getReplyHeader("icy-metaint");
    if (iceMetaintStr) {
      LOGI("icy-metaint: %s", iceMetaintStr);
    } else {
      LOGE("icy-metaint not defined");
    }
    StrView value(iceMetaintStr);
    int iceMetaint = value.toInt();
    return iceMetaint;
  }

  /// Executes the metadata callbacks with data provided from the http request
  /// result parameter
  void executeCallback(void (*callback)(MetaDataType info, const char* str,
                                        int len)) {
    TRACEI();
    if (callback == nullptr) {
      LOGW("callback not defined")
    }
    if (p_url == nullptr) {
      LOGW("http not defined")
    }
    // Callbacks filled from url reply for icy
    if (callback != nullptr && p_url != nullptr) {
      // handle icy parameters
      StrView genre(p_url->getReplyHeader("icy-genre"));
      if (!genre.isEmpty()) {
        callback(Genre, genre.c_str(), genre.length());
      }

      StrView descr(p_url->getReplyHeader("icy-description"));
      if (!descr.isEmpty()) {
        callback(Description, descr.c_str(), descr.length());
      }

      StrView name(p_url->getReplyHeader("icy-name"));
      if (!name.isEmpty()) {
        callback(Name, name.c_str(), name.length());
      }
    }
  }


 protected:
  AbstractURLStream* p_url = nullptr;
};

}  // namespace audio_tools
