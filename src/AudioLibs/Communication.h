
#pragma once
#include <WiFiUdp.h>
#include <esp_now.h>

#include "AudioTools/AudioStreams.h"
#include "AudioTools/Buffers.h"

namespace audio_tools {

// forward declarations
class ESPNowStream;
ESPNowStream *ESPNowStreamSelf = nullptr;


/**
 * @brief ESPNow as Stream
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class ESPNowStream : public AudioStreamX {
public:
  ESPNowStream() { ESPNowStreamSelf = this; };

  /// Adds a peer to which we can send info or from which we can receive info
  bool addPeer(esp_now_peer_info_t &peer) {
    esp_err_t result = esp_now_add_peer(&peer);
    if (result != ESP_OK) {
      LOGE("addPeer: %d", result);
    }
    return result == ESP_OK;
  }

  /// Adds an array of
  template <size_t size> bool addPeers(const char *(&array)[size]) {
    bool result = true;
    for (int j = 0; j < size; j++) {
      if (!addPeer(array[j])) {
        result = false;
      }
    }
    return result;
  }

  /// Adds a peer to which we can send info or from which we can receive info
  bool addPeer(const char *address) {
    esp_now_peer_info_t peer;
    strncpy((char *)peer.peer_addr, address, ESP_NOW_ETH_ALEN);
    esp_err_t result = esp_now_add_peer(&peer);
    if (result != ESP_OK) {
      LOGE("addPeer: %d", result);
    }
    return result == ESP_OK;
  }

  /// Defines an alternative send callback
  void setSendCallback(esp_now_send_cb_t cb) { send = cb; }

  /// Defines the Receive Callback - Deactivates the readBytes and available()
  /// methods!
  void setReceiveCallback(esp_now_recv_cb_t cb) { receive = cb; }

  /// Initialization incl WIFI
  bool begin(const char *ssid, const char *password,
             wifi_mode_t mode = WIFI_STA) {
    WiFi.mode(mode);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      Serial.print('.');
      delay(1000);
    }
    Serial.println();
    return begin();
  }

  /// Initialization
  bool begin() {
    if (WiFi.status() != WL_CONNECTED) {
      LOGE("Wifi not connected");
      return false;
    }
    esp_err_t result = esp_now_init();
    if (result != ESP_OK) {
      LOGE("esp_now_init");
    } else {
      LOGI("esp_now_init: %s", address());
    }
    esp_now_register_recv_cb(receive);
    esp_now_register_send_cb(send);
    return result == ESP_OK;
  }

  /// DeInitialization
  void end() {
    if (esp_now_deinit() != ESP_OK) {
      LOGE("esp_now_deinit");
    }
  }

  /// Writes the data - sends it to all the peers
  size_t write(uint8_t *data, size_t len) {
    int open = len;
    size_t result = 0;
    while (open > 0) {
      if (available_to_write > 0) {
        size_t send_len = min(open, ESP_NOW_MAX_DATA_LEN);
        esp_err_t result = esp_now_send(nullptr, data + result, send_len);
        available_to_write = 0;
        if (result == ESP_OK) {
          open -= send_len;
          result += send_len;
        } else {
          LOGE("%d", result);
          break;
        }
      } else {
        delay(100);
      }
    }
    // reset available to write and we wait for the confirmation
    available_to_write = 0;
    return result;
  }

  /// Reeds the data from the peers
  size_t readBytes(uint8_t *data, size_t len) override {
    return buffer.readArray(data, len);
  }

  int available() override { return buffer.available(); }

  int availableForWrite() { return available_to_write; }

  /// Returns the mac address of the current ESP32
  const char *address() { return WiFi.macAddress().c_str(); }

protected:
  RingBuffer<uint8_t> buffer{1024 * 5};
  esp_now_recv_cb_t receive = default_recv_cb;
  esp_now_send_cb_t send = default_send_cb;
  size_t available_to_write = ESP_NOW_MAX_DATA_LEN;

  static void default_recv_cb(const uint8_t *mac_addr, const uint8_t *data,
                              int data_len) {
    ESPNowStreamSelf->buffer.writeArray(data, data_len);
  }

  static void default_send_cb(const uint8_t *mac_addr,
                              esp_now_send_status_t status) {
    static uint8_t first_mac[8] = {0};
    // we use the first confirming mac_addr for further confirmations and ignore
    // others
    if (first_mac[0] == 0) {
      strcpy((char *)first_mac, (char *)mac_addr);
    }
    LOGI("%s:%d", mac_addr, status);
    if (strcmp((char *)first_mac, (char *)mac_addr) == 0 &&
        status == ESP_NOW_SEND_SUCCESS) {
        // 
      ESPNowStreamSelf->available_to_write = ESP_NOW_MAX_DATA_LEN;
    }
  }
};


/**
 * A Simple exension of the WiFiUDP class which makes sure that the basic Stream
 * functioinaltiy which is used as AudioSource and AudioSink
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class UDPStream : public WiFiUDP {
public:
  /**
   * Provides the available size of the current package and if this is used up
   * of the next package
   */
  int available() override {
    int size = WiFiUDP::available();
    // if the curren package is used up we prvide the info for the next
    if (size == 0) {
      size = parsePacket();
    }
    return size;
  }

