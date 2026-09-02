/**
 * @file streams-generator-webserver_adpcm.ino
 *
 *  This sketch generates a test sine wave and provides it as an IMA/DVI
 *  ADPCM encoded WAV stream. Compared to plain PCM, ADPCM only needs 4 bits
 *  per sample, so this roughly quarters the required bandwidth.
 *
 *  NOTE: this will NOT play in Chrome (or most browsers) - their built-in
 *  WAV decoder only supports plain PCM (and IEEE float/A-law/mu-law), not
 *  ADPCM. Use ffplay, VLC, or an ADPCMDecoder-based client to listen.
 *
 * @author Phil Schatzmann
 * @copyright GPLv3
 *
 */

#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecADPCM.h"
#include "AudioTools/Communication/AudioHttp.h"

// WIFI
const char *ssid = "ssid";
const char *password = "password";

AudioWAVServer server(ssid, password);

// Sound Generation
const int sample_rate = 16000;
const int channels = 1;

SineGenerator<int16_t> sineWave;             // Subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> in(sineWave);  // Stream generated from sine wave
ADPCMEncoder adpcmEncoder(AV_CODEC_ID_ADPCM_IMA_WAV);  // IMA/DVI ADPCM


void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  // use ADPCM instead of the default PCM to reduce the needed bandwidth
  server.wavEncoder().setEncoder(adpcmEncoder, AudioFormat::DVI_ADPCM);
  // write the extended 'fmt '/'fact' chunks so that clients relying on them
  // (e.g. to determine the number of samples) can correctly decode the file
  // NOTE: setEncoder() now enables this automatically for ADPCM formats, so
  // this explicit call is not be needed any more in the latest release.
  server.wavEncoder().setExtADPCMHeader(true);

  // start server
  server.begin(in, sample_rate, channels);

  // start generation of sound
  sineWave.begin(channels, sample_rate, N_B4);
  in.begin();
}


// copy the data
void loop() {
  server.copy();
}
