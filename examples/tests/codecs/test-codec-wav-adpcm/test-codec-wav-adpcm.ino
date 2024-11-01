#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecWAV.h" 
#include "AudioTools/AudioCodecs/CodecADPCM.h"

AudioInfo info(16000, 2, 16);
SineWaveGenerator<int16_t> sineWave( 32000);  
GeneratedSoundStream<int16_t> sound( sineWave); 
CsvOutput<int16_t> out(Serial);
AVCodecID id = AV_CODEC_ID_ADPCM_IMA_WAV;
ADPCMDecoder adpcm_decoder(id); 
ADPCMEncoder adpcm_encoder(id);  
WAVDecoder wav_decoder(adpcm_decoder, AudioFormat::ADPCM);
WAVEncoder wav_encoder(adpcm_encoder, AudioFormat::ADPCM);
EncodedAudioStream decoder(&out, &wav_decoder); 
EncodedAudioStream encoder(&decoder, &wav_encoder); 
StreamCopy copier(encoder, sound);     

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  // start Output
  auto cfgi = out.defaultConfig(TX_MODE);
  cfgi.copyFrom(info);
  out.begin(cfgi);

  // Setup sine wave
  auto cfgs = sineWave.defaultConfig();
  cfgs.copyFrom(info);
  sineWave.begin(info, N_B4);

  // start decoder
  decoder.begin(info);

  // start encoder
  encoder.begin(info);
}

void loop() { 
  copier.copy();
}
