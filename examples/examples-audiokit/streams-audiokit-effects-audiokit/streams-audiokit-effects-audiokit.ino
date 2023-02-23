/**
 * @file audiokit-effects-audiokit.ino
 * @author Phil Schatzmann
 * @brief Some Guitar Effects that can be controlled via a Web Gui. 
 * @version 0.1
 * @date 2022-10-14
 * 
 * @copyright Copyright (c) 2022
 * 
 */ 
#include <ArduinoJson.h>  // https://arduinojson.org/
#include "HttpServer.h" // https://github.com/pschatzmann/TinyHttp
#include "AudioTools.h" // https://github.com/pschatzmann/arduino-audio-tools.git
#include "AudioLibs/AudioKit.h" // https://github.com/pschatzmann/arduino-audiokit.git

// Server
WiFiServer wifi;
HttpServer server(wifi);
const char *ssid = "SSID";
const char *password = "PASSWORD";

// Audio Format
const int sample_rate = 44100;
const int channels = 1;
const int bits_per_sample = 16;

// Effects control input
float volumeControl = 0.1;
int16_t clipThreashold = 0;
float fuzzEffectValue = 0;
int16_t fuzzMaxValue = 0;
int16_t tremoloDuration = 0;
float tremoloDepth = 0;

// Effects
Boost boost(volumeControl);
Distortion distortion(clipThreashold);
Fuzz fuzz(fuzzEffectValue);
Tremolo tremolo(tremoloDuration, tremoloDepth, sample_rate);

// Audio
AudioKitStream kit; // Access I2S as stream
AudioEffectStream effects(kit); // input from kit
StreamCopy copier(kit, effects); // copy effects to kit

// Update values in effects
void updateValues(){
  // update values in controls
  boost.setVolume(volumeControl);
  boost.setActive(volumeControl>0);
  distortion.setClipThreashold(clipThreashold);
  distortion.setActive(clipThreashold>0);
  fuzz.setFuzzEffectValue(fuzzEffectValue);
  fuzz.setMaxOut(fuzzMaxValue);
  fuzz.setActive(fuzzEffectValue>0);
  tremolo.setDepth(tremoloDepth);
  tremolo.setDuration(tremoloDuration);
  tremolo.setActive(tremoloDuration>0);
}

// provide JSON as webservice
void getJson(HttpServer * server, const char*requestPath, HttpRequestHandlerLine * hl) {
  auto parameters2Json = [](Stream & out) {
    DynamicJsonDocument doc(1024);
    doc["volumeControl"]["value"] = volumeControl;
    doc["volumeControl"]["min"] = 0;
    doc["volumeControl"]["max"] = 1;
    doc["volumeControl"]["step"] = 0.1;
    doc["clipThreashold"]["value"]   = clipThreashold;
    doc["clipThreashold"]["min"] = 0;
    doc["clipThreashold"]["max"] = 6000;
    doc["clipThreashold"]["step"] = 100;
    doc["fuzzEffectValue"]["value"] = fuzzEffectValue;
    doc["fuzzEffectValue"]["min"] = 0;
    doc["fuzzEffectValue"]["max"] = 10;
    doc["fuzzEffectValue"]["step"] = 0.1;
    doc["fuzzMaxValue"]["value"] = fuzzMaxValue;
    doc["fuzzMaxValue"]["min"] = 0;
    doc["fuzzMaxValue"]["max"] = 32200;
    doc["fuzzMaxValue"]["step"] = 1;
    doc["tremoloDuration"]["value"] = tremoloDuration;
    doc["tremoloDuration"]["min"] = 0;
    doc["tremoloDuration"]["max"] = 2000;
    doc["tremoloDuration"]["step"] = 1;
    doc["tremoloDepth"]["value"] = tremoloDepth;
    doc["tremoloDepth"]["min"] = 0;
    doc["tremoloDepth"]["max"] = 100;
    doc["tremoloDepth"]["step"] = 1;
    serializeJson(doc, out);
  };

  // provide data as json using callback
  server->reply("text/json", parameters2Json, 200);
};


// Poroces Posted Json
void postJson(HttpServer *server, const char*requestPath, HttpRequestHandlerLine *hl) {
  // post json to server
  DynamicJsonDocument doc(1024);
  deserializeJson(doc, server->client());
  volumeControl = doc["volumeControl"];
  clipThreashold = doc["clipThreashold"];
  fuzzEffectValue = doc["fuzzEffectValue"];
  fuzzMaxValue = doc["fuzzMaxValue"];
  tremoloDuration = doc["tremoloDuration"];
  tremoloDepth = doc["tremoloDepth"];

  // update values in controls
  updateValues();

  server->reply("text/json", "{}", 200);
  char msg[120];
  snprintf(msg, 120, "====> updated values %f %d %f %d %d %f", volumeControl, clipThreashold, fuzzEffectValue, tremoloDuration, tremoloDepth);
  Serial.println(msg);
}

// Arduino Setup
void setup(void) {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Warning);

  // Setup Server
  static HttpTunnel tunnel_url("https://pschatzmann.github.io/TinyHttp/app/guitar-effects.html");
  server.on("/", T_GET, tunnel_url);
  server.on("/service", T_GET, getJson);
  server.on("/service", T_POST, postJson);
  server.begin(80, ssid, password);
  server.setNoConnectDelay(0);

  // Setup Kit
  auto cfg = kit.defaultConfig(RXTX_MODE);
  cfg.sd_active = false;
  cfg.input_device = AUDIO_HAL_ADC_INPUT_LINE2;
  cfg.sample_rate = sample_rate;
  cfg.channels = channels;
  // minimize lag
  cfg.buffer_count = 2;
  cfg.buffer_size = 256;
  kit.begin(cfg);
  kit.setVolume(1.0);

  // setup effects
  effects.addEffect(boost);
  effects.addEffect(distortion);
  effects.addEffect(fuzz);
  effects.addEffect(tremolo);
  effects.begin(cfg);

  updateValues();
}

// Arduino loop - copy data
void loop() {
  copier.copy();
  server.copy();
}
