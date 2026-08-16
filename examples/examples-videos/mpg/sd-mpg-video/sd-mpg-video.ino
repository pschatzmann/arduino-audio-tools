/**
 * @file sd-mpg-video.ino
 * @brief Plays a local MPEG-1 Program Stream (.mpg) file on an SD card:
 * demuxes it live with DemuxerMPG, decodes the video track with MPGDecoder
 * (TinyMPG, https://github.com/pschatzmann/TinyMPG - pure software, works
 * on any board) and displays the result live on a TinyGPU-driven TFT. The
 * audio track (if any) is ignored - DemuxerMPG only decodes/forwards it if
 * setOutputAudio() is called, which this sketch doesn't do.
 *
 * Unlike DemuxerAVI/DemuxerMP4, DemuxerMPG needs no external wrapper
 * format - ISO/IEC 11172-1's own pack_header/system_header/PES_packet
 * framing *is* the container. To build a compatible test file:
 *   ffmpeg -i in.mp4 -c:v mpeg1video -an -f mpeg1video out.mpg
 * (or use http-server-mpg.ino, this project's own MuxerMPG encoder, and
 * save its HTTP output to a file).
 *
 * Pipeline: File (SD) -> CodecCopy -> DemuxerMPG (demux)
 *   \-> MPGDecoder (MPEG-1 decode -> RGB565) -> OutputTinyGPU (draw)
 *
 * DemuxerMPG is a *streaming* (forward-only) demuxer - it does not need a
 * seekable source, so a File read sequentially with CodecCopy (the same
 * way http-client-mpg.ino feeds it from a live HTTP download, via
 * StreamCopy) works fine. See sd-mp4-video.ino/sd-avi-video.ino for the
 * MP4/AVI equivalents of this file, and http-client-mpg.ino for the
 * network (HTTP) equivalent.
 *
 * Dependencies (install via Library Manager):
 * - https://github.com/pschatzmann/TinyGPU (SPI/display pins below match its
 *   bouncing-ball example - adjust for your own wiring)
 * - https://github.com/pschatzmann/TinyMPG
 *
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
#include "AudioTools.h"
#include "AudioTools/AudioCodecs/ContainerMPG.h"
#include "AudioTools/Video/CodecMPG.h"
#include "AudioTools/Video/OutputTinyGPU.h"
#include "SD.h"

// ---- File on the SD card to play ----
const char *file_path = "/video.mpg";

// File -copy-> DemuxerMPG -> MPGDecoder -> OutputTinyGPU

// ---- SPI / display pins (adjust for your wiring) ----
constexpr int8_t kPinMosi = 13;
constexpr int8_t kPinMiso = 12;
constexpr int8_t kPinSclk = 14;
constexpr int8_t kPinCs = 15;
constexpr int8_t kPinDc = 2;
constexpr int8_t kPinRst = -1;
constexpr int8_t kPinBacklight = 27;

// display resolution - used by OutputTinyGPU's begin()/clearScreen()
// sizing; the actual per-frame size still comes from MPGDecoder via
// setVideoInfoSource()
const uint16_t video_width = 320;
const uint16_t video_height = 240;

ILI9341Driver<RGB565> tftDriver(SPI, kPinCs, kPinDc, kPinRst);
OutputTinyGPU tftOutput(tftDriver, video_width, video_height, kPinBacklight);
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

  SPI.begin(kPinSclk, kPinMiso, kPinMosi, kPinCs);
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
