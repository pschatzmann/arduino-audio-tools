
#include <AsyncUDP.h>
#include <WiFi.h>

#include "AudioTools/AudioLibs/vban/vban.h"
#include "AudioTools/CoreAudio/AudioStreams.h"
#include "AudioTools/Concurrency/RTOS/BufferRTOS.h"

namespace audio_tools {

class VBANConfig : public AudioInfo {
 public:
  VBANConfig() {
    sample_rate = 11025;
    channels = 1;
    bits_per_sample = 16;
  }
  RxTxMode mode;
  /// name of the stream
  const char* stream_name = "Stream1";
  /// default port is 6980
  uint16_t udp_port = 6980;
  /// Use {0,0,0,0}; as broadcast address
  IPAddress target_ip{0, 0, 0, 0};
  /// ssid for wifi connection
  const char* ssid = nullptr;
  /// password for wifi connection
  const char* password = nullptr;
  int rx_buffer_count = 30;
  // set to true if samples are generated faster then sample rate
  bool throttle_active = false;
  // when negative the number of ms that are subtracted from the calculated wait
  // time to fine tune Overload and Underruns
  int throttle_correction_us = 0;
  // defines the max write size
  int max_write_size =
      DEFAULT_BUFFER_SIZE * 2;  // just good enough for 44100 stereo
  uint8_t format = 0;

