/**
 * @file CodecDSF.h
 * @brief DSF (DSD Stream File) format decoder and encoder implementation
 * @author pschatzmann
 * @copyright GPLv3
 *
 * Decoder: converts DSD audio from DSF files to PCM format.
 * Encoder: converts PCM audio to DSD and writes DSF files.
 *
 * Key features:
 * - DSF file header parsing, validation, and generation
 * - Correct block-level channel de-interleaving per DSF spec
 * - DSD-to-PCM via bit counting with cascaded Butterworth anti-aliasing
 * - PCM-to-DSD via 2nd order delta-sigma modulation with interpolation
 * - Streaming-compatible operation for real-time processing
 * - Support for mono and multi-channel DSD files (DSD64 and higher)
 */

#pragma once
#pragma GCC optimize("O3")

#include "AudioTools/AudioCodecs/AudioCodecsBase.h"
#include "AudioTools/CoreAudio/AudioFilter/Filter.h"
#include "AudioTools/CoreAudio/Buffers.h"

namespace audio_tools {

/**
 * @brief Metadata structure for DSF (DSD Stream File) format
 * @author pschatzmann
 */
struct DSFMetadata : public AudioInfo {
  DSFMetadata() = default;
  DSFMetadata(int rate) { sample_rate = rate; }
  /// DSD sample rate in Hz (e.g. 2822400 for DSD64, 5644800 for DSD128)
  uint32_t dsd_sample_rate = 0;
  /// Total size of DSD data in bytes (0 for streaming mode)
  uint64_t dsd_data_bytes = 0;
  /// Estimated number of PCM frames after DSD-to-PCM conversion
  uint64_t pcm_frames = 0;
  /// Approximate audio duration in seconds
  float duration_sec = 0;
  /// DSF block size per channel in bytes (from file header, typically 4096)
  uint32_t block_size_per_channel = 4096;
  /// Anti-aliasing filter cutoff as fraction of sample_rate (~0.45 = 90% of Nyquist at 44.1kHz)
  float filter_cutoff = 0.45f;
  /// Number of cascaded Butterworth biquad filter stages (1-3), 0 to disable filtering
  int filter_stages = 3;
  /// PCM output buffer size in bytes (must be >= one frame)
  int output_buffer_size = 1024;
  /// When true, output de-interleaved DSD bitstream instead of converting to PCM
  bool is_raw = false;
};

/// DSF file prefix containing file identification and basic information
struct __attribute__((packed)) DSDPrefix {
  char id[4];               // "DSD "
  uint64_t chunkSize;       // 28
  uint64_t fileSize;        // total file size
  uint64_t metadataOffset;  // offset to "ID3 " chunk (0 if none)
};

/// DSF format chunk containing audio format parameters
struct __attribute__((packed)) DSFFormat {
  char id[4];                    // "fmt "
  uint64_t chunkSize;            // 52
  uint32_t formatVersion;        // 1
  uint32_t formatID;             // 0
  uint32_t channelType;          // e.g., 2 for stereo
  uint32_t channelNum;           // number of channels
  uint32_t samplingFrequency;    // e.g., 2822400
  uint32_t bitsPerSample;        // 1
  uint64_t sampleCount;          // total samples per channel
  uint32_t blockSizePerChannel;  // e.g., 4096
  uint32_t reserved;             // 0
};

/// DSF data chunk header containing audio data size information
struct __attribute__((packed)) DSFDataHeader {
  char id[4];          // "data"
  uint64_t chunkSize;  // size of DSD data
};

/**
 * @brief Decodes DSF files containing DSD audio and converts to PCM.
 * @ingroup codecs
 * @author pschatzmann
 *
 * Uses per-channel buffers for correct block-level de-interleaving,
 * bit-counting decimation, and cascaded Butterworth low-pass filters for
 * anti-aliasing.
 */
class DSFDecoder : public AudioDecoder {
 public:
  DSFDecoder() = default;
  DSFDecoder(DSFMetadata metaData) { setMetaData(metaData); };

