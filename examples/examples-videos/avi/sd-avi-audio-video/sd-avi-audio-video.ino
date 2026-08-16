/**
 * @file sd-avi-audio-video.ino
 * @brief Plays both the audio and video (H.264) tracks of a local .avi
 * file on an SD card: demuxes it live with DemuxerAVI, decodes the video
 * track with H264Decoder (TinyH264, https://github.com/pschatzmann/TinyH264
 * - pure software, works on any board) and displays the result live on a
 * TFT screen with TFT_eSPI, while playing the audio track through I2S.
 * AVI's audio format varies per file (PCM/WAV, AAC, MP3 are all valid
 * 'strf' choices), so this uses DecoderHelix - a multi-format decoder that
 * auto-detects which one it's looking at - rather than assuming a specific
 * codec.
 *
 * DemuxerAVI needs the video track's biCompression FOURCC to be H264/h264/
 * X264/x264/avc1/AVC1 and its payload to be a raw Annex-B bitstream - see
 * sd-avi-video.ino's own comment for how to build a compatible file with
 * ffmpeg (add e.g. -c:a aac for an audio track too).
 *
 * Pipeline: File (SD) -> EncodedAudioOutput (Print bridge) -> DemuxerAVI
 * (demux)
 *   -> EncodedAudioStream (DecoderHelix: WAV/AAC/MP3) -> I2SStream (audio)
 *   \-> H264Decoder (H.264 decode -> RGB565) -> OutputTFT_eSPI (draw) (video)
 *
 * DemuxerAVI is a *streaming* (forward-only) demuxer - it does not need a
 * seekable source, so a File read sequentially with StreamCopy (the same
 * way http-client-avi-h264.ino feeds it from a live HTTP download) works
 * fine. See sd-avi-audio.ino for an audio-only version of this same file,
 * sd-avi-video.ino for video-only, and http-client-avi-h264.ino for the
 * network (HTTP) equivalent.
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
#include "AudioTools/AudioCodecs/ContainerAVI.h"
#include "AudioTools/AudioCodecs/CodecHelix.h"
#include "AudioTools/Video/CodecH264.h"
#include "AudioTools/Video/OutputTFT_eSPI.h"
#include "SD.h"

// ---- File on the SD card to play ----
const char *file_path = "/video.avi";

TFT_eSPI tft = TFT_eSPI();
H264Decoder h264Decoder;
OutputTFT_eSPI tftOutput(tft);
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

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  auto cfg = i2s.defaultConfig(TX_MODE);
  i2s.begin(cfg);
  audioOut.begin();
  multiDecoder.begin();

  h264Decoder.setOutput(tftOutput);  // RGB565 (the default) matches
                                      // pushImage()'s expected format
  tftOutput.setVideoInfoSource(h264Decoder);
  h264Decoder.begin();

  aviDemuxer.setOutputAudio(audioOut);
  aviDemuxer.setOutputVideo(h264Decoder);
  aviInput.begin();
}

void loop() {
  if (file && !copier.copy()) {
    Serial.println("Done");
    file.close();
  }
}
