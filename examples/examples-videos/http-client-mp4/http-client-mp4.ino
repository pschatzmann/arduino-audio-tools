/**
 * @file http-client-mp4.ino
 * @brief Connects to an HTTP MP4 stream (interleaved H.264 video + AAC
 * audio, "faststart" muxed so moov comes before mdat), demuxes it live with
 * DemuxerMP4, plays the audio track through I2S, and writes the demuxed
 * H.264 Annex-B elementary stream to an SD file for later inspection (there
 * is no on-device H.264 pixel decoder anywhere in this library - same scope
 * boundary as the AVI examples; check the result on a PC with e.g.
 * `ffplay video.h264`).
 *
 * Pipeline: URLStream (HTTP) -> EncodedAudioOutput (Print bridge) ->
 * DemuxerMP4 (interleaved demux)
 *   -> EncodedAudioStream (AACDecoderHelix) -> I2SStream (audio)
 *   \-> FileVideoOutput -> SD file (video)
 *
 * DemuxerMP4 only demuxes - it does not own or configure an audio decoder
 * itself; setOutputAudio() just points it at a plain Print, so any decoder
 * matching the track's codec can be wired in externally via an
 * EncodedAudioStream (as done here for AAC).
 *
 * DemuxerMP4 is a *streaming* (forward-only) demuxer - it does not need a
 * seekable source, so unlike a typical "seek to the next chunk" MP4 reader
 * it works directly over a live HTTP download. It does require the file to
 * be faststart-muxed, e.g.:
 *   ffmpeg -i in.mp4 -c:v libx264 -c:a aac -movflags +faststart out.mp4
 *
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
#include "AudioTools.h"
#include "AudioTools/Communication/AudioHttp.h"
#include "AudioTools/AudioCodecs/ContainerMP4.h"
#include "AudioTools/AudioCodecs/CodecAACHelix.h"
#include "SD.h"

// ---- WiFi ----
const char *ssid = "ssid";
const char *password = "password";

// ---- Video source: the URL of a faststart-muxed H.264+AAC .mp4 ----
const char *video_url = "http://192.168.1.100/video.mp4";

// Writes the demuxed H.264 Annex-B stream to an SD file (no on-device H.264
// pixel decoder exists in this library, so this is as far as video can go)
class FileVideoOutput : public VideoOutput {
 public:
  File file;
  size_t write(const uint8_t *data, size_t len) override {
    return file ? file.write(data, len) : 0;
  }
};

I2SStream i2s;
AACDecoderHelix aacDecoder;
EncodedAudioStream audioOut(&i2s, &aacDecoder);  // decodes AAC -> I2S
FileVideoOutput videoOut;

// video-only-vs-audio-only decision is per track inside DemuxerMP4; both
// sinks are wired in and it dispatches each demuxed sample to the right one
DemuxerMP4 mp4Demuxer;
EncodedAudioOutput mp4Input(&mp4Demuxer);  // bridges raw HTTP bytes -> DemuxerMP4::write()

URLStream url(ssid, password);
StreamCopy copier;

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  if (!SD.begin()) {
    Serial.println("SD Card initialization failed!");
    return;
  }
  videoOut.file = SD.open("/video.h264", FILE_WRITE);
  if (!videoOut.file) {
    Serial.println("Could not open /video.h264 for writing");
    return;
  }

  auto cfg = i2s.defaultConfig(TX_MODE);
  i2s.begin(cfg);
  audioOut.begin();
  mp4Demuxer.setOutputAudio(audioOut);
  mp4Demuxer.setOutputVideo(videoOut);

  mp4Input.begin();

  if (!url.begin(video_url, "video/mp4")) {
    Serial.println("Connection to video server failed");
    while (true) delay(1000);
  }

  copier.begin(mp4Input, url);
}

void loop() {
  if (url) {
    // pumps bytes from the HTTP stream into DemuxerMP4, which routes
    // decoded AAC audio to I2S and demuxed H.264 video to the SD file
    copier.copy();
  } else {
    Serial.println("Disconnected - reconnecting...");
    delay(2000);
    if (url.begin(video_url, "video/mp4")) {
      copier.begin(mp4Input, url);
    }
  }
}
