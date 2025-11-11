// Example for creating a Media Renderer backed by a AudioPlayer

#include "WiFi.h"
#include "DLNA.h"
#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"
#include "AudioTools/Communication/AudioHttp.h"
#include "AudioTools/Disk/AudioSourceURL.h"
#include "AudioTools/AudioCodecs/CodecHelix.h"
#include "AudioTools/Concurrency/RTOS.h"

const char* ssid = "ssid";
const char* password = "password";

// DLNA
const int port = 9000;
WiFiServer wifi(port);
HttpServer<WiFiClient, WiFiServer> server(wifi);
UDPService<WiFiUDP> udp;
DLNAMediaRenderer<WiFiClient> media_renderer(server, udp);

// AudioPlayer
URLStream url;
AudioSourceDynamicURL source(url);
AudioBoardStream i2s(AudioKitEs8388V1);  // or e.g. I2SStream i2s;
MultiDecoder multi_decoder;
AACDecoderHelix dec_aac;
MP3DecoderHelix dec_mp3;
WAVDecoder dec_wav;
AudioPlayer player(source, i2s, multi_decoder);
bool is_paused = false;
Task dlna_task("dlna", 8000, 10, 0);

// Callback when playback reaches EOF
void onEOF(AudioPlayer& player) {
  media_renderer.setPlaybackCompleted();
}

// DLNA event handler
void handleMediaEvent(MediaEvent ev, DLNAMediaRenderer<WiFiClient>& mr) {
  switch (ev) {
    case MediaEvent::SET_URI:
      Serial.print("Event: SET_URI ");
      if (mr.getCurrentUri()) {
        Serial.println(mr.getCurrentUri());
        source.clear();
        source.addURL(mr.getCurrentUri());
      }
      break;
    case MediaEvent::PLAY:
      Serial.println("Event: PLAY");
      if (is_paused) player.setActive(true);
      else player.begin();
      break;
    case MediaEvent::STOP:
      Serial.println("Event: STOP");
      is_paused = false;
      player.end();
      break;
    case MediaEvent::PAUSE:
      Serial.println("Event: PAUSE");
      player.setActive(false);
      is_paused = true;
      break;
    case MediaEvent::SET_VOLUME:
      Serial.print("Event: SET_VOLUME ");
      Serial.println(mr.getVolume());
      player.setVolume(static_cast<float>(mr.getVolume()) / 100.0);
      break;
    case MediaEvent::SET_MUTE:
      Serial.print("Event: SET_MUTE ");
      Serial.println(mr.isMuted() ? 1 : 0);
      player.setMuted(mr.isMuted());
      break;
    default:
      Serial.println("Event: OTHER");
  }
}

// start WiFi
void setupWifi() {
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  WiFi.setSleep(false);
  Serial.println("connected!");
}

void setup() {
  // setup logger
  Serial.begin(115200);
  DlnaLogger.begin(Serial, DlnaLogLevel::Warning);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  // start Wifi
  setupWifi();

  // setup MultiDecoder
  multi_decoder.addDecoder(dec_aac, "audio/aac");
  multi_decoder.addDecoder(dec_mp3, "audio/mpeg");
  multi_decoder.addDecoder(dec_wav, "audio/wav");

  // configure player: EOF handling
  player.setOnEOFCallback(onEOF);

  // start I2S
  i2s.begin();

  // setup media renderer (use event callbacks to handle audio at app level)
  media_renderer.setBaseURL(WiFi.localIP(), port);
  media_renderer.setMediaEventHandler(handleMediaEvent);

  // start device
  if (!media_renderer.begin()) {
    Serial.println("MediaRenderer failed to start");
  }

  // DLNA processing in separate task
  dlna_task.begin([]() {
    media_renderer.loop();
  });
}

void loop() {
  player.copy();
}