  /// Starts to send data to the indicated address / port
  uint8_t begin(IPAddress a, uint16_t port) {
    remote_address_ext = a;
    remote_port_ext = port;
    return WiFiUDP::begin(port);
  }

  /// Starts to receive data from/with the indicated port
  uint8_t begin(uint16_t port, uint16_t port_ext=0) {
    remote_address_ext = 0u;
    remote_port_ext = port_ext!=0 ? port_ext : port;
    return WiFiUDP::begin(port);
  }

  /// We use the same remote port as defined in begin for write
  uint16_t remotePort() {
    uint16_t result = WiFiUDP::remotePort();
    return result != 0 ? result : remote_port_ext;
  }

  /// We use the same remote ip as defined in begin for write
  IPAddress remoteIP() {
    // Determine address if it has not been specified 
    if ((uint32_t)remote_address_ext==0){
      remote_address_ext = WiFiUDP::remoteIP(); 
    }
    // IPAddress result = WiFiUDP::remoteIP();
    // LOGI("ip: %u", result);
    return remote_address_ext;
  }

  /**
   *  Replys will be sent to the initial remote caller
   */
  size_t write(const uint8_t *buffer, size_t size) override {
    LOGD(LOG_METHOD);
    beginPacket(remoteIP(), remotePort());
    size_t result = WiFiUDP::write(buffer, size);
    endPacket();
    return result;
  }

protected:
  uint16_t remote_port_ext;
  IPAddress remote_address_ext;
};


enum RecordType:uint8_t {Undefined, Begin,Send,Receive,End};
enum AudioType:uint8_t {PCM,MP3,AAC, WAV};
enum TransmitRole:uint8_t {Sender, Receiver};

/// Common Header for all records
struct AudioHeader  {
  AudioHeader() = default;
  uint8_t app = 123;
  RecordType rec = Undefined;
  uint16_t seq = 0;
  // record counter
  void increment() {
    static uint16_t static_count = 0;
    seq = static_count++;
  }
};

/// Protocal Record To Start
struct AudioDataBegin : public AudioHeader {
  AudioDataBegin(){
    rec = Begin;
  }
  AudioBaseInfo info;
  AudioType type = PCM;
};

/// Protocol Record for Data
struct AudioSendData : public AudioHeader{
  AudioSendData(){
    rec = Send;;
  }
  uint16_t size = 0;
};

/// Protocol Record for Request
struct AudioConfirmDataToReceive : public AudioHeader {
  AudioConfirmDataToReceive(){
    rec = Receive;
  }
  uint16_t size = 0;
};

/// Protocol Record for End
struct AudioDataEnd : public AudioHeader {
  AudioDataEnd(){
    rec = End;
  }
};

