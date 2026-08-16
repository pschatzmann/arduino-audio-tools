/**
 * @file sd-mpg-audio.ino
 * @brief Plays just the audio track out of a local MPEG-1 Program Stream
 * (.mpg) file on an SD card, demuxing it live with DemuxerMPG. The audio
 * track is a raw MPEG-1 Layer I/II/III elementary stream (self-delimiting
 * via its own frame sync word, no container-level format tag), decoded
 * here with MP3DecoderHelix - the same choice ContainerMPG.h's own file
 * comment uses.
 *
 * Pipeline: File (SD) -> EncodedAudioOutput (Print bridge) -> DemuxerMPG
 * (demux) -> EncodedAudioStream (MP3DecoderHelix) -> I2SStream (audio)
 *
 * DemuxerMPG only demuxes - it does not own or configure an audio decoder
 * itself; setOutputAudio() just points it at a plain Print, so any decoder
 * matching the track's codec can be wired in externally via an
 * EncodedAudioStream (as done here).
 *
 * DemuxerMPG is a *streaming* (forward-only) demuxer - it does not need a
 * seekable source, so a File read sequentially with StreamCopy (the same
 * way http-client-mpg.ino feeds it from a live HTTP download) works fine.
 * setOutputVideo() is left unset here, so the video track (if any) is just
 * ignored - see sd-mpg-video.ino / sd-mpg-audio-video.ino for versions that
 * also decode and display it.
 *
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
#include "AudioTools.h"
#include "AudioTools/AudioCodecs/ContainerMPG.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"
#include "SD.h"

// ---- File on the SD card to play ----
const char *file_path = "/audio.mpg";

I2SStream i2s;
MP3DecoderHelix mp3Decoder;
EncodedAudioStream audioOut(&i2s, &mp3Decoder);  // decodes MP3/MP2 -> I2S

DemuxerMPG mpgDemuxer;
EncodedAudioOutput mpgInput(&mpgDemuxer);  // bridges raw file bytes -> DemuxerMPG::write()

File file;
StreamCopy copier(mpgInput, file);

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  if (!SD.begin()) {
    Serial.println("SD Card initialization failed!");
    return;
  }
  file = SD.open(file_path);
  if (!file) {
    Serial.print("Could not open ");
    Serial.println(file_path);
    return;
  }

  auto cfg = i2s.defaultConfig(TX_MODE);
  i2s.begin(cfg);
  audioOut.begin();

  mpgDemuxer.setOutputAudio(audioOut);
  mpgInput.begin();
}

void loop() {
  if (file && !copier.copy()) {
    Serial.println("Done");
    file.close();
  }
}