  AudioInfo audioInfo() override { return meta; }

  void setAudioInfo(AudioInfo from) override {
    TRACED();
    AudioDecoder::setAudioInfo(from);
    meta.copyFrom(from);
    if (isHeaderAvailable()) {
      int buffer_size = getOutputBufferSize();
      pcmBuffer.resize(buffer_size);

      int perChBufSize = meta.block_size_per_channel * 2;
      channelDsdBuffers.resize(meta.channels);
      for (int ch = 0; ch < meta.channels; ch++) {
        channelDsdBuffers[ch].resize(perChBufSize);
      }

      setupFilters();
      setupDecimationStep();
    }
  }

  bool begin() {
    TRACED();
    headerParsed = false;
    blockPos = 0;
    decimationStep = 64;
    isActive = true;
    return true;
  }

  void end() override { isActive = false; }

  const DSFMetadata getMetadata() { return meta; }

  void setMetaData(DSFMetadata metaData) {
    meta = metaData;
    AudioDecoder::setAudioInfo(meta);
  }

  /// Register a callback that receives the raw DSFFormat header after parsing
  void setInfoCallback(void (*callback)(const DSFFormat& fmt)) {
    infoCallback = callback;
  }

  bool isHeaderAvailable() { return headerParsed; }

  operator bool() { return isActive; }

  size_t write(const uint8_t* data, size_t len) {
    LOGD("write: %u", (unsigned)len);
    size_t i = 0;

    i += processHeader(data, len);

    if (headerParsed && i < len) {
      i += processDSDData(data, len, i);
    }

    return len;
  }

 protected:
  bool headerParsed = false;
  bool isActive = false;
  uint32_t blockPos = 0;
  void (*infoCallback)(const DSFFormat& fmt) = nullptr;

  SingleBuffer<uint8_t> pcmBuffer{0};
  Vector<SingleBuffer<uint8_t>> channelDsdBuffers;
  Vector<LowPassFilter<float>> channelFilters;
  uint32_t decimationStep;

  DSFMetadata meta;

  int getOutputBufferSize() {
    int frame_size = meta.bits_per_sample / 8 * meta.channels;
    if (meta.bits_per_sample == 24) frame_size = 4 * meta.channels;
    int buffer_size = frame_size;
    if (meta.output_buffer_size > buffer_size)
      buffer_size = meta.output_buffer_size;
    return buffer_size;
  }

  size_t processHeader(const uint8_t* data, size_t len) {
    if (headerParsed) return 0;
    LOGI("processHeader: %u", (unsigned)len);

    if (memcmp(data, "DSD ", 4) != 0) {
      LOGE("Invalid DSF header magic");
      return 0;
    }

    int dataPos = findTag("data", data, len);
    int fmtPos = findTag("fmt ", data, len);
    if (dataPos < 0 || fmtPos < 0) {
      LOGE("DSF header not found in data (fmt: %d, data: %d)", fmtPos, dataPos);
      return 0;
    }

    parseFMT(data + fmtPos, len - fmtPos);
    parseData(data + dataPos, len - dataPos);
    headerParsed = true;

    if (infoCallback) {
      infoCallback(*reinterpret_cast<const DSFFormat*>(data + fmtPos));
    }

    if (!meta.is_raw) {
      setAudioInfo(meta);
    }

    return dataPos + sizeof(DSFDataHeader);
  }

  size_t processDSDData(const uint8_t* data, size_t len, size_t startPos) {
    LOGD("processDSDData: %u (%u)", (unsigned)len, (unsigned)startPos);

    if (meta.is_raw) {
      size_t rawLen = len - startPos;
      writeBlocking(getOutput(),(uint8_t*) data + startPos, rawLen);
      return rawLen;
    }

    size_t totalProcessed = 0;
    size_t pos = startPos;

    while (pos < len) {
      size_t buffered = bufferDSDData(data, len, pos);
      if (buffered == 0) break;
      pos += buffered;
      totalProcessed += buffered;
      convertDSDToPCM();
    }

    return totalProcessed;
  }

