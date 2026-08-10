/**
 * @file sd-mp4-audio.ino
 * @brief Plays just the audio track (AAC) out of a local .mp4 file on an SD
 * card, demuxing it live with DemuxerMP4 - same "faststart" MP4 (moov
 * before mdat) DemuxerMP4 requires everywhere else, e.g.:
 *   ffmpeg -i in.mp4 -c:v libx264 -c:a aac -movflags +faststart out.mp4
 *
 * Pipeline: File (SD) -> EncodedAudioOutput (Print bridge) -> DemuxerMP4
 * (demux) -> EncodedAudioStream (AACDecoderHelix) -> I2SStream (audio)
 *
 * DemuxerMP4 only demuxes - it does not own or configure an audio decoder
 * itself; setOutputAudio() just points it at a plain Print, so any decoder
 * matching the track's codec can be wired in externally via an
 * EncodedAudioStream (as done here for AAC).
 *
 * DemuxerMP4 is a *streaming* (forward-only) demuxer - it does not need a
 * seekable source, so a File read sequentially with StreamCopy (the same
 * way http-client-mp4.ino feeds it from a live HTTP download) works fine.
 * setOutputVideo() is left unset here, so the video track (if any) is just
 * ignored - see http-client-mp4.ino for a version that also decodes and
 * displays it.
 *
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
#include "AudioTools.h"
#include "AudioTools/AudioCodecs/ContainerMP4.h"
#include "AudioTools/AudioCodecs/CodecAACHelix.h"
#include "SD.h"

// ---- File on the SD card to play ----
const char *file_path = "/audio.mp4";

I2SStream i2s;
AACDecoderHelix aacDecoder;
EncodedAudioStream audioOut(&i2s, &aacDecoder);  // decodes AAC -> I2S

DemuxerMP4 mp4Demuxer;
EncodedAudioOutput mp4Input(&mp4Demuxer);  // bridges raw file bytes -> DemuxerMP4::write()

File file;
StreamCopy copier(mp4Input, file);

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

  mp4Demuxer.setOutputAudio(audioOut);
  mp4Input.begin();

}

void loop() {
  if (file && !copier.copy()) {
    Serial.println("Done");
    file.close();
  }
}
