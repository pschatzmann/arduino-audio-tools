/**
 * @file sd-mp4-audio-video.ino
 * @brief Plays both the audio (AAC) and video (H.264) tracks of a local
 * .mp4 file on an SD card: demuxes it live with DemuxerMP4, decodes the
 * video track with H264Decoder (TinyH264,
 * https://github.com/pschatzmann/TinyH264 - pure software, works on any
 * board) and displays the result live on a TFT screen with TFT_eSPI, while
 * playing the audio track through I2S. Same "faststart" MP4 (moov before
 * mdat) DemuxerMP4 requires everywhere else, e.g.:
 *   ffmpeg -i in.mp4 -c:v libx264 -c:a aac -movflags +faststart out.mp4
 *
 * Pipeline: File (SD) -> EncodedAudioOutput (Print bridge) -> DemuxerMP4
 * (demux)
 *   -> EncodedAudioStream (AACDecoderHelix) -> I2SStream (audio)
 *   \-> H264Decoder (H.264 decode -> RGB565) -> OutputTFT (draw) (video)
 *
 * DemuxerMP4 is a *streaming* (forward-only) demuxer - it does not need a
 * seekable source, so a File read sequentially with StreamCopy (the same
 * way http-client-mp4.ino feeds it from a live HTTP download) works fine.
 * See sd-mp4-audio.ino for an audio-only version of this same file, and
 * http-client-mp4.ino for the network (HTTP) equivalent of this one.
 *
 * On an ESP32-S3 board, swap H264Decoder for H264DecoderESP32S3
 * (AudioTools/Video/CodecH264ESP32S3.h) to use the hardware/esp_h264
 * backend (https://github.com/pschatzmann/ESP32S3-h264) instead - same
 * setOutput()/setVideoFormat() surface, no other change needed below.
 *
 * Dependencies (install via Library Manager):
 * - https://github.com/Bodmer/TFT_eSPI (configure your display's pins/driver
 *   in that library's User_Setup.h - not done in this sketch)
 * - https://github.com/pschatzmann/TinyH264
 *
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
#include "AudioTools.h"
#include "AudioTools/AudioCodecs/ContainerMP4.h"
#include "AudioTools/AudioCodecs/CodecAACHelix.h"
#include "AudioTools/Video/CodecH264.h"
#include "AudioTools/Video/OutputTFT_eSPI.h"
#include "SD.h"

// ---- File on the SD card to play ----
const char *file_path = "/video.mp4";

// File -cop-> DemuxerMP4 -> H264Decoder -> OutputTFT

TFT_eSPI tft = TFT_eSPI();
OutputTFT_eSPI tftOutput(tft);
H264Decoder h264Decoder(tftOutput);
NullStream audio;
DemuxerMP4 mp4Demuxer(h264Decoder, audio);
File file;
CodecCopy copier(mp4Demuxer, file);


void setup() {
  Serial.begin(115200);
  tftOutput.setVideoInfoSource(h264Decoder);
  tftOutput.begin();
  h264Decoder.begin();
  mp4Demuxer.begin();
}


void loop() {
  if (file && !copier.copy()) {
    Serial.println("Done");
    file.close();
  }
}
