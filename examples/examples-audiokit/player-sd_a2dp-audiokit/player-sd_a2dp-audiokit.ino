#define USE_SDFAT
#define USE_HELIX
#define USE_A2DP

#include "AudioTools.h"
#include "AudioLibs/AudioKit.h"

const char *startFilePath="/";
const char* ext="mp3";
int speedMz = 10;
AudioKitStream kit;
AudioSourceSdFat source(startFilePath, ext, PIN_AUDIO_KIT_SD_CARD_CS, speedMz);
MP3DecoderHelix decoder;
AudioPlayer player(source, kit, decoder);
BluetoothA2DPSink a2dp_sink;
bool player_active = true;

// Write data to AudioKit in callback
void read_data_stream(const uint8_t *data, uint32_t length) {
    kit.write(data, length);
}

// switch between a2dp and sd player
void mode(bool, int, void*) {
  player_active = !player_active;
  if (player_active){
      Serial.println("Stopping A2DP");          
      a2dp_sink.end();        
      // setup player
      player.setVolume(0.7);
      player.begin();
      Serial.println("Player started");     
  } else {
      Serial.println("Stopping player..");     
      player.end();
      // start sink
      a2dp_sink.start("AudioKit"); 
      // update sample rate
      auto cfg = kit.defaultConfig();
      cfg.sample_rate = a2dp_sink.sample_rate();
      kit.setAudioInfo(cfg);
      Serial.println("A2DP started");     
  }
}

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  // provide a2dp data
  a2dp_sink.set_stream_reader(read_data_stream, false);

  // setup output
  auto cfg = kit.defaultConfig(TX_MODE);
  kit.setVolume(40);
  kit.begin(cfg);

 // setup additional buttons 
  kit.addAction(PIN_KEY4, mode);

  // setup player
  player.setVolume(0.7);
  player.begin();
}

void loop() {
  if (player) {
    player.copy();
  } else {
    // feed watchdog
    delay(10);
  }
  kit.processActions();
}
