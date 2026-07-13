#pragma once

#include "AudioTools/CoreAudio/Buffers.h"
#include "AudioTools/CoreAudio/BaseStream.h"
#include "LoRa.h" // 👉 https://github.com/sandeepmistry/arduino-LoRa

// define the default pins used by the transceiver module
#define ss 8
#define rst 12
#define dio0 14

namespace audio_tools {

/**
 * @brief Configuration for AudioLoRa.
 *
 * LoRa is a long-range, low-power radio technology designed for occasional
 * telemetry (a few bytes every so often), not for continuous audio. LoRa's
 * PHY settings trade range/robustness for throughput: spreading_factor
 * ranges from 6 (fastest, shortest range) to 12 (slowest, longest range),
 * and signal_bandwidth from 7.8 kHz (slowest, longest range) to 500 kHz
 * (fastest, shortest range). The defaults below (spreading_factor = 6,
 * signal_bandwidth = 500 kHz) pick LoRa's fastest, shortest-range end of
 * that scale, since audio needs throughput far more than a typical LoRa
 * telemetry use case does. If you want LoRa's usual long range/robustness
 * instead and don't care about audio, override these back towards 12 /
 * 7.8 kHz -- but expect only tens of bits per second at that end, which
 * cannot carry audio at all.
 *
 * Even at these fast defaults, usable throughput is still only on the
 * order of tens of kbps -- nowhere near typical PCM bitrates (8 kHz/8-bit
 * mono alone is 64 kbps) -- so audio only becomes practical once reduced
 * to a very low sample rate, 8-bit and/or mono, and ideally compressed
 * before it reaches this stream. Treat this as a fit for short clips or
 * intermittent bursts, not a continuous/live stream; several regions
 * additionally impose legal duty-cycle limits on the ISM bands LoRa uses
 * (e.g. roughly 1% airtime in the EU 868 MHz band) that a sustained stream
 * would likely violate.
 *
 * Codec recommendation: encode speech with Codec2 (Codec2Encoder /
 * Codec2Decoder in AudioTools/AudioCodecs/CodecCodec2.h) before writing to
 * this stream, and decode it back out after reading. Codec2 was designed
 * for exactly this situation -- squeezing intelligible speech over
 * narrowband radio links (it's the codec behind ham radio digital voice
 * modes like FreeDV) -- and offers modes from 3200 bit/s down to as low as
 * 450 bit/s. At its 700 bit/s mode, a single ~200 byte LoRa packet carries
 * over 2 seconds of speech, compared to a fraction of a second of raw
 * 8-bit PCM; that's the difference between this being usable for short
 * voice messages and not being usable at all. See the full codec list at
 * https://github.com/pschatzmann/arduino-audio-tools/wiki/Encoding-and-Decoding-of-Audio
 * for alternatives if Codec2 doesn't fit your case (e.g. GSM 06.10 at
 * ~13 kbps if you have more throughput headroom and want wider codec
 * compatibility with other systems).
 *
 * Heltec LoRa 32 pins (as used by pin_ss/pin_rst/pin_dio0 below):
 *   NSS: 8, SCK: 9, MOSI: 10, MISO: 11, RST: 12, BUSY: 13, DIO1: 14
 *
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
struct AudioLoRaConfig : public AudioInfo {
  int32_t spi_speed = 8E6;
  int max_size = 200;  // LoRa's payload cap is ~256 bytes
  int frequency = 868E6;  // (433E6, 868E6, 915E6)
  int sync_word = 0xF3;
  int tx_power = 20;         // 2-20;
  int spreading_factor = 6;  // 6 (fastest/shortest range) - 12 (slowest/longest range)
  int signal_bandwidth =
      500E3;  // 7.8E3 (slowest/longest range) - 500E3 (fastest/shortest range):
              // 7.8E3, 10.4E3, 15.6E3, 20.8E3, 31.25E3, 41.7E3,
              // 62.5E3, 125E3, 250E3, 500E3.
  int pin_ss = ss;
  int pin_rst = rst;
  int pin_dio0 = dio0;
  /// Send/expect a leading AudioInfo header packet (see AudioLoRa's class
  /// doc for how this is used and its single-point-of-failure caveat).
  bool process_audio_info = true;
};

/**
 * @brief Sends and receives audio over a LoRa radio link (via the
 * sandeepmistry/arduino-LoRa library) as a regular AudioTools Stream, so it
 * can be dropped into a StreamCopy pipeline like any other audio
 * source/sink -- e.g. `StreamCopy(audioLoRa, i2sMic)` to transmit, or
 * `StreamCopy(i2sSpeaker, audioLoRa)` to receive. See AudioLoRaConfig's
 * documentation first for why, even with its audio-tuned defaults, this
 * only works at all in a narrow envelope (very low bitrate, short
 * clips/bursts).
 *
 * Wire protocol: if `cfg.process_audio_info` is set (the default),
 * begin() sends a single LoRa packet containing a raw AudioInfo struct
 * (sample rate/channels/bits) right away, so a receiver can learn the
 * format instead of needing it configured out of band. From then on,
 * write() appends audio bytes to an internal buffer and transmits one
 * LoRa packet each time it fills to `cfg.max_size`. On the receiving
 * side, readBytes() checks each incoming packet's size and, whenever it
 * matches sizeof(AudioInfo), decodes it as a header via setAudioInfo()
 * instead of returning it as audio payload.
 *
 * Known limitations (review notes, not yet fixed):
 * - The header is sent exactly once, synchronously inside begin(), with
 *   no retry or periodic re-announcement. If the receiving side hasn't
 *   called its own begin() yet at that exact moment -- a likely race when
 *   two independently-powered boards start up -- that one packet is
 *   simply gone, and the receiver never learns the audio format. Consider
 *   calling begin() on the receiver well before the transmitter, or
 *   configuring both sides identically and setting
 *   `process_audio_info = false` to skip the handshake entirely.
 * - readBytes()'s header check is driven by the `process_audio_info`
 *   *setting*, not by whether a header has already been received -- so
 *   it keeps reinterpreting any packet of exactly sizeof(AudioInfo) bytes
 *   as a fresh header for the life of the stream. With the default
 *   `max_size = 200`, ordinary audio packets are always ~200 bytes so
 *   this is unlikely to misfire in practice, but it becomes a real risk
 *   if `max_size` is ever configured close to sizeof(AudioInfo).
 * - begin() calls LoRa.begin(cfg.frequency) exactly once; if the radio
 *   isn't ready yet it fails immediately rather than retrying.
 * - available()/availableForWrite() always return `cfg.max_size`, a
 *   constant, rather than reflecting whether a packet has actually
 *   arrived or whether the radio is free.
 * - write() only flushes once its buffer is exactly full; any partial
 *   buffer content left over when the stream ends is never sent (no
 *   flush(), and end() does not drain it), so trailing audio can be lost.
 * - LoRa itself provides no retransmission/ACK for these packets, so a
 *   lost or corrupted packet is simply missing audio (an audible gap),
 *   with no recovery built in here.
 * - Requires the external, separately-installed sandeepmistry/arduino-LoRa
 *   library and matching transceiver hardware (e.g. an SX127x module);
 *   this header also defines `ss`/`rst`/`dio0` as short, generic macros in
 *   the global namespace -- `ss` in particular (a very common local
 *   variable name, e.g. std::stringstream ss) is likely to collide with
 *   unrelated code that includes this header.
 *
 * @note Supported only on Arduino platforms with LoRa support (e.g. ESP32) and the LoRa library!
 * @note Requires: 👉 https://github.com/sandeepmistry/arduino-LoRa
 *
 * @author Phil Schatzmann
 * @ingroup communications
 * @copyright GPLv3
 */
class AudioLoRa : public AudioStream {
 public:
  AudioLoRaConfig defaultConfig() {
    AudioLoRaConfig rc;
    return rc;
  }

