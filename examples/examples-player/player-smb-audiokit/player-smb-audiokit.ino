/**
 * @file player-smb-audiokit.ino
 * @brief AudioPlayer example that is using a SMB (Windows) share as audio
 * source. Dependencies: https://github.com/pschatzmann/TinySMB
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include "WiFi.h"
#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"
#include "AudioTools/Disk/AudioSourceSMB.h"

const char* smb_host = "192.168.1.39";
const char* smb_share = "share";
const char* smb_user = "user";
const char* smb_pwd = "password";
const char* path = "/Music";
const char* ext = "mp3";
const char* ssid = "ssid";
const char* ssid_pwd = "ssid-password";

WiFiClient wifi;
SMBClient smbClient(wifi);
AudioSourceSMB source(smbClient, path, ext);
AudioBoardStream i2s(AudioKitEs8388V1);
MP3DecoderHelix decoder;
AudioPlayer player(source, i2s, decoder);

void next(bool, int, void*) { player.next(); }

void previous(bool, int, void*) { player.previous(); }

void startStop(bool, int, void*) { player.setActive(!player.isActive()); }

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);
  SMBLogger.begin(Serial, SMBLogLevel::Error);

  // connect to WIFI
  WiFi.begin(ssid, ssid_pwd);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  WiFi.setSleep(false);

  // connect to \\host\share
  if (!smbClient.begin(smb_host, smb_share, smb_user, smb_pwd)) {
    Serial.println("smb failed");
    stop();
  }

  // setup output
  auto cfg = i2s.defaultConfig(TX_MODE);
  if (!i2s.begin(cfg)) {
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
  if (!player.begin()) {
    Serial.println("player failed");
    stop();
  }
}

void loop() {
  player.copy();
  i2s.processActions();
}