  size_t bufferDSDData(const uint8_t* data, size_t len, size_t startPos) {
    size_t consumed = 0;
    uint32_t blockSetSize = meta.block_size_per_channel * meta.channels;

    for (size_t i = startPos; i < len; i++) {
      int ch = blockPos / meta.block_size_per_channel;
      if (ch >= meta.channels) {
        blockPos = 0;
        ch = 0;
      }
      if (channelDsdBuffers[ch].availableForWrite() <= 0) break;
      channelDsdBuffers[ch].write(data[i]);
      consumed++;
      blockPos++;
      if (blockPos >= blockSetSize) blockPos = 0;
    }
    return consumed;
  }

  void convertDSDToPCM() {
    int bytesPerDecimation = decimationStep / 8;
    int stages = getFilterStages();

    while (allChannelsHaveData(bytesPerDecimation)) {
      for (int ch = 0; ch < meta.channels; ch++) {
        int ones = 0;
        for (int b = 0; b < bytesPerDecimation; b++) {
          uint8_t dsdByte;
          channelDsdBuffers[ch].read(dsdByte);
          ones += popcount8(dsdByte);
        }

        float pcm = 2.0f * ones / decimationStep - 1.0f;

        for (int s = 0; s < stages; s++) {
          pcm = channelFilters[ch * stages + s].process(pcm);
        }

        writePCMSample(clip(pcm));
      }

      if (pcmBuffer.isFull()) {
        size_t frameSize = pcmBuffer.available();
        writeBlocking(getOutput(), (uint8_t*)pcmBuffer.data(), frameSize);
        pcmBuffer.reset();
      }
    }

    // Flush remaining PCM samples
    if (pcmBuffer.available() > 0) {
      writeBlocking(getOutput(), (uint8_t*)pcmBuffer.data(),
                    pcmBuffer.available());
      pcmBuffer.reset();
    }

    // Compact linear buffers so write space is reclaimed
    for (int ch = 0; ch < meta.channels; ch++) {
      channelDsdBuffers[ch].trim();
    }
  }

  bool allChannelsHaveData(int bytesNeeded) {
    for (int ch = 0; ch < meta.channels; ch++) {
      if (channelDsdBuffers[ch].available() < bytesNeeded) return false;
    }
    return true;
  }

  int popcount8(uint8_t v) {
    v = (v & 0x55) + ((v >> 1) & 0x55);
    v = (v & 0x33) + ((v >> 2) & 0x33);
    return (v & 0x0F) + ((v >> 4) & 0x0F);
  }

  float clip(float value) {
    if (value > 1.0f) return 1.0f;
    if (value < -1.0f) return -1.0f;
    return value;
  }

  int getFilterStages() {
    int stages = meta.filter_stages;
    if (stages < 0) stages = 0;
    if (stages > 3) stages = 3;
    return stages;
  }

  void setupFilters() {
    TRACEI();
    int stages = getFilterStages();
    if (stages == 0 || meta.sample_rate <= 0 || meta.channels <= 0) return;

    float cutoffFreq = meta.sample_rate * meta.filter_cutoff;
    channelFilters.resize(meta.channels * stages);

    // Butterworth Q values for maximally-flat passband
    static const float butterworthQ[][3] = {
        {0.7071f, 0, 0},
        {0.5412f, 1.3066f, 0},
        {0.5176f, 0.7071f, 1.9319f},
    };

    for (int ch = 0; ch < meta.channels; ch++) {
      for (int s = 0; s < stages; s++) {
        float q = butterworthQ[stages - 1][s];
        channelFilters[ch * stages + s].begin(cutoffFreq, meta.sample_rate, q);
      }
    }
  }

