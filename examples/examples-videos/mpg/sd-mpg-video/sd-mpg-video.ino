/**
 * @file sd-mpg-video.ino
 * @brief Plays a local MPEG-1 Program Stream (.mpg) file on an SD card:
 * demuxes it live with DemuxerMPG, decodes the video track with MPGDecoder
 * (TinyMPG, https://github.com/pschatzmann/TinyMPG - pure software, works
 * on any board) and displays the result live on a TFT screen with
 * TFT_eSPI. The audio track (if any) is ignored - DemuxerMPG only decodes/
 * forwards it if setOutputAudio() is called, which this sketch doesn't do.
 *
 * Unlike DemuxerAVI/DemuxerMP4, DemuxerMPG needs no external wrapper
 * format - ISO/IEC 11172-1's own pack_header/system_header/PES_packet
 * framing *is* the container. To build a compatible test file:
 *   ffmpeg -i in.mp4 -c:v mpeg1video -an -f mpeg1video out.mpg
 * (or use http-server-mpg.ino, this project's own MuxerMPG encoder, and
 * save its HTTP output to a file).
 *
 * Pipeline: File (SD) -> CodecCopy -> DemuxerMPG (demux)
 *   \-> MPGDecoder (MPEG-1 decode -> RGB565) -> OutputTFT_eSPI (draw)
 *
 * DemuxerMPG is a *streaming* (forward-only) demuxer - it does not need a
 * seekable source, so a File read sequentially with CodecCopy (the same
 * way http-client-mpg.ino feeds it from a live HTTP download, via
 * StreamCopy) works fine. See sd-mp4-video.ino/sd-avi-video.ino for the
 * MP4/AVI equivalents of this file, and http-client-mpg.ino for the
 * network (HTTP) equivalent.
 *
 * Dependencies (install via Library Manager):
 * - https://github.com/Bodmer/TFT_eSPI (configure your display's pins/driver
 *   in that library's User_Setup.h - not done in this sketch)
 * - https://github.com/pschatzmann/TinyMPG
 *
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
#include "AudioTools.h"
#include "AudioTools/AudioCodecs/ContainerMPG.h"
#include "AudioTools/Video/CodecMPG.h"
#include "AudioTools/Video/OutputTFT_eSPI.h"
#include "SD.h"

// ---- File on the SD card to play ----
const char *file_path = "/video.mpg";

// File -copy-> DemuxerMPG -> MPGDecoder -> OutputTFT_eSPI

TFT_eSPI tft = TFT_eSPI();
OutputTFT_eSPI tftOutput(tft);
MPGDecoder mpgDecoder(tftOutput);
DemuxerMPG mpgDemuxer;
File file;
CodecCopy copier(mpgDemuxer, file);


void setup() {
  Serial.begin(115200);

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

  tftOutput.setVideoInfoSource(mpgDecoder);
  tftOutput.begin();
  mpgDecoder.begin();

  mpgDemuxer.setOutputVideo(mpgDecoder);
  mpgDemuxer.begin();
}


void loop() {
  if (file && !copier.copy()) {
    Serial.println("Done");
    file.close();
  }
}
