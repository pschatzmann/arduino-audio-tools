/**
 * @file sd-avi-audio.ino
 * @brief Plays just the audio track of a local .avi file on an SD card,
 * demuxing it live with DemuxerAVI. AVI's audio format varies per file
 * (PCM/WAV, AAC, MP3 are all valid 'strf' choices), so this uses
 * DecoderHelix - a multi-format decoder that auto-detects which one it's
 * looking at - rather than assuming a specific codec.
 *
 * Pipeline: File (SD) -> EncodedAudioOutput (Print bridge) -> DemuxerAVI
 * (demux) -> EncodedAudioStream (DecoderHelix: WAV/AAC/MP3) -> I2SStream
 * (audio)
 *
 * DemuxerAVI only demuxes - it does not own or configure an audio decoder
 * itself; setOutputAudio() just points it at a plain Print, so any decoder
 * matching the track's codec can be wired in externally via an
 * EncodedAudioStream (as done here).
 *
 * DemuxerAVI is a *streaming* (forward-only) demuxer - it does not need a
 * seekable source, so a File read sequentially with StreamCopy (the same
 * way http-client-avi-h264.ino feeds it from a live HTTP download) works
 * fine. setOutputVideo() is left unset here, so the video track (if any) is
 * just ignored - see sd-avi-video.ino / sd-avi-audio-video.ino for versions
 * that also decode and display it.
 *
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
#include "AudioTools.h"
#include "AudioTools/AudioCodecs/ContainerAVI.h"
#include "AudioTools/AudioCodecs/CodecHelix.h"
#include "SD.h"

// ---- File on the SD card to play ----
const char *file_path = "/audio.avi";

I2SStream i2s;
DecoderHelix multiDecoder;
EncodedAudioStream audioOut(&i2s, &multiDecoder);  // decodes PCM/AAC/MP3 -> I2S

DemuxerAVI aviDemuxer;
EncodedAudioOutput aviInput(&aviDemuxer);  // bridges raw file bytes -> DemuxerAVI::write()

File file;
StreamCopy copier(aviInput, file);

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
  multiDecoder.begin();

  aviDemuxer.setOutputAudio(audioOut);
  aviInput.begin();
}

void loop() {
  if (file && !copier.copy()) {
    Serial.println("Done");
    file.close();
  }
}
