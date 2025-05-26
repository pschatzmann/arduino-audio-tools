/**
 * @file player-ftp-audiokit.ino
 * @brief AudioPlayer example that is using FTP as audio source
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include "WiFi.h"
#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"
#include "AudioTools/Disk/AudioSourceFTP.h"

const char* path = "Music/Tracy Chapman/Matters of the Heart";
const char* ext = "mp3";
const char* ftp_user = "user";
const char* ftp_pwd = "password";
const char* ssid = "ssid";
const char* ssid_pwd = "ssid-password";

FTPClient<WiFiClient> ftp;
AudioSourceFTP<WiFiClient> source(ftp, path, ext);
AudioBoardStream i2s(AudioKitEs8388V1);
MP3DecoderHelix decoder; 
AudioPlayer player(source, i2s, decoder);

void next(bool, int, void*) { player.next(); }

void previous(bool, int, void*) { player.previous(); }

void startStop(bool, int, void*) { player.setActive(!player.isActive()); }

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);
  FTPLogger::setOutput(Serial);
  FTPLogger::setLogLevel(LOG_DEBUG);

  // connect to WIFI
  WiFi.begin(ssid, ssid_pwd);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  WiFi.setSleep(false);

  // connect to ftp
  if (!ftp.begin(IPAddress(192,168,1,39), ftp_user, ftp_pwd)){
    Serial.println("ftp failed");
    stop();
  }

  // setup output
  auto cfg = i2s.defaultConfig(TX_MODE);
  if (!i2s.begin(cfg)){
    Serial.println("i2s failed");
    stop();
  }

  // setup additional buttons
  i2s.addDefaultActions();
  i2s.addAction(i2s.getKey(1), startStop);
  i2s.addAction(i2s.getKey(4), next);
  i2s.addAction(i2s.getKey(3), previous);

  // setup player
  player.setVolume(0.7);
  // load the directory 
  if (!player.begin()){
    Serial.println("player failed");
    stop();
  }
}

void loop() {
  player.copy();
  i2s.processActions();
}
