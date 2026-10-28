/**
 * @file spi-reg-master.ino
 * @brief SPI master which sends a sine wave as WAV to a SPIRegAudioSlave (see
 * spi-reg-slave). It defines the AudioInfo and mime type and writes the audio
 * data according to the available buffer space reported by the slave.
 * To send mp3 or aac use the corresponding encoder and the mime type
 * "audio/mpeg" or "audio/aac". For raw PCM write to the master directly and
 * use the mime type "audio/pcm" (the AudioInfo defines the format).
 */
#include "AudioTools.h"
#include "AudioTools/Communication/SPI/SPIRegAudio.h"

AudioInfo info(44100, 2, 16);
SineWaveGenerator<int16_t> sineWave(16000);
GeneratedSoundStream<int16_t> sound(sineWave);
SPIRegAudioMaster master;
WAVEncoder wav;
EncodedAudioOutput enc(&master, &wav);
StreamCopy copier(enc, sound);

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  auto cfg = SPIRegAudioMasterConfig();
  cfg.copyFrom(info);
  cfg.cs_pin = 5;
  cfg.clock_hz = 4000000;
  cfg.is_blocking_write = true;  // the encoder can not repeat a write
  master.begin(cfg);             // defines the AudioInfo on the slave
  master.setMime(wav.mime());    // "audio/wav"

  sineWave.begin(info, N_B4);
  enc.begin(info);
}

void loop() { copier.copy(); }
