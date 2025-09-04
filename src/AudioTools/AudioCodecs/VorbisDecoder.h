#pragma once


#include "AudioTools/AudioCodecs/AudioCodecsBase.h"
#include "AudioTools/CoreAudio/Buffers.h"

#include <vorbis.h>

namespace audio_tools {

/**
 * @brief Vorbis Audio Decoder using low-level libvorbis API
 *
 * This decoder expects Ogg Vorbis packets to be provided via the write()
 * method. It parses the Vorbis headers, initializes the decoder, and outputs
 * PCM audio.
 *
 * Usage:
 * 1. Call begin() to reset the decoder.
 * 2. Feed the first three Vorbis header packets via write().
 * 3. Feed subsequent audio packets via write().
 * 4. Use setOutput() to set the PCM output destination.
 * 5. Call audioInfo() to retrieve stream parameters after header parsing.
 *
 * @author Phil Schatzmann
 * @ingroup codecs
 * @ingroup decoder
 * @copyright GPLv3
 */
class VorbisDecoder : public AudioDecoder {
 public:
  /**
   * @brief Constructor for VorbisDecoder
   * @param buffer_size Size of the PCM output buffer (default: 256)
   * @param header_packets Number of Vorbis header packets (default: 3)
   *
   * Initializes the decoder and allocates the PCM output buffer.
   */
  VorbisDecoder(size_t buffer_size = 256, int header_packets = 3)
      : pcm_buffer_size(buffer_size), num_header_packets(header_packets) {}

  /**
   * @brief Destructor for VorbisDecoder
   *
   * Cleans up all decoder resources.
   */
  ~VorbisDecoder() { end(); }

  /**
   * @brief Resets decoder state and prepares for new Vorbis stream
   *
   * This method clears all decoder state, resizes the PCM output buffer,
   * and initializes Vorbis structures. Call this before feeding header packets.
   * @return true if successful
   */
  bool begin() override {
    end();
    pcmout_buffer.resize(pcm_buffer_size);
    vorbis_info_init(&vi);
    vorbis_comment_init(&vc);
    active = true;
    return true;
  }

  /**
   * @brief Cleans up all Vorbis decoder structures
   */
  void end() override {
    vorbis_block_clear(&vb);
    vorbis_dsp_clear(&vd);
    vorbis_comment_clear(&vc);
    vorbis_info_clear(&vi);
    header_packets = 0;
    decoder_initialized = false;
    active = false;
  }

  /**
   * @brief Feeds a Vorbis packet (header or audio) to the decoder
   *
   * The first three packets must be Vorbis headers. Subsequent packets are
   * audio. PCM output is written to the Print stream set via setOutput().
   *
   * @param data Pointer to packet data
   * @param len Length of packet data
   * @return Number of PCM bytes written to output
   */
  size_t write(const uint8_t *data, size_t len) override {
    ogg_packet packet;
    packet.packet = (unsigned char *)data;
    packet.bytes = len;
    packet.b_o_s = (header_packets == 0) ? 1 : 0;
    packet.e_o_s = 0;
    packet.granulepos = 0;
    packet.packetno = header_packets;

    if (num_header_packets == 0 && !decoder_initialized) {
      if (!initDecoder()) return 0;
      decoder_initialized = true;
    }
    if (header_packets < num_header_packets) {
      if (!parseHeaderPacket(packet, header_packets)) return 0;
      header_packets++;
      if (header_packets == num_header_packets) {
        if (!initDecoder()) return 0;
        decoder_initialized = true;
      }
      return 0;
    }
    if (header_packets == num_header_packets) {
      notifyAudioChange(audioInfo());
    }
    if (!decoder_initialized) return 0;
    return decodeAudioPacket(packet);
  }

  /**
   * @brief Returns audio stream info (sample rate, channels, bits per sample)
   * @return AudioInfo struct with stream parameters
   */
  AudioInfo audioInfo() override {
    AudioInfo info;
    if (vi.channels > 0 && vi.rate > 0) {
      info.sample_rate = vi.rate;
      info.channels = vi.channels;
      info.bits_per_sample = 16;
    }
    return info;
  }

  /**
   * @brief Returns true if decoder is active
   */
  operator bool() override { return active; }

 protected:
  /** @brief Vorbis stream info (channels, sample rate, etc.) */
  vorbis_info vi{};
  /** @brief Vorbis comment metadata */
  vorbis_comment vc{};
  /** @brief Decoder state for synthesis */
  vorbis_dsp_state vd{};
  /** @brief Block structure for synthesis */
  vorbis_block vb{};
  /** @brief Output stream for PCM audio */
  Print *p_print = nullptr;
  /** @brief Decoder active state */
  bool active = false;
  /** @brief PCM output buffer size */
  size_t pcm_buffer_size = 256;
  /** @brief Number of Vorbis header packets */
  int num_header_packets = 3;
  /** @brief Buffer for interleaved PCM output */
  Vector<int16_t> pcmout_buffer;
  int header_packets = 0;
  bool decoder_initialized = false;
  /**
   * @brief Parses a Vorbis header packet
   * @param packet Ogg Vorbis header packet
   * @param header_packets Index of header packet (0, 1, 2)
   * @return true if successful
   */
  bool parseHeaderPacket(ogg_packet &packet, int header_packets) {
    if (vorbis_synthesis_headerin(&vi, &vc, &packet) != 0) {
      LOGE("Header packet %d invalid", header_packets);
      return false;
    }
    return true;
  }

  /**
   * @brief Initializes the Vorbis decoder after header parsing
   * @return true if successful
   */
  bool initDecoder() {
    if (vorbis_synthesis_init(&vd, &vi) != 0) {
      LOGE("vorbis_synthesis_init failed");
      return false;
    }
    vorbis_block_init(&vd, &vb);
    return true;
  }

  /**
   * @brief Decodes an audio packet and writes PCM to output
   * @param packet Ogg Vorbis audio packet
   * @return Number of PCM bytes written
   */
  size_t decodeAudioPacket(ogg_packet &packet) {
    size_t total_written = 0;
    if (vorbis_synthesis(&vb, &packet) == 0) {
      vorbis_synthesis_blockin(&vd, &vb);
      float **pcm = nullptr;
      int samples = vorbis_synthesis_pcmout(&vd, &pcm);
      while (samples > 0 && pcm) {
        int chunk = (samples > pcm_buffer_size) ? pcm_buffer_size : samples;
        convertFloatToInt16PCM(pcm, chunk, vi.channels);
        if (!pcmout_buffer.empty() && p_print) {
          p_print->write((uint8_t *)pcmout_buffer.data(),
                         pcmout_buffer.size() * sizeof(int16_t));
          total_written += pcmout_buffer.size() * sizeof(int16_t);
          pcmout_buffer.clear();
        }
        vorbis_synthesis_read(&vd, chunk);
        samples = vorbis_synthesis_pcmout(&vd, &pcm);
      }
    }
    return total_written;
  }

  /**
   * @brief Converts float PCM to interleaved int16 PCM and stores in
   * pcmout_buffer
   * @param pcm Pointer to float PCM array [channels][samples]
   * @param samples Number of samples
   * @param channels Number of channels
   */
  void convertFloatToInt16PCM(float **pcm, int samples, int channels) {
    for (int i = 0; i < samples; ++i) {
      for (int ch = 0; ch < channels; ++ch) {
        float val = pcm[ch][i];
        int16_t sample = (int16_t)(val * 32767.0f);
        if (sample > 32767) sample = 32767;
        if (sample < -32768) sample = -32768;
        pcmout_buffer.push_back(sample);
      }
    }
  }
};

}  // namespace audio_tools