  //reply for discovery packet
  uint32_t device_flags = 0x00000001;  // default: receiver only
  uint32_t bitfeature = 0x00000001;  // default: audio only
  uint32_t device_color = 0x00FF00;    // green default
  //const char* stream_name_reply = "VBAN SPOT PING";
  const char* device_name = nullptr;  // nullptr means use MAC by default
  const char* manufacturer_name = "ESP32 AudioTools";
  const char* application_name = "VBAN Streamer";
  const char* host_name = nullptr;    // will fallback to WiFi.getHostname()
  const char* user_name = "User";
  const char* user_comment = "ESP32 VBAN Audio Device";
};

/**
 * @brief VBAN Audio Source and Sink for the ESP32. For further details please
 * see https://vb-audio.com/Voicemeeter/vban.htm .
 * Inspired by https://github.com/rkinnett/ESP32-VBAN-Audio-Source/tree/master
 * and https://github.com/rkinnett/ESP32-VBAN-Network-Audio-Player
 * @ingroup communications
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class VBANStream : public AudioStream {
 public:
  VBANConfig defaultConfig(RxTxMode mode = TX_MODE) {
    VBANConfig def;
    def.mode = mode;
    return def;
  }

  void setOutput(Print &out){
    p_out = &out;
  }

  void setAudioInfo(AudioInfo info) override {
    cfg.copyFrom(info);
    AudioStream::setAudioInfo(info);
    auto thc = throttle.defaultConfig();
    thc.copyFrom(info);
    thc.correction_us = cfg.throttle_correction_us;  
    throttle.begin(thc);
    if (cfg.mode == TX_MODE) {
      configure_tx();
    }
  }

  bool begin(VBANConfig cfg) {
    this->cfg = cfg;
    setAudioInfo(cfg);
    return begin();
  }

  bool begin() {
    if (cfg.mode == TX_MODE) {
      if (cfg.bits_per_sample != 16) {
        LOGE("Only 16 bits supported")
        return false;
      }
      tx_buffer.resize(VBAN_PACKET_NUM_SAMPLES);
      return begin_tx();
    } else {
#ifdef ESP32
      rx_buffer.resize(DEFAULT_BUFFER_SIZE * cfg.rx_buffer_count);
      rx_buffer.setReadMaxWait(10);
#else
      rx_buffer.resize(DEFAULT_BUFFER_SIZE, cfg.rx_buffer_count);
#endif  
      return begin_rx();
    }
  }

  size_t write(const uint8_t* data, size_t len) override {
    if (!udp_connected) return 0;

    int16_t* adc_data = (int16_t*)data;
    size_t samples = len / (cfg.bits_per_sample/8);

    // limit output speed
    if (cfg.throttle_active) {
      throttle.delayFrames(samples / cfg.channels);
    }

    for (int j = 0; j < samples; j++) {
      tx_buffer.write(adc_data[j]);
      if (tx_buffer.availableForWrite() == 0) {
        memcpy(vban.data_frame, tx_buffer.data(), vban.packet_data_bytes);
        *vban.packet_counter = packet_counter;  // increment packet counter
        // Send packet
        if (cfg.target_ip == broadcast_address) {
          udp.broadcastTo((uint8_t*)&vban.packet, vban.packet_total_bytes,
                          cfg.udp_port);
        } else {
          udp.writeTo((uint8_t*)&vban.packet, vban.packet_total_bytes,
                      cfg.target_ip, cfg.udp_port);
        }
        // defile delay start time
        packet_counter++;
        tx_buffer.reset();
      }
    }
    return len;
  }

  int availableForWrite() { return cfg.max_write_size; }

  size_t readBytes(uint8_t* data, size_t len) override {
    TRACED();
    size_t samples = len / (cfg.bits_per_sample/8);
    if (cfg.throttle_active) {
      throttle.delayFrames(samples / cfg.channels);
    }
    return rx_buffer.readArray(data, len);
  }

  int available() { return available_active ? rx_buffer.available() : 0; }

 protected:
  const IPAddress broadcast_address{0, 0, 0, 0};
  AsyncUDP udp;
  VBan vban;
  VBANConfig cfg;
  SingleBuffer<int16_t> tx_buffer{0};
 #ifdef ESP32
   BufferRTOS<uint8_t> rx_buffer{ 0};
 #else
  NBuffer<uint8_t> rx_buffer{DEFAULT_BUFFER_SIZE, 0};
 #endif
  bool udp_connected = false;
  uint32_t packet_counter = 0;
  Throttle throttle;
  size_t bytes_received = 0;
  bool available_active = false;
  Print *p_out = nullptr;

  bool begin_tx() {
    if (!configure_tx()) {
      return false;
    }
    start_wifi();
    if (WiFi.status() != WL_CONNECTED) {
      LOGE("Wifi not connected");
      return false;
    }
    WiFi.setSleep(false);
    IPAddress myIP = WiFi.localIP();
    udp_connected = udp.connect(myIP, cfg.udp_port);
    return udp_connected;
  }

  bool begin_rx() {
    start_wifi();
    if (WiFi.status() != WL_CONNECTED) {
      LOGE("Wifi not connected");
      return false;
    }
    WiFi.setSleep(false);
    bytes_received = 0;
    this->available_active = false;
    // connect to target
    if (!udp.listen(cfg.udp_port)) {
      LOGE("Could not connect to '%s:%d' target", toString(cfg.target_ip),
           cfg.udp_port);
    }
    // handle data
    udp.onPacket([this](AsyncUDPPacket packet) { receive_udp(packet); });

    return true;
  }

  bool configure_tx() {
    int rate = vban_sample_rate();
    if (rate < 0) {
      LOGE("Invalid sample rate: %d", cfg.sample_rate);
      return false;
    }
    configure_vban((VBanSampleRates)rate);
    return true;
  }

  void start_wifi() {
    if (cfg.ssid == nullptr) return;
    if (cfg.password == nullptr) return;
    LOGI("ssid %s", cfg.ssid);
    // Setup Wifi:
    WiFi.begin(cfg.ssid, cfg.password);      // Connect to your WiFi router
    while (WiFi.status() != WL_CONNECTED) {  // Wait for connection
      delay(500);
      Serial.print(".");
    }
    Serial.println();

    LOGI("Wifi connected to IP (%d.%d.%d.%d)", WiFi.localIP()[0],
         WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
  }

  void configure_vban(VBanSampleRates rate) {
    // Set vban packet header, counter, and data frame pointers to respective
    // parts of packet:
    vban.hdr = (VBanHeader*)&vban.packet[0];
    vban.packet_counter = (uint32_t*)&vban.packet[VBAN_PACKET_HEADER_BYTES];
    vban.data_frame =
        (uint8_t*)&vban
            .packet[VBAN_PACKET_HEADER_BYTES + VBAN_PACKET_COUNTER_BYTES];

    // Setup the packet header:
    strncpy(vban.hdr->preamble, "VBAN", 4);
    vban.hdr->sample_rate =
        static_cast<int>(VBAN_PROTOCOL_AUDIO) |
        rate;  // 11025 Hz, which matches default sample rate for soundmodem
    vban.hdr->num_samples =
        (VBAN_PACKET_NUM_SAMPLES / cfg.channels) - 1;  // 255 = 256 samples
    vban.hdr->num_channels = cfg.channels - 1;         // 0 = 1 channel
    vban.hdr->sample_format =
        static_cast<int>(VBAN_BITFMT_16_INT) | VBAN_CODEC_PCM;  // int16 PCM
    strncpy(vban.hdr->stream_name, cfg.stream_name,
            min((int)strlen(cfg.stream_name), VBAN_STREAM_NAME_SIZE));

    vban.packet_data_bytes =
        (vban.hdr->num_samples + 1) * (vban.hdr->num_channels + 1) *
        ((vban.hdr->sample_format & VBAN_BIT_RESOLUTION_MASK) + 1);
    vban.packet_total_bytes = vban.packet_data_bytes +
                              VBAN_PACKET_HEADER_BYTES +
                              VBAN_PACKET_COUNTER_BYTES;
  }

  int vban_sample_rate() {
    int result = -1;
    switch (cfg.sample_rate) {
      case 6000:
        result = SAMPLE_RATE_6000_HZ;
        break;
      case 12000:
        result = SAMPLE_RATE_12000_HZ;
        break;
      case 24000:
        result = SAMPLE_RATE_24000_HZ;
        break;
      case 48000:
        result = SAMPLE_RATE_48000_HZ;
        break;
      case 96000:
        result = SAMPLE_RATE_96000_HZ;
        break;
      case 192000:
        result = SAMPLE_RATE_192000_HZ;
        break;
      case 384000:
        result = SAMPLE_RATE_384000_HZ;
        break;
      case 8000:
        result = SAMPLE_RATE_8000_HZ;
        break;
      case 16000:
        result = SAMPLE_RATE_16000_HZ;
        break;
      case 32000:
        result = SAMPLE_RATE_32000_HZ;
        break;
      case 64000:
        result = SAMPLE_RATE_64000_HZ;
        break;
      case 128000:
        result = SAMPLE_RATE_128000_HZ;
        break;
      case 256000:
        result = SAMPLE_RATE_256000_HZ;
        break;
      case 512000:
        result = SAMPLE_RATE_512000_HZ;
        break;
      case 11025:
        result = SAMPLE_RATE_11025_HZ;
        break;
      case 22050:
        result = SAMPLE_RATE_22050_HZ;
        break;
      case 44100:
        result = SAMPLE_RATE_44100_HZ;
        break;
      case 88200:
        result = SAMPLE_RATE_88200_HZ;
        break;
      case 176400:
        result = SAMPLE_RATE_176400_HZ;
        break;
      case 352800:
        result = SAMPLE_RATE_352800_HZ;
        break;
      case 705600:
        result = SAMPLE_RATE_705600_HZ;
        break;
    }
    return result;
  }

  const char* toString(IPAddress adr) {
    static char str[11] = {0};
    snprintf(str, 11, "%d.%d.%d.%d", adr[0], adr[1], adr[2], adr[3]);
    return str;
  }

  /**
   * @brief VBAN adjusts the number of samples per packet according to sample
   *rate. Assuming 16-bit PCM mono, sample rates 11025, 22050, 44100, and 88200
   *yield packets containing 64, 128, 256, and 256 samples per packet,
   *respectively. The even-thousands sample rates below 48000 yield
   *non-power-of-2 lengths. For example, sample rate 24000 yields 139 samples
   *per packet. This VBAN->DMA->DAC method seems to require the dma buffer
   *length be set equal to the number of samples in each VBAN packet. ESP32
   *I2S/DMA does not seem to handle non-power-of-2 buffer lengths well. Sample
   *rate 24000 doesn't work reliably at all. Sample rate 32000 is stable but
   *stutters. Recommend selecting from sample rates 11025, 22050, 44100, and
   *above And set samplesPerPacket to 64 for 11025, 128 for 22050, or 256 for
   *all else.
   **/