  void setupDecimationStep() {
    TRACEI();
    if (meta.sample_rate == 0 || meta.dsd_sample_rate == 0) {
      LOGE("Invalid sample rates: DSD=%u, PCM=%u",
           (unsigned)meta.dsd_sample_rate, (unsigned)meta.sample_rate);
      return;
    }

    decimationStep = meta.dsd_sample_rate / meta.sample_rate;
    if (decimationStep < 64) {
      LOGW("Decimation step %u too low, setting to 64",
           (unsigned)decimationStep);
      decimationStep = 64;
    }
    if (decimationStep > 512) {
      LOGW("Decimation step %u too high, setting to 512",
           (unsigned)decimationStep);
      decimationStep = 512;
    }

    decimationStep = (decimationStep / 8) * 8;
    if (decimationStep < 64) decimationStep = 64;

    LOGI("Decimation step set to %u for DSD rate %u and target PCM rate %u",
         (unsigned)decimationStep, (unsigned)meta.dsd_sample_rate,
         (unsigned)meta.sample_rate);
  }

  void writePCMSample(float filteredValue) {
    switch (meta.bits_per_sample) {
      case 8: {
        int8_t buffer8 = static_cast<int8_t>(filteredValue * 127.0f);
        pcmBuffer.write(buffer8);
        break;
      }
      case 16: {
        int16_t buffer16 = static_cast<int16_t>(filteredValue * 32767.0f);
        pcmBuffer.writeArray((uint8_t*)&buffer16, sizeof(int16_t));
        break;
      }
      case 24: {
        int24_t buffer24 = static_cast<int24_t>(filteredValue * 8388607.0f);
        pcmBuffer.writeArray((uint8_t*)&buffer24, sizeof(int24_t));
        break;
      }
      case 32: {
        int32_t buffer32 = static_cast<int32_t>(filteredValue * 2147483647.0f);
        pcmBuffer.writeArray((uint8_t*)&buffer32, sizeof(int32_t));
        break;
      }
      default:
        LOGE("Unsupported bits per sample: %d", meta.bits_per_sample);
        break;
    }
  }

  int findTag(const char* tag, const uint8_t* data, size_t len) {
    int taglen = strlen(tag);
    for (size_t j = 0; j + taglen <= len; j++) {
      if (memcmp(tag, data + j, taglen) == 0) {
        return j;
      }
    }
    return -1;
  }

  bool parseFMT(const uint8_t* data, size_t len) {
    TRACEI();
    if (len < sizeof(DSFFormat)) {
      LOGE("FMT section too short to parse DSF format header");
      return false;
    }
    DSFFormat* fmt = (DSFFormat*)data;
    meta.channels = fmt->channelNum;
    if (meta.channels == 0) meta.channels = fmt->channelType;
    meta.dsd_sample_rate = fmt->samplingFrequency;
    meta.block_size_per_channel = fmt->blockSizePerChannel;
    if (meta.block_size_per_channel == 0) meta.block_size_per_channel = 4096;

    if (meta.channels == 0 || meta.channels > 8) {
      LOGE("Invalid channel count: %u (must be 1-8)", (unsigned)meta.channels);
      return false;
    }

    LOGI("channels: %u, DSD sample rate: %u, block size: %u",
         (unsigned)meta.channels, (unsigned)meta.dsd_sample_rate,
         (unsigned)meta.block_size_per_channel);
    return true;
  }

  bool parseData(const uint8_t* data, size_t len) {
    TRACEI();
    if (len < sizeof(DSFDataHeader)) {
      LOGE("Data section too short to parse DSF data header");
      return false;
    }
    DSFDataHeader* header = (DSFDataHeader*)data;
    meta.dsd_data_bytes = header->chunkSize;

    uint64_t totalBits = meta.dsd_data_bytes * 8;
    uint64_t totalDSDSamples = totalBits / meta.channels;
    uint64_t totalPCMFrames =
        totalDSDSamples / (meta.dsd_sample_rate / meta.sample_rate);
    meta.pcm_frames = totalPCMFrames;
    meta.duration_sec = (float)totalPCMFrames / meta.sample_rate;
    return true;
  }
};

/**
 * @brief DSF (DSD Stream File) format encoder
 * @ingroup codecs
 * @author pschatzmann
 *
 * Encodes PCM audio into DSD format and writes DSF files. Uses a 2nd order
 * error-feedback delta-sigma modulator with linear interpolation between PCM
 * samples. Output uses correct DSF block-level channel interleaving.
 */
class DSFEncoder : public AudioEncoder {
 public:
  DSFEncoder() = default;
  DSFEncoder(DSFMetadata metaData) { setMetaData(metaData); }