/**
 * @brief Audio Writer which is synchronizing the amount of data
 * that can be processed with the AudioReceiver
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AudioSyncWriter : public AudioPrint {

  public:
    AudioSyncWriter(Stream &dest){
      p_dest = &dest;
    }

    bool begin(AudioBaseInfo &info, AudioType type){
      is_sync = sync;
      AudioDataBegin begin;
      begin.info = info;
      begin.type = type;
      begin.increment();
      int write_len = sizeof(begin);
      int len = p_dest->write((const uint8_t*)&begin, write_len);
      return len==write_len;
    }

    size_t write(const uint8_t* data, size_t len) override {
      int written_len = 0;
      int open_len = len;
      AudioSendData send;
      while(written_len<len){
        int available_to_write = waitForRequest();
        size_t to_write_len = DEFAULT_BUFFER_SIZE;
        to_write_len = min(open_len, available_to_write);
        send.increment();
        send.size = to_write_len;
        p_dest->write((const uint8_t*) &send, sizeof(send));
        int w = p_dest->write(data+written_len, to_write_len);
        written_len += w;
        open_len -= w;
      }
      return written_len;
    }

    int availableForWrite() override {
      return available_to_write;
    }

    void end() {
      AudioDataEnd end;
      end.increment();
      p_dest->write((const uint8_t*)&end, sizeof(end));
    }

  protected:
    Stream *p_dest;
    int available_to_write = 1024;
    bool is_sync;

    /// Waits for the data to be available
    void waitFor(int size){
        while(p_dest->available()<size){
          delay(10);
        }
    }

    int waitForRequest(){
      AudioConfirmDataToReceive rcv;
        size_t rcv_len = sizeof(rcv);
        waitFor(rcv_len);
        p_dest->readBytes((uint8_t*)&rcv, rcv_len);
        return rcv.size;
    }

};



/**
 * @brief Receving Audio Data over the wire and requesting for more data when done to
 * synchronize the processing with the sender. The audio data is processed by the 
 * EncodedAudioStream; If you have multiple readers, only one receiver should be used as confirmer!
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AudioSyncReader: public AudioStreamX {
  public:
    AudioSyncReader(Stream &in, EncodedAudioStream &out, bool isConfirmer=true){
      p_in = &in;
      p_out = &out;
      is_confirmer = isConfirmer;
    }

    size_t copy() {
      int processed = 0;
      int header_size = sizeof(header);
      waitFor(header_size);
      readBytes((uint8_t*)&header,header_size);

      switch(header.rec){
        case Begin: 
            audioDataBegin();
            break;
        case End: 
            audioDataEnd();
            break;
        case Send: 
            processed = receiveData();
            break;
      }
      return processed;
    }

  protected: 
    Stream *p_in;
    EncodedAudioStream *p_out;
    AudioConfirmDataToReceive req;
    AudioHeader header;
    AudioDataBegin begin;
    size_t available = 0; // initial value
    bool is_started = false;
    bool is_confirmer;
    int last_seq=0;

    /// Starts the processing
    void audioDataBegin() {
        readProtocol(&begin, sizeof(begin));
        p_out->begin();
        p_out->setAudioInfo(begin.info);
        requestData();
    }

    /// Ends the processing
    void audioDataEnd() {
        AudioDataEnd end;
        readProtocol(&end, sizeof(end));
        p_out->end();
    }

    // Receives audio data
    int receiveData() {
        AudioSendData data;
        readProtocol(&data, sizeof(data));
        available = data.size;
        // receive and process audio data
        waitFor(available);
        int max_gap = 10;
        if (data.seq>last_seq || (data.seq<max_gap && last_seq>=(32767-max_gap))){
          uint8_t buffer[available];
          p_in->readBytes((uint8_t*)buffer, available);
          p_out->write((const uint8_t*)buffer,available);
          // only one reader should be used as confirmer
          if (is_confirmer){
            requestData();
          }
          last_seq = data.seq;
        }
        return available;
    }

    /// Waits for the data to be available
    void waitFor(int size){
        while(p_in->available()<size){
          delay(10);
        }
    }

    /// Request new data from writer
    void requestData() {
        req.size = p_out->availableForWrite();
        req.increment();
        p_in->write((const uint8_t*)&req, sizeof(req));
        p_in->flush();
    }

    /// Reads the protocol record
    void readProtocol(AudioHeader*data, int len){
        const static int header_size = sizeof(header);
        memcpy(data, &header, header_size);
        int read_size = len-header_size;
        waitFor(read_size);
        readBytes((uint8_t*)data+header_size, read_size);
    }
};


/**
 * @brief Configure Throttle setting
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
struct ThrottleConfig : public AudioBaseInfo {
  ThrottleConfig(){
    sample_rate = 44100;
    bits_per_sample = 16;
    channels = 2;
  }
  int correction_ms = 0;
};

/**
 * @brief Throttle the sending of the audio data to limit it to the indicated sample rate.
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class Throttle {
public:
  Throttle() = default;

  void begin(ThrottleConfig info) {
    this->info = info;
    bytesPerSample = info.bits_per_sample / 8 * info.channels;
  }

  // starts the timing
  void startDelay() { start_time = millis(); }

  // delay
  void delayBytes(size_t bytes) { delaySamples(bytes / bytesPerSample); }

  // delay
  void delaySamples(size_t samples) {
    int durationMsEff = millis() - start_time;
    int durationToBe = (samples * 1000) / info.sample_rate;
    int waitMs = durationToBe - durationMsEff + info.correction_ms;
    if (waitMs > 0) {
      delay(waitMs);
    }
  }

protected:
  unsigned long start_time;
  ThrottleConfig info;
  int bytesPerSample;
};

} // namespace audio_tools