  void receive_udp(AsyncUDPPacket& packet) {
    uint16_t vban_rx_data_bytes, vban_rx_sample_count;
    int16_t* vban_rx_data;
    uint32_t* vban_rx_pkt_nbr;
    uint16_t outBuf[VBAN_PACKET_MAX_SAMPLES + 1];
    size_t bytesOut;

    int len = packet.length();
    if (len > 0) {
      LOGD("receive_udp %d", len);
      uint8_t* udpIncomingPacket = packet.data();

      // receive incoming UDP packet
      // Check if packet length meets VBAN specification:
      if (len < VBAN_PACKET_HEADER_BYTES) {
        LOGE("Too short to be VBAN (%u bytes)", len);
        return;
      }

      // Check if preamble matches VBAN format:
      if (strncmp("VBAN", (const char*)udpIncomingPacket, 4) != 0) {
        LOGE("Unrecognized preamble %.4s", udpIncomingPacket);
        return;
      }

      uint8_t protocol = udpIncomingPacket[4] & VBAN_PROTOCOL_MASK;

      if (protocol == VBAN_PROTOCOL_SERVICE) {
        // Allow up to ~1024 bytes for service packets like Ping0
        if (len > 1024) {
          LOGE("Service packet length invalid: %u bytes", len);
          return;
        }
      } else {
        // Audio, serial, etc
        if (len <= (VBAN_PACKET_HEADER_BYTES + VBAN_PACKET_COUNTER_BYTES) || len > VBAN_PACKET_MAX_LEN_BYTES) {
          LOGE("Audio/other packet length invalid: %u bytes", len);
          rx_buffer.reset();
          return;
        }
      }
      
      //LOGI("VBAN format byte: 0x%02X", udpIncomingPacket[7]);
      //LOGD("VBAN protocol mask applied: 0x%02X", udpIncomingPacket[7] & VBAN_PROTOCOL_MASK);
      //Serial.printf("Header[7] = 0x%02X\n", udpIncomingPacket[7]);

      
      //-------------------------------------------------------------------------
      //SUPPORT PING REQUEST
      if ( protocol == VBAN_PROTOCOL_SERVICE ) {

          uint8_t service_type = udpIncomingPacket[5];
          uint8_t service_fnct = udpIncomingPacket[6];

          if (service_type == VBAN_SERVICE_IDENTIFICATION) {
              bool isReply = (service_fnct & VBAN_SERVICE_FNCT_REPLY) != 0;
              uint8_t function = service_fnct & 0x7F;

              if (!isReply && function == 0) {
                  LOGI("Received VBAN PING0 request");
                  sendVbanPing0Reply(packet);
              }
          }
          return;
      }
      //--------------------------------------------------------------------------

      vban_rx_data_bytes =
          len - (VBAN_PACKET_HEADER_BYTES + VBAN_PACKET_COUNTER_BYTES);
      vban_rx_pkt_nbr = (uint32_t*)&udpIncomingPacket[VBAN_PACKET_HEADER_BYTES];
      vban_rx_data = (int16_t*)&udpIncomingPacket[VBAN_PACKET_HEADER_BYTES +
                                                  VBAN_PACKET_COUNTER_BYTES];
      vban_rx_sample_count = vban_rx_data_bytes / (cfg.bits_per_sample / 8);
      uint8_t vbanSampleRateIdx = udpIncomingPacket[4] & VBAN_SR_MASK;
      uint8_t vbchannels = udpIncomingPacket[6] + 1;
      uint8_t vbframes = udpIncomingPacket[5] + 1;
      uint8_t vbformat = udpIncomingPacket[7] & VBAN_PROTOCOL_MASK;
      uint8_t vbformat_bits = udpIncomingPacket[7] & VBAN_BIT_RESOLUTION_MASK;
      uint32_t vbanSampleRate = VBanSRList[vbanSampleRateIdx];

      //LOGD("sample_count: %d -  frames: %d", vban_rx_sample_count, vbframes);
      //assert (vban_rx_sample_count == vbframes*vbchannels);

      // E.g. do not process any text
      if (vbformat != cfg.format){
        LOGE("Format ignored: 0x%x", vbformat);
        return;
      }

      // Currently we support only 16 bits.
      if (vbformat_bits != VBAN_BITFMT_16_INT){
        LOGE("Format only 16 bits supported");
        return;
      }

      // Just to be safe, re-check sample count against max sample count to
      // avoid overrunning outBuf later
      if (vban_rx_sample_count > VBAN_PACKET_MAX_SAMPLES) {
        LOGE("unexpected packet size: %u", vban_rx_sample_count);
        return;
      }

      // update sample rate
      if (cfg.sample_rate != vbanSampleRate || cfg.channels != vbchannels) {
        // update audio info
        cfg.sample_rate = vbanSampleRate;
        cfg.channels = vbchannels;
        setAudioInfo(cfg);
        // remove any buffered data
        rx_buffer.reset();
        available_active = false;
      }

      if (p_out!=nullptr){
        int size_written = p_out->write((uint8_t*)vban_rx_data, vban_rx_data_bytes);
        if (size_written != vban_rx_data_bytes) {
          LOGE("buffer overflow %d -> %d", vban_rx_data_bytes, size_written);
        }
        return;
      }

      // write data to buffer
      int size_written = rx_buffer.writeArray((uint8_t*)vban_rx_data, vban_rx_data_bytes);
      if (size_written != vban_rx_data_bytes) {
        LOGE("buffer overflow %d -> %d", vban_rx_data_bytes, size_written);
      }

      // report available bytes only when buffer is 50% full
      if (!available_active) {
        bytes_received += vban_rx_data_bytes;
        if (bytes_received >= cfg.rx_buffer_count * DEFAULT_BUFFER_SIZE * 0.75){
          available_active = true;
          LOGI("Activating vban");
        }
      }
    }
  }
//-------------------------------------------------------------------------------------
  //implement ping reply based on VBAN standard
 void sendVbanPing0Reply(AsyncUDPPacket& sourcePacket) {

    // Prepare VBAN 28-byte service header
    uint8_t header[28];
    memset(header, 0, sizeof(header));
    memcpy(header, "VBAN", 4);
    header[4] = VBAN_PROTOCOL_SERVICE;
    header[5] = VBAN_SERVICE_FNCT_PING0 | VBAN_SERVICE_FNCT_REPLY; // Service function + reply bit
    header[6] = 0x00; // must be zero
    // Copy incoming stream name from discovery packet
    const uint8_t* data = sourcePacket.data();
    memcpy(&header[8], &data[8], 16);
    // Copy frame number (little endian)
    
    uint32_t frameNumber = (uint32_t)((data[24] & 0xFF) | ((data[25] & 0xFF) << 8) | ((data[26] & 0xFF) << 16) | ((data[27] & 0xFF) << 24));
    memcpy(&header[24], &frameNumber, 4);
    
    // Construct the PING0 payload using the struct
    VBAN_PING0 ping0;
    memset(&ping0, 0, sizeof(ping0));

    // Fill fields with your config data and fixed values
    ping0.bitType       = cfg.device_flags;
    ping0.bitfeature    = cfg.bitfeature; 
    ping0.bitfeatureEx  = 0x00000000;
    ping0.PreferedRate  = 44100;
    ping0.MinRate       = 8000;
    ping0.MaxRate       = 96000;
    ping0.color_rgb     = cfg.device_color;

    // Version string, 8 bytes total (zero padded)
    memcpy(ping0.nVersion, "v1.0", 4);

    // GPS_Position left empty (all zero), so no need to set
    // USER_Position 8 bytes
    memcpy(ping0.USER_Position, "USRPOS", 6);
    // LangCode_ascii 8 bytes ("EN" + padding)
    memset(ping0.LangCode_ascii, 0, sizeof(ping0.LangCode_ascii));
    memcpy(ping0.LangCode_ascii, "EN", 2);
    // reserved_ascii and reservedEx are zeroed by memset
    // IP as string, max 32 bytes
 
    char ipStr[16]; // Enough for "255.255.255.255\0"
    sprintf(ipStr, "%d.%d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
    safe_strncpy(ping0.DistantIP_ascii, ipStr, sizeof(ping0.DistantIP_ascii));
    // Ports (network byte order)
   
    ping0.DistantPort = cfg.udp_port; //returs port I am listening for VBAN - more useful then UDP ephemeral port
    ping0.DistantReserved = 0;

    // Device name (64 bytes)
    if (cfg.device_name && cfg.device_name[0] != '\0') {
        safe_strncpy(ping0.DeviceName_ascii, cfg.device_name, sizeof(ping0.DeviceName_ascii));
    } else {
        uint8_t mac[6];
        WiFi.macAddress(mac);
        char macStr[64];
        snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        safe_strncpy(ping0.DeviceName_ascii, macStr, sizeof(ping0.DeviceName_ascii));
    }

    // Manufacturer name (64 bytes)
    safe_strncpy(ping0.ManufacturerName_ascii, cfg.manufacturer_name, sizeof(ping0.ManufacturerName_ascii));
    // Application name (64 bytes)
    safe_strncpy(ping0.ApplicationName_ascii, cfg.application_name, sizeof(ping0.ApplicationName_ascii));
    // Host name (64 bytes)
    const char* hostName = cfg.host_name;
    if (!hostName || hostName[0] == '\0') {
        hostName = WiFi.getHostname();
        if (!hostName) hostName = "ESP32";
    }
    safe_strncpy(ping0.HostName_ascii, hostName, sizeof(ping0.HostName_ascii));

    // UserName_utf8
    safe_strncpy(ping0.UserName_utf8, cfg.user_name, sizeof(ping0.UserName_utf8));
    //UserComment_utf8
    safe_strncpy(ping0.UserComment_utf8, cfg.user_comment, sizeof(ping0.UserComment_utf8));

    // Prepare final packet: header + payload
    uint8_t packet[28 + sizeof(VBAN_PING0)];
    memcpy(packet, header, 28);
    memcpy(packet + 28, &ping0, sizeof(VBAN_PING0));

    // Send UDP packet
    udp.writeTo(packet, sizeof(packet), sourcePacket.remoteIP(), sourcePacket.remotePort());
}

  // Safely copy a C-string with guaranteed null termination
  void safe_strncpy(char* dest, const char* src, size_t dest_size) {
      if (dest_size == 0) return;
      strncpy(dest, src, dest_size - 1);
      dest[dest_size - 1] = '\0';
  }
  //-----------------------------------------------------------------------------------
};

}  // namespace audio_tools