  const char* mime() override { return "audio/dsf"; }

  void setOutput(Print& out) override { p_print = &out; }

  void setAudioInfo(AudioInfo from) override {
    AudioEncoder::setAudioInfo(from);
    meta.copyFrom(from);
  }

  void setMetaData(DSFMetadata metaData) {
    meta = metaData;
    AudioEncoder::setAudioInfo(meta);
  }

  const DSFMetadata getMetadata() { return meta; }

  bool begin() override {
    TRACED();
    if (meta.dsd_sample_rate == 0) meta.dsd_sample_rate = meta.sample_rate * 64;

    oversamplingRatio = meta.dsd_sample_rate / meta.sample_rate;
    if (oversamplingRatio < 8) oversamplingRatio = 8;

    channelBlocks.resize(meta.channels);
    for (int ch = 0; ch < meta.channels; ch++) {
      channelBlocks[ch].resize(meta.block_size_per_channel);
    }

    chState.resize(meta.channels);
    for (int ch = 0; ch < meta.channels; ch++) {
      chState[ch] = ChannelModState();
    }

    totalDsdBytes = 0;
    headerWritten = false;
    isOpen = true;
    return true;
  }

  void end() override {
    if (!isOpen) return;
    flushPartialBlocks();
    isOpen = false;
  }

  size_t write(const uint8_t* data, size_t len) override {
    if (!isOpen || p_print == nullptr) return 0;

    if (!headerWritten) {
      writeDSFHeader();
      headerWritten = true;
    }

    int bytesPerSample = meta.bits_per_sample / 8;
    if (meta.bits_per_sample == 24) bytesPerSample = 4;
    int frameSize = bytesPerSample * meta.channels;
    if (frameSize == 0) return 0;

    size_t pos = 0;
    while (pos + frameSize <= len) {
      for (int ch = 0; ch < meta.channels; ch++) {
        float sample = readPCMSample(data + pos + ch * bytesPerSample);
        convertSampleToDSD(ch, sample);
      }
      pos += frameSize;

      if (allBlocksFull()) {
        writeBlockSet();
      }
    }

    return pos;
  }

  operator bool() override { return isOpen; }

 protected:
  struct ChannelModState {
    float dsError1 = 0;
    float dsError2 = 0;
    float prevSample = 0;
    uint8_t currentByte = 0;
    int bitPos = 0;
  };

  DSFMetadata meta;
  Print* p_print = nullptr;
  Vector<SingleBuffer<uint8_t>> channelBlocks;
  Vector<ChannelModState> chState;
  uint32_t oversamplingRatio = 64;
  uint64_t totalDsdBytes = 0;
  bool headerWritten = false;
  bool isOpen = false;

  float readPCMSample(const uint8_t* ptr) {
    switch (meta.bits_per_sample) {
      case 8:
        return *reinterpret_cast<const int8_t*>(ptr) / 127.0f;
      case 16: {
        int16_t v;
        memcpy(&v, ptr, sizeof(int16_t));
        return v / 32767.0f;
      }
      case 24: {
        int24_t v;
        memcpy(&v, ptr, sizeof(int24_t));
        return static_cast<float>(static_cast<int32_t>(v)) / 8388607.0f;
      }
      case 32: {
        int32_t v;
        memcpy(&v, ptr, sizeof(int32_t));
        return v / 2147483647.0f;
      }
      default:
        return 0.0f;
    }
  }

