// Example for creating a Media Renderer

#include "WiFi.h"
#include "DLNA.h" // https://github.com/pschatzmann/arduino-dlna
#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"
#include "AudioTools/Communication/AudioHttp.h"
#include "AudioTools/Disk/AudioSourceURL.h"
#include "AudioTools/AudioCodecs/CodecHelix.h"
#include "AudioTools/Concurrency/RTOS.h"
#include "AudioTools/Concurrency/AudioPlayerThreadSafe.h"

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

// Multitasking
QueueRTOS<AudioPlayerCommand> queue(20, portMAX_DELAY, 5);
AudioPlayerThreadSafe<QueueRTOS> player_save(player, queue);
Task dlna_task("dlna", 8000, 10, 0);

void onEOF(AudioPlayer& player) {
  if (source.size() > 0) {
    Serial.println("*** onEOF() ***");
    player_save.end();
    source.clear();
    media_renderer.setPlaybackCompleted();
  }
}

void handleMediaEvent(MediaEvent ev, DLNAMediaRenderer<WiFiClient>& mr) {
  switch (ev) {
    case MediaEvent::SET_URI:
      Serial.print("Event: SET_URI ");
      Serial.println(mr.getCurrentUri());
      source.clear();
      source.setTimeoutAutoNext(1000);
      player_save.setPath(mr.getCurrentUri());
      player_save.begin(0, media_renderer.isActive());
      break;
    case MediaEvent::PLAY:
      Serial.println("Event: PLAY");
      player_save.setActive(true);
      break;
    case MediaEvent::STOP:
      Serial.println("Event: STOP");
      player_save.end();
      url.end();
      break;
    case MediaEvent::PAUSE:
      Serial.println("Event: PAUSE");
      player_save.setActive(false);
      break;
    case MediaEvent::SET_VOLUME:
      Serial.print("Event: SET_VOLUME ");
      Serial.println(mr.getVolume());
      player_save.setVolume(static_cast<float>(mr.getVolume()) / 100.0);
      break;
    case MediaEvent::SET_MUTE:
      Serial.print("Event: SET_MUTE ");
      Serial.println(mr.isMuted() ? 1 : 0);
      player_save.setMuted(mr.isMuted());
      break;
    default:
      Serial.println("Event: OTHER");
  }
}

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
  player.setAutoNext(false);

  // start I2S
  i2s.begin();

  // setup media renderer (use event callbacks to handle audio at app level)
  media_renderer.setBaseURL(WiFi.localIP(), port);
  media_renderer.setMediaEventHandler(handleMediaEvent);

  // start device
  if (!media_renderer.begin()) {
    Serial.println("MediaRenderer failed to start");
  }

  dlna_task.begin([]() {
    media_renderer.loop();
  });
}

void loop() {
  // if we have nothing to copy, be nice to other tasks
  if (player_save.copy()==0) delay(200);
}
