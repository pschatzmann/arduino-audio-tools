#pragma once

#include "AudioTools/CoreAudio/BaseStream.h"
#include "AudioTools/CoreAudio/AudioMetaData/AbstractMetaData.h" // for MetaDataType
#include "HttpTypes.h"
#include "HttpRequest.h"
#include "AudioClient.h"

namespace audio_tools {

/**
 * @brief Abstract Base class for all URLStream implementations
 * @author Phil Schatzmann
 * @ingroup http
 * @copyright GPLv3
 */
class AbstractURLStream : public AudioStream {
 public:
  // executes the URL request
  virtual bool begin(const char* urlStr, const char* acceptMime = nullptr,
                     MethodID action = GET, const char* reqMime = "",
                     const char* reqData = "") = 0;
  // ends the request
  virtual void end() override = 0;

  /// Adds/Updates a request header
  virtual void addRequestHeader(const char* header, const char* value) = 0;

  /// Provides reply header information
  virtual const char* getReplyHeader(const char* header) = 0;

  // only the ICYStream supports this
  virtual bool setMetadataCallback(void (*fn)(MetaDataType info,
                                              const char* str, int len)) {
    return false;
  }
  /// Writes are not supported
  int availableForWrite() override { return 0; }

  /// Sets the ssid that will be used for logging in (when calling begin)
  virtual void setSSID(const char* ssid) = 0;

  /// Sets the password that will be used for logging in (when calling begin)
  virtual void setPassword(const char* password) = 0;

  /// if set to true, it activates the power save mode which comes at the cost
  /// of performance! - By default this is deactivated. ESP32 Only!
  virtual void setPowerSave(bool ps) = 0;

  /// Define the Root PEM Certificate for SSL
  virtual void setCACert(const char* cert) = 0;

  /// provides access to the HttpRequest
  virtual HttpRequest& httpRequest() = 0;

  /// (Re-)defines the client
  virtual void setClient(Client& clientPar) = 0;

  /// Add Connection: close to request header
  virtual void setConnectionClose(bool flag) = 0;

  /// Provides the url as string
  virtual const char* urlStr() = 0;

  /// Total amout of data that was consumed so far
  virtual size_t totalRead() = 0;

  /// Provides the reported data size from the http reply
  virtual int contentLength() = 0;
 
  /// Waits the indicated time for the data to be available
  /// waits for some data - returns false if the request has failed
  virtual bool waitForData(int timeout) = 0;

};


}  // namespace audio_tools