  void setAudioInfo(AudioInfo info) {
    cfg.sample_rate = info.sample_rate;
    cfg.channels = info.channels;
    cfg.bits_per_sample = info.bits_per_sample;
    AudioStream::setAudioInfo(info);
  }

  bool begin(AudioLoRaConfig config) {
    cfg = config;
    AudioStream::setAudioInfo(config);
    return begin();
  }

  bool begin() {
    TRACEI();
    buffer.resize(cfg.max_size);
    LoRa.setSignalBandwidth(cfg.signal_bandwidth);
    LoRa.setSpreadingFactor(cfg.spreading_factor);
    LoRa.setTxPower(cfg.tx_power);
    LoRa.setSPIFrequency(cfg.spi_speed);
    LoRa.setPins(cfg.pin_ss, cfg.pin_rst, cfg.pin_dio0);
    LoRa.setSyncWord(cfg.sync_word);
    bool rc = LoRa.begin(cfg.frequency);
    if (cfg.process_audio_info) {
      writeAudioInfo();
    }
    return rc;
  }

  void end() { LoRa.end(); }

  size_t readBytes(uint8_t* data, size_t len) {
    TRACEI();
    size_t packetSize = LoRa.parsePacket();
    if (cfg.process_audio_info && packetSize == sizeof(AudioInfo)) {
      readAudioInfo();
      packetSize = LoRa.parsePacket();
    }
    int toRead = min(len, packetSize);
    int read = LoRa.readBytes(data, toRead);
    return read;
  }

  int available() { return cfg.max_size; }

  int availableForWrite() { return cfg.max_size; }

  size_t write(const uint8_t* data, size_t len) {
    TRACEI();
    for (int j = 0; j < len; j++) {
      buffer.write(data[j]);
      if (buffer.isFull()) {
        LoRa.beginPacket();
        LoRa.write(buffer.data(), buffer.available());
        LoRa.endPacket();
        buffer.clear();
      }
    }
    return len;
  }

 protected:
  AudioLoRaConfig cfg;
  SingleBuffer<uint8_t> buffer;

  void readAudioInfo() {
    AudioInfo tmp;
    int read = LoRa.readBytes((uint8_t*)&tmp, sizeof(AudioInfo));
    setAudioInfo(tmp);
  }

  void writeAudioInfo() {
    LoRa.beginPacket();
    AudioInfo ai = audioInfo();
    LoRa.write((uint8_t*)&ai, sizeof(ai));
    LoRa.endPacket();
  }
};

}  // namespace audio_tools