/**
 * @file spi-reg-slave.ino
 * @brief SPI audio slave based on the TinySPISlave library. The master defines
 * the mime type via a register and writes the (encoded) audio data according
 * to the available buffer space register. A MultiDecoder with MP3, AAC (Helix)
 * and WAV support is selected by the provided mime type. For audio/pcm the
 * decoder is bypassed and the AudioInfo register defines the output format.
 * Use together with spi-reg-master. Requires the TinySPISlave
 * (https://github.com/pschatzmann/TinySPISlave) and arduino-libhelix libraries.
 *
 * Supported mime types: audio/mpeg, audio/aac, audio/wav, audio/pcm
 */
#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecHelix.h"
#include "TinySPISlave.h"  // must be included before SPIRegAudio.h
#include "AudioTools/Communication/SPI/SPIRegAudio.h"

#if defined(ARDUINO_ARCH_RP2040)
// pins are fixed by the RP2040 function select
const int SLAVE_MOSI = 16, SLAVE_CS = 17, SLAVE_SCK = 18, SLAVE_MISO = 19;
#else
const int SLAVE_SCK = 18, SLAVE_MISO = 19, SLAVE_MOSI = 23, SLAVE_CS = 5;
#endif

RingBuffer<uint8_t> buffer(16 * 1024);
SPIRegAudioSlave slave(buffer);
I2SStream out;
MP3DecoderHelix mp3;
AACDecoderHelix aac;
WAVDecoder wav;
MultiDecoder multi(slave);  // decoder is selected by the mime of the slave
EncodedAudioOutput dec(&out, &multi);  // decoders set the format of out
StreamCopy copier(dec, slave);      // encoded data: via decoder
StreamCopy pcm_copier(out, slave);  // audio/pcm: directly to the output
bool is_pcm = false;

// A new mime type: restart the decoder so that it gets selected again
void onMime(const char* mime, void*) {
  LOGI("mime: %s", mime);
  is_pcm = StrView(mime).equalsIgnoreCase("audio/pcm");
  dec.end();
  dec.begin();
}

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  multi.addDecoder(mp3, "audio/mpeg");
  multi.addDecoder(aac, "audio/aac");
  multi.addDecoder(wav, "audio/wav");

  out.begin(out.defaultConfig(TX_MODE));

  auto cfg = SPIRegAudioSlaveConfig();
  cfg.sck = SLAVE_SCK;
  cfg.miso = SLAVE_MISO;
  cfg.mosi = SLAVE_MOSI;
  cfg.cs = SLAVE_CS;
  slave.addNotifyAudioChange(out);  // AudioInfo register -> output (pcm)
  slave.setMimeCallback(onMime);
  if (!slave.begin(cfg)) stop();

  dec.begin();
}

void loop() {
  slave.update();  // applies AudioInfo / mime changes
  if (is_pcm) {
    pcm_copier.copy();
  } else {
    copier.copy();
  }
}