  void convertSampleToDSD(int ch, float currentSample) {
    float prevSample = chState[ch].prevSample;

    for (uint32_t i = 0; i < oversamplingRatio; i++) {
      // Linear interpolation between previous and current PCM sample
      float t = (float)i / oversamplingRatio;
      float input = prevSample + t * (currentSample - prevSample);
      input *= 0.5f;

      // 2nd order error-feedback delta-sigma: NTF = (1 - z^-1)^2
      float shaped = input + 2.0f * chState[ch].dsError1 - chState[ch].dsError2;
      int bit = (shaped >= 0.0f) ? 1 : 0;
      float feedback = bit ? 1.0f : -1.0f;
      float quantError = shaped - feedback;

      if (quantError > 4.0f) quantError = 4.0f;
      if (quantError < -4.0f) quantError = -4.0f;

      chState[ch].dsError2 = chState[ch].dsError1;
      chState[ch].dsError1 = quantError;

      // LSB first for DSF format
      if (bit) {
        chState[ch].currentByte |= (1 << chState[ch].bitPos);
      }
      chState[ch].bitPos++;

      if (chState[ch].bitPos >= 8) {
        channelBlocks[ch].write(chState[ch].currentByte);
        chState[ch].currentByte = 0;
        chState[ch].bitPos = 0;
      }
    }

    chState[ch].prevSample = currentSample;
  }

  bool allBlocksFull() {
    for (int ch = 0; ch < meta.channels; ch++) {
      if (!channelBlocks[ch].isFull()) return false;
    }
    return true;
  }

  void writeBlockSet() {
    for (int ch = 0; ch < meta.channels; ch++) {
      writeBlocking(p_print, (uint8_t*)channelBlocks[ch].data(),
                     channelBlocks[ch].available());
      totalDsdBytes += channelBlocks[ch].available();
      channelBlocks[ch].reset();
    }
  }

  void flushPartialBlocks() {
    bool hasData = false;
    for (int ch = 0; ch < meta.channels; ch++) {
      if (channelBlocks[ch].available() > 0 || chState[ch].bitPos > 0) {
        hasData = true;
        break;
      }
    }
    if (!hasData) return;

    for (int ch = 0; ch < meta.channels; ch++) {
      if (chState[ch].bitPos > 0) {
        channelBlocks[ch].write(chState[ch].currentByte);
        chState[ch].currentByte = 0;
        chState[ch].bitPos = 0;
      }
    }

    // Pad to full block size with DSD silence (alternating bit pattern)
    for (int ch = 0; ch < meta.channels; ch++) {
      while (!channelBlocks[ch].isFull()) {
        channelBlocks[ch].write(0x69);
      }
    }
    writeBlockSet();
  }

  void writeDSFHeader() {
    uint64_t dsdDataBytes = meta.dsd_data_bytes;
    bool streaming = (dsdDataBytes == 0);
    uint64_t headerTotal =
        sizeof(DSDPrefix) + sizeof(DSFFormat) + sizeof(DSFDataHeader);

    DSDPrefix prefix;
    memcpy(prefix.id, "DSD ", 4);
    prefix.chunkSize = sizeof(DSDPrefix);
    prefix.fileSize = streaming ? 0 : (headerTotal + dsdDataBytes);
    prefix.metadataOffset = 0;

    DSFFormat fmt;
    memcpy(fmt.id, "fmt ", 4);
    fmt.chunkSize = sizeof(DSFFormat);
    fmt.formatVersion = 1;
    fmt.formatID = 0;
    fmt.channelType = meta.channels;
    fmt.channelNum = meta.channels;
    fmt.samplingFrequency = meta.dsd_sample_rate;
    fmt.bitsPerSample = 1;
    fmt.sampleCount = streaming ? 0 : (dsdDataBytes * 8 / meta.channels);
    fmt.blockSizePerChannel = meta.block_size_per_channel;
    fmt.reserved = 0;

    DSFDataHeader dataHdr;
    memcpy(dataHdr.id, "data", 4);
    dataHdr.chunkSize = streaming ? 0 : (sizeof(DSFDataHeader) + dsdDataBytes);

    p_print->write((uint8_t*)&prefix, sizeof(prefix));
    p_print->write((uint8_t*)&fmt, sizeof(fmt));
    p_print->write((uint8_t*)&dataHdr, sizeof(dataHdr));
  }
};

}  // namespace audio_tools
