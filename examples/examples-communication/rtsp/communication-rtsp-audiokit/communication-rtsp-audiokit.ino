
#include "AudioTools.h" // https://github.com/pschatzmann/arduino-audio-tools
#include "AudioCodecs/CodecMP3Helix.h" // https://github.com/pschatzmann/arduino-libhelix
#include "AudioLibs/AudioKit.h" // https://github.com/pschatzmann/arduino-audiokit
#include "AudioLibs/AudioClientRTSP.h" // install https://github.com/pschatzmann/arduino-live555 

AudioKitStream i2s; // final output of decoded stream
EncodedAudioStream out_mp3(&i2s, new MP3DecoderHelix()); // Decoding stream
AudioClientRTSP rtsp(1024);

void setup(){
    rtsp.setLogin("ssid", "password");
    rtsp.begin("https://samples.mplayerhq.hu/A-codecs/MP3/01%20-%20Charity%20Case.mp3", out_mp3);
}

void loop() {
    rtsp.loop();
}