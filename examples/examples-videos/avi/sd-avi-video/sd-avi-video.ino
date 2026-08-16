/**
 * @file sd-avi-video.ino
 * @brief Plays just the video (H.264) track of a local .avi file on an SD
 * card: demuxes it live with DemuxerAVI, decodes the video track with
 * H264Decoder (TinyH264, https://github.com/pschatzmann/TinyH264 - pure
 * software, works on any board) and displays the result live on a TFT
 * screen with TFT_eSPI. The audio track (if any) is ignored - DemuxerAVI
 * only decodes/forwards it if setOutputAudio() is called, which this
 * sketch doesn't do.
 *
 * DemuxerAVI needs the video track's biCompression FOURCC to be H264/h264/
 * X264/x264/avc1/AVC1 and its payload to be a raw Annex-B bitstream (start
 * codes, not AVCC length-prefixed NAL units - unlike DemuxerMP4, which
 * converts AVCC to Annex-B itself). http-server-avi-h264.ino (this
 * project's own AVI+H.264 encoder) already writes exactly that; to build a
 * compatible file from an existing .mp4 instead:
 *   ffmpeg -i in.mp4 -c:v libx264 -bsf:v h264_mp4toannexb -an out.avi
 *
 * Pipeline: File (SD) -> CodecCopy -> DemuxerAVI (demux)
 *   \-> H264Decoder (H.264 decode -> RGB565) -> OutputTFT_eSPI (draw)
 *
 * DemuxerAVI is a *streaming* (forward-only) demuxer - it does not need a
 * seekable source, so a File read sequentially with CodecCopy (the same
 * way http-client-avi-h264.ino feeds it from a live HTTP download, via
 * StreamCopy) works fine. See sd-mp4-video.ino for the MP4 equivalent of
 * this file, and http-client-avi-h264.ino for the network (HTTP)
 * equivalent.
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
#include "AudioTools/Video/CodecH264.h"
#include "AudioTools/Video/OutputTFT_eSPI.h"
#include "SD.h"

// ---- File on the SD card to play ----
const char *file_path = "/video.avi";

// File -copy-> DemuxerAVI -> H264Decoder -> OutputTFT_eSPI

TFT_eSPI tft = TFT_eSPI();
OutputTFT_eSPI tftOutput(tft);
H264Decoder h264Decoder(tftOutput);
DemuxerAVI aviDemuxer;
File file;
CodecCopy copier(aviDemuxer, file);


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

  tftOutput.setVideoInfoSource(h264Decoder);
  tftOutput.begin();
  h264Decoder.begin();

  aviDemuxer.setOutputVideo(h264Decoder);
  aviDemuxer.begin();
}


void loop() {
  if (file && !copier.copy()) {
    Serial.println("Done");
    file.close();
  }
}
