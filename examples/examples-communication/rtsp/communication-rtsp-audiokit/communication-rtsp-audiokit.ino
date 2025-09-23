
/**
 * @file communication-rtsp-audiokit.ino
 * @brief RTSP client demo using the new UDP/RTP client and AudioKit output.
 *        Connects to an RTSP server, decodes audio via MultiDecoder, and plays
 *        out via `AudioBoardStream` (AudioKit ES8388). Tested with RTSP
 *        servers. Requires WiFi on ESP32.
 *
 * Steps:
 * - Update WiFi credentials and RTSP server address/path below
 * - Builds a fixed pipeline: MultiDecoder -> ResampleStream -> AudioKit output
 * - Call client.copy() in loop to push received RTP payloads into decoders
 */
#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecADPCM.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"  // https://github.com/pschatzmann/arduino-libhelix
#include "AudioTools/AudioLibs/AudioBoardStream.h"
#include "AudioTools/Communication/RTSP.h"  // brings RTSPClientWiFi alias

const char* SSID = "ssid";
const char* PASS = "password";
IPAddress srv(192, 168, 1, 39);  // change to your RTSP server IP
const uint16_t rtspPort = 8554;  // typical RTSP port
const char* rtspPath =
    "stream";  // change to your RTSP server path (e.g., "audio", "stream1")
AudioBoardStream i2s(AudioKitEs8388V1);
RTSPClientWiFi client(i2s);
MP3DecoderHelix mp3;  // Decoder for "audio/mpeg" (MP3) payloads
ADPCMDecoder adpcm(AV_CODEC_ID_ADPCM_IMA_WAV, 512);  // ima adpcm decoder

void startWiFi() {
  WiFi.begin(SSID, PASS);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("WiFi connected, IP: ");
  Serial.println(WiFi.localIP());
  WiFi.setSleep(false);
}

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  // Connect WiFi
  startWiFi();

  // Configure and start I2S/AudioKit output
  auto cfg = i2s.defaultConfig(TX_MODE);
  cfg.sd_active = false;
  i2s.begin(cfg);

  // Start RTSP session
  client.addDecoder("audio/mpeg", mp3);
  client.addDecoder("audio/adpcm", adpcm);
  client.setResampleFactor(1.0);  // no resampling
  // Servers often require a concrete path; also extend header timeout if needed
  client.setHeaderTimeoutMs(8000);
  if (!client.begin(srv, rtspPort, rtspPath)) {
    Serial.println("Failed to start RTSP client");
    stop();
  }
  Serial.println("RTSP client started");
}

void loop() {
  // Push next available RTP payload to decoder chain
  client.copy();
}
