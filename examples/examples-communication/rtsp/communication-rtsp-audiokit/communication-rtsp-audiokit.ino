
/**
 * @file communication-rtsp-i2s.ino
 * @author Phil Schatzmann
 * @brief Demo for RTSP Client that is playing mp3. I tested with the live555 server with linux 
 * @version 0.1
 * @date 2022-05-02
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include "AudioTools.h" // https://github.com/pschatzmann/arduino-audio-tools
#include "AudioCodecs/CodecMP3Helix.h" // https://github.com/pschatzmann/arduino-libhelix
#include "AudioLibs/AudioBoardStream.h" // https://github.com/pschatzmann/arduino-audio-driver
#include "AudioLibs/AudioClientRTSP.h" // install https://github.com/pschatzmann/arduino-live555 

AudioBoardStream i2s(AudioKitEs8388V1); // final output of decoded stream
EncodedAudioStream out_mp3(&i2s, new MP3DecoderHelix()); // Decoding stream
AudioClientRTSP rtsp(1024);

void setup(){
    rtsp.setLogin("ssid", "password");
    rtsp.begin("https://samples.mplayerhq.hu/A-codecs/MP3/01%20-%20Charity%20Case.mp3", out_mp3);
}

void loop() {
    rtsp.loop();
}