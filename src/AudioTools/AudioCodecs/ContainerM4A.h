#pragma once

#include "AudioTools/AudioCodecs/AudioCodecsBase.h"
#include "AudioTools/AudioCodecs/MultiDecoder.h"

namespace audio_tools {

/**
 * @brief M4A Demuxer that extracts audio from M4A/MP4 containers.
 * If you provide a decoder (in the constructor) the audio is decoded into pcm
 * format.
 * @ingroup codecs
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class ContainerM4A : public ContainerDecoder {
 public:
  /// Supported codec types
  enum class M4ACodecType { UNKNOWN, AAC, ALAC };

  ContainerM4A() = default;

  ContainerM4A(MultiDecoder& decoder) {
    p_decoder = &decoder;
    p_decoder->addNotifyAudioChange(*this);
  }

  void setOutput(AudioStream& out_stream) override {
    if (p_decoder != nullptr) p_decoder->setOutput(out_stream);
    ContainerDecoder::setOutput(out_stream);
  }

  bool isResultPCM() override { return p_decoder != nullptr ? true : false; }

  /// Get the detected codec type
  M4ACodecType getM4ACodecType() const { return result_codec_type; }

  /// Get the codec MIME type of the compressed output mode
  const char* mime() {
    switch (result_codec_type) {
      case M4ACodecType::AAC:
        return "audio/aac";
      case M4ACodecType::ALAC:
        return "audio/alac";
      default:
        return "audio/unknown";
    }
  }

  /// Get raw codec configuration data (for AAC or ALAC)
  const uint8_t* getCodecConfig(size_t& size) const {
    if (result_codec_type == M4ACodecType::AAC && has_audio_config) {
      size = sizeof(audio_config);
      return audio_config;
    } else if (result_codec_type == M4ACodecType::ALAC && is_alac) {
      size = alac_config_size;
      return alac_config;
    }
    size = 0;
    return nullptr;
  }

  /// Defines the buffer size (default 8192)
  void setBufferSize(int size) {
    buffer_size = size;
    if (buffer.size() != 0) buffer.resize(size);
  }

  bool begin() override {
    TRACED();
    is_active = true;
    state = ParserState::WAITING_FOR_FTYP;
    audio_track_id = -1;
    sample_count = 0;
    current_sample = 0;
    current_chunk = 0;
    has_audio_config = false;
    is_alac = false;
    result_codec_type = M4ACodecType::UNKNOWN;
    notified_config = false;

    // Initialize sample tables
    stts_entries.clear();
    stsc_entries.clear();
    stsz_entries.clear();
    stco_entries.clear();
    default_sample_size = 0;

    // Reset box hierarchy tracking
    box_stack.clear();

    buffer.resize(buffer_size);

    if (p_decoder) p_decoder->begin();

    return true;
  }

  void end() override {
    TRACED();
    is_active = false;
    if (p_decoder) p_decoder->end();
  }

  size_t write(const uint8_t* data, size_t len) override {
    if (!is_active || p_print == nullptr) {
      return 0;
    }

    size_t processed = 0;

    // Process input data byte by byte
    for (size_t i = 0; i < len; i++) {
      uint8_t byte = data[i];
      processed++;

      switch (state) {
        case ParserState::WAITING_FOR_FTYP:
          processFtyp(byte);
          break;

        case ParserState::READING_BOX_HEADER:
          processBoxHeader(byte);
          break;

        case ParserState::READING_BOX_DATA:
          processBoxData(byte);
          break;

        case ParserState::READING_MDAT:
          processMdat(byte);
          break;
      }
    }

    return processed;
  }

  operator bool() override { return is_active; }

 protected:
  // optional decoder
  MultiDecoder* p_decoder = nullptr;

  enum class ParserState {
    WAITING_FOR_FTYP,
    READING_BOX_HEADER,
    READING_BOX_DATA,
    READING_MDAT
  };

  struct BoxHeader {
    uint32_t size;
    char type[5];  // 4 chars + null terminator
    uint32_t bytes_read;
    uint64_t absolute_offset;  // To track offsets for co64 support
  };

  // Sample table structures
  struct SttsEntry {
    uint32_t sample_count;
    uint32_t sample_delta;
  };

  struct StscEntry {
    uint32_t first_chunk;
    uint32_t samples_per_chunk;
    uint32_t sample_description_index;
  };

  struct StszEntry {
    uint32_t sample_size;
  };

  struct StcoEntry {
    uint64_t chunk_offset;  // Using uint64_t to support co64 boxes
  };

  ParserState state = ParserState::WAITING_FOR_FTYP;
  bool is_active = false;

  BoxHeader current_box;
  Vector<BoxHeader> box_stack;  // Track box hierarchy

  int audio_track_id = -1;
  uint32_t sample_count = 0;
  uint32_t current_sample = 0;
  uint32_t current_chunk = 0;
  uint64_t mdat_start_offset = 0;
  uint64_t file_offset = 0;  // Track current file position

  // Sample tables
  Vector<SttsEntry> stts_entries;
  Vector<StscEntry> stsc_entries;
  Vector<StszEntry> stsz_entries;
  Vector<StcoEntry> stco_entries;
  uint32_t default_sample_size = 0;  // For stsz when all samples have same size

  // Buffer for collecting box data
  Vector<uint8_t> buffer;
  size_t buffer_size = 8192;  // Default size
  size_t buffer_pos = 0;

  // Codec-specific
  M4ACodecType result_codec_type = M4ACodecType::UNKNOWN;
  bool notified_config = false;

  // AAC specific
  bool has_audio_config = false;
  uint8_t audio_config[2];  // AudioSpecificConfig (2 bytes for AAC)

  // ALAC specific
  bool is_alac = false;
  uint8_t alac_config[36];  // ALAC config is typically 36 bytes
  size_t alac_config_size = 0;

  // Track the current sample's data
  uint32_t current_sample_size = 0;
  uint64_t current_sample_offset = 0;

  // Box type identifiers - expanded to include all needed boxes
  const char* BOX_FTYP = "ftyp";
  const char* BOX_MOOV = "moov";
  const char* BOX_MDAT = "mdat";
  const char* BOX_TRAK = "trak";
  const char* BOX_MDIA = "mdia";
  const char* BOX_MINF = "minf";
  const char* BOX_STBL = "stbl";
  const char* BOX_STSD = "stsd";
  const char* BOX_MP4A = "mp4a";
  const char* BOX_ESDS = "esds";
  const char* BOX_STTS = "stts";
  const char* BOX_STSC = "stsc";
  const char* BOX_STSZ = "stsz";
  const char* BOX_STCO = "stco";
  const char* BOX_CO64 = "co64";
  const char* BOX_HDLR = "hdlr";
  const char* BOX_ALAC = "alac";

  void processFtyp(uint8_t byte) {
    static uint8_t ftyp_signature[] = {'f', 't', 'y', 'p'};
    static int signature_pos = 0;

    // Skip first 4 bytes (box size)
    static int header_pos = 0;
    if (header_pos < 4) {
      header_pos++;
      file_offset++;
      return;
    }

    // Check for ftyp signature
    if (byte == ftyp_signature[signature_pos]) {
      signature_pos++;
      file_offset++;
      if (signature_pos == 4) {
        // Found ftyp, move to general box parsing
        resetBoxHeader();
        state = ParserState::READING_BOX_HEADER;
        signature_pos = 0;
        header_pos = 0;
      }
    } else {
      // Reset if signature doesn't match
      signature_pos = 0;
      file_offset++;
    }
  }

  void resetBoxHeader() {
    current_box.size = 0;
    memset(current_box.type, 0, 5);
    current_box.bytes_read = 0;
    current_box.absolute_offset = file_offset;
  }

  void processBoxHeader(uint8_t byte) {
    static int header_pos = 0;

    if (header_pos < 4) {
      // Reading size (big endian)
      current_box.size = (current_box.size << 8) | byte;
    } else if (header_pos < 8) {
      // Reading box type (4 characters)
      current_box.type[header_pos - 4] = byte;
    }

    header_pos++;
    file_offset++;

    if (header_pos == 8) {
      // Header complete
      header_pos = 0;
      LOGI("Found box: %s, size: %u, offset: %llu", current_box.type,
           current_box.size, current_box.absolute_offset);

      // Handle special box sizes
      if (current_box.size == 1) {
        // Extended size in next 8 bytes (64-bit) - not handling for simplicity
        LOGW("64-bit box size not fully supported");
      } else if (current_box.size == 0) {
        // Box extends to end of file - not handling for simplicity
        LOGW("Box extends to end of file - not fully supported");
      }

      // Process based on box type
      if (strcmp(current_box.type, BOX_MDAT) == 0) {
        mdat_start_offset = file_offset;
        state = ParserState::READING_MDAT;
        current_box.bytes_read = 8;  // Account for the header we just read
      } else {
        // Check if this is a container box
        bool is_container = isContainerBox(current_box.type);

        if (is_container) {
          // Push current box to stack and continue reading boxes inside it
          box_stack.push_back(current_box);
          state = ParserState::READING_BOX_HEADER;
          resetBoxHeader();
        } else {
          // Standard data box
          state = ParserState::READING_BOX_DATA;
          buffer_pos = 0;
          current_box.bytes_read = 8;  // Account for the header we just read
        }
      }
    }
  }

  bool isContainerBox(const char* type) {
    // List of container boxes that contain other boxes
    return (strcmp(type, BOX_MOOV) == 0 || strcmp(type, BOX_TRAK) == 0 ||
            strcmp(type, BOX_MDIA) == 0 || strcmp(type, BOX_MINF) == 0 ||
            strcmp(type, BOX_STBL) == 0);
  }

  void processBoxData(uint8_t byte) {
    // Add byte to buffer if space available
    if (buffer_pos < buffer_size) {
      buffer[buffer_pos++] = byte;
    }

    current_box.bytes_read++;
    file_offset++;

    // If we've read the entire box
    if (current_box.bytes_read >= current_box.size) {
      // Process the completed box
      processCompletedBox();

      // Check if we need to pop from box stack
      if (!box_stack.empty()) {
        BoxHeader& parent = box_stack.back();
        parent.bytes_read += current_box.size;

        // If parent is complete, process it
        if (parent.bytes_read >= parent.size) {
          processCompletedParentBox(parent);
          box_stack.pop_back();

          // Keep popping if higher level boxes are also complete
          while (!box_stack.empty()) {
            BoxHeader& grandparent = box_stack.back();
            grandparent.bytes_read += parent.size;

            if (grandparent.bytes_read >= grandparent.size) {
              processCompletedParentBox(grandparent);
              box_stack.pop_back();
              parent = grandparent;
            } else {
              break;
            }
          }
        }
      }

      // Move back to reading box headers
      state = ParserState::READING_BOX_HEADER;
      resetBoxHeader();
    }
  }

  void processCompletedBox() {
    // Process different box types
    if (strcmp(current_box.type, BOX_ESDS) == 0) {
      parseEsdsBox();
    } else if (strcmp(current_box.type, BOX_ALAC) == 0) {
      parseAlacBox();
    } else if (strcmp(current_box.type, BOX_STSD) == 0) {
      parseStsdBox();
    } else if (strcmp(current_box.type, BOX_HDLR) == 0) {
      parseHdlrBox();
    } else if (strcmp(current_box.type, BOX_STTS) == 0) {
      parseSttsBox();
    } else if (strcmp(current_box.type, BOX_STSC) == 0) {
      parseStscBox();
    } else if (strcmp(current_box.type, BOX_STSZ) == 0) {
      parseStszBox();
    } else if (strcmp(current_box.type, BOX_STCO) == 0) {
      parseStcoBox();
    } else if (strcmp(current_box.type, BOX_CO64) == 0) {
      parseCo64Box();
    }
  }

  void processCompletedParentBox(BoxHeader& box) {
    // Process container boxes after all children are processed
    if (strcmp(box.type, BOX_STBL) == 0) {
      finalizeTrackInfo();
    }
  }

  // Parse AudioSpecificConfig from esds box
  void parseEsdsBox() {
    if (buffer_pos < 31) {
      LOGW("ESDS box too small");
      return;
    }

    // The structure of the ESDS box is complex with nested descriptors
    // For simplicity, we assume a fixed structure to find the
    // AudioSpecificConfig

    // Search for the config
    for (size_t i = 0; i < buffer_pos - 2; i++) {
      // Look for the audio config pattern: typical descriptor structure
      if (buffer[i] == 0x05 && buffer[i + 1] == 0x02) {
        // Found AudioSpecificConfig (ESDescriptor with tag 0x05)
        audio_config[0] = buffer[i + 2];
        audio_config[1] = buffer[i + 3];
        has_audio_config = true;
        result_codec_type = M4ACodecType::AAC;

        LOGI("Found AAC config: %02X %02X", audio_config[0], audio_config[1]);
        break;
      }
    }
  }

  // Parse ALAC configuration
  void parseAlacBox() {
    if (buffer_pos < 36) {
      LOGW("ALAC box too small");
      return;
    }

    // Store the ALAC configuration data
    // Skip first 8 bytes (version and flags)
    alac_config_size = min(buffer_pos - 8, sizeof(alac_config));
    memcpy(alac_config, buffer.data() + 8, alac_config_size);
    is_alac = true;
    result_codec_type = M4ACodecType::ALAC;

    LOGI("Found ALAC config, size: %d", alac_config_size);

    // Extract basic audio parameters from config
    uint32_t frameLength = readUint32(alac_config, 0);
    uint8_t bitDepth = alac_config[9];
    uint8_t channels = alac_config[13];
    uint32_t sampleRate = readUint32(alac_config, 20);

    // Update audio info
    info.bits_per_sample = bitDepth;
    info.channels = channels;
    info.sample_rate = sampleRate;

    LOGI("ALAC: %d Hz, %d bit, %d ch", info.sample_rate, info.bits_per_sample,
         info.channels);

    // Notify decoder about the codec info if available
    if (p_decoder){
      if (p_decoder->selectDecoder(mime())){
        p_decoder->writeCodecInfo(alac_config, alac_config_size);
      }
    }
  }

  // Handler to identify if a track is audio
  void parseHdlrBox() {
    if (buffer_pos < 12) {
      LOGW("Handler box too small");
      return;
    }

    // Skip 8 bytes (version, flags, and pre_defined)
    // Handler type starts at offset 8
    char handler_type[5] = {0};
    memcpy(handler_type, buffer.data() + 8, 4);

    // If handler is 'soun', this is an audio track
    if (strcmp(handler_type, "soun") == 0) {
      // Get track ID from trak box - simplified in this version
      audio_track_id = 1;  // In a real implementation, get this from tkhd box
      LOGI("Found audio track, ID: %d", audio_track_id);
    }
  }

  // Parse Sample Description Box to detect codec type
  void parseStsdBox() {
    if (buffer_pos < 16) {
      LOGW("STSD box too small");
      return;
    }

    uint32_t version_flags = readUint32(buffer.data(), 0);
    uint32_t entry_count = readUint32(buffer.data(), 4);

    if (entry_count == 0 || buffer_pos < 16 + 8) {
      LOGW("No entries in STSD box");
      return;
    }

    // Each entry starts with size (4 bytes) and type (4 bytes)
    uint32_t entry_size = readUint32(buffer.data(), 8);
    char entry_type[5] = {0};
    memcpy(entry_type, buffer.data() + 12, 4);

    LOGI("STSD entry: %s, size: %d", entry_type, entry_size);

    // Check codec type
    if (strcmp(entry_type, "mp4a") == 0) {
      result_codec_type = M4ACodecType::AAC;
      LOGI("Found AAC audio");
    } else if (strcmp(entry_type, "alac") == 0) {
      result_codec_type = M4ACodecType::ALAC;
      LOGI("Found ALAC audio");
    } else {
      result_codec_type = M4ACodecType::UNKNOWN;
      LOGW("Unknown audio codec: %s", entry_type);
    }
  }

  // Parse time-to-sample box
  void parseSttsBox() {
    if (buffer_pos < 8) {
      LOGW("STTS box too small");
      return;
    }

    uint32_t version_flags = readUint32(buffer.data(), 0);
    uint32_t entry_count = readUint32(buffer.data(), 4);

    if (buffer_pos < 8 + (entry_count * 8)) {
      LOGW("STTS box incomplete");
      return;
    }

    LOGI("STTS: %d entries", entry_count);
    stts_entries.clear();

    for (uint32_t i = 0; i < entry_count; i++) {
      SttsEntry entry;
      entry.sample_count = readUint32(buffer.data(), 8 + (i * 8));
      entry.sample_delta = readUint32(buffer.data(), 8 + (i * 8) + 4);
      stts_entries.push_back(entry);
    }
  }

  // Parse sample-to-chunk box
  void parseStscBox() {
    if (buffer_pos < 8) {
      LOGW("STSC box too small");
      return;
    }

    uint32_t version_flags = readUint32(buffer.data(), 0);
    uint32_t entry_count = readUint32(buffer.data(), 4);

    if (buffer_pos < 8 + (entry_count * 12)) {
      LOGW("STSC box incomplete");
      return;
    }

    LOGI("STSC: %d entries", entry_count);
    stsc_entries.clear();

    for (uint32_t i = 0; i < entry_count; i++) {
      StscEntry entry;
      entry.first_chunk = readUint32(buffer.data(), 8 + (i * 12));
      entry.samples_per_chunk = readUint32(buffer.data(), 8 + (i * 12) + 4);
      entry.sample_description_index =
          readUint32(buffer.data(), 8 + (i * 12) + 8);
      stsc_entries.push_back(entry);
    }
  }

  // Parse sample size box
  void parseStszBox() {
    if (buffer_pos < 12) {
      LOGW("STSZ box too small");
      return;
    }

    uint32_t version_flags = readUint32(buffer.data(), 0);
    default_sample_size = readUint32(buffer.data(), 4);
    sample_count = readUint32(buffer.data(), 8);

    LOGI("STSZ: count=%d, default_size=%d", sample_count, default_sample_size);

    stsz_entries.clear();

    // If default_sample_size is 0, each sample has its own size
    if (default_sample_size == 0) {
      if (buffer_pos < 12 + (sample_count * 4)) {
        LOGW("STSZ box incomplete");
        return;
      }

      for (uint32_t i = 0; i < sample_count; i++) {
        StszEntry entry;
        entry.sample_size = readUint32(buffer.data(), 12 + (i * 4));
        stsz_entries.push_back(entry);
      }
    } else {
      // All samples have the same size
      for (uint32_t i = 0; i < sample_count; i++) {
        StszEntry entry;
        entry.sample_size = default_sample_size;
        stsz_entries.push_back(entry);
      }
    }
  }

  // Parse chunk offset box
  void parseStcoBox() {
    if (buffer_pos < 8) {
      LOGW("STCO box too small");
      return;
    }

    uint32_t version_flags = readUint32(buffer.data(), 0);
    uint32_t entry_count = readUint32(buffer.data(), 4);

    if (buffer_pos < 8 + (entry_count * 4)) {
      LOGW("STCO box incomplete");
      return;
    }

    LOGI("STCO: %d entries", entry_count);
    stco_entries.clear();

    for (uint32_t i = 0; i < entry_count; i++) {
      StcoEntry entry;
      entry.chunk_offset = readUint32(buffer.data(), 8 + (i * 4));
      stco_entries.push_back(entry);
    }
  }

  // Parse 64-bit chunk offset box
  void parseCo64Box() {
    if (buffer_pos < 8) {
      LOGW("CO64 box too small");
      return;
    }

    uint32_t version_flags = readUint32(buffer.data(), 0);
    uint32_t entry_count = readUint32(buffer.data(), 4);

    if (buffer_pos < 8 + (entry_count * 8)) {
      LOGW("CO64 box incomplete");
      return;
    }

    LOGI("CO64: %d entries", entry_count);
    stco_entries.clear();

    for (uint32_t i = 0; i < entry_count; i++) {
      StcoEntry entry;
      entry.chunk_offset = readUint64(buffer.data(), 8 + (i * 8));
      stco_entries.push_back(entry);
    }
  }

  // Called after all sample tables are processed
  void finalizeTrackInfo() {
    if (sample_count > 0 && !stco_entries.empty()) {
      LOGI("Track info complete: %d samples, %d chunks", sample_count,
           stco_entries.size());

      // Initialize for sample extraction
      current_sample = 0;
      current_chunk = 0;

      // Calculate first sample position
      calculateNextSamplePosition();
    }
  }

  void processMdat(uint8_t byte) {
    // First check if we have valid sample tables and still have samples to
    // process
    if (current_sample >= sample_count || stsz_entries.empty() ||
        stco_entries.empty()) {
      // Skip MDAT if we don't have valid sample info
      current_box.bytes_read++;
      file_offset++;

      if (current_box.bytes_read >= current_box.size) {
        state = ParserState::READING_BOX_HEADER;
        resetBoxHeader();
      }
      return;
    }

    // Check if we've reached the offset for the current sample
    if (file_offset >= current_sample_offset) {
      // Add byte to buffer
      if (buffer_pos < buffer_size) {
        buffer[buffer_pos++] = byte;
      }

      // Check if we've collected the entire sample
      if (buffer_pos >= current_sample_size) {
        // Process based on output mode and codec type
        // Output compressed format
        switch (result_codec_type) {
          case M4ACodecType::AAC:
            // Output AAC frame with ADTS header if needed
            if (has_audio_config) {
              if (!notified_config) {
                notified_config = true;
                LOGI("Outputting AAC with ADTS headers");
              }
              uint8_t adts_header[7];
              createADTSHeader(adts_header, buffer_pos);
              writeResult(adts_header, 7);
            }
            writeResult(buffer.data(), buffer_pos);
            break;

          case M4ACodecType::ALAC:
            // Output ALAC frame
            if (!notified_config) {
              notified_config = true;
              LOGI("Outputting raw ALAC frames");
            }
            writeResult(buffer.data(), buffer_pos);
            break;

          default:
            // Unknown codec, output raw data
            LOGW("Unknown codec type, outputting raw data");
            writeResult(buffer.data(), buffer_pos);
            break;
        }
      }

      // Move to next sample
      current_sample++;
      buffer_pos = 0;

      if (current_sample < sample_count) {
        calculateNextSamplePosition();
      }
    }

    // Increment counters
    current_box.bytes_read++;
    file_offset++;

    // Check if we've read the entire MDAT box
    if (current_box.bytes_read >= current_box.size) {
      state = ParserState::READING_BOX_HEADER;
      resetBoxHeader();
    }
  }

  // write the result to the decoder or to the final output
  size_t writeResult(uint8_t* data, size_t len) {
    // if we have a decoder we only decode the supported type
    const char* original_mime = p_decoder->selectedMime();
    if (p_decoder != nullptr) {
      if (!p_decoder->selectDecoder(mime())) {
        LOGE("Unsupported mime type %s", mime());
        return 0;
      }
      size_t result = p_decoder->write(data, len);
      // restore original decoder
      p_decoder->selectDecoder(original_mime);
      return result;
    }
    // No decoder, just return any result
    return p_print->write(data, len);
  }

  // Calculate position and size of the next sample to extract
  void calculateNextSamplePosition() {
    // Get sample size
    current_sample_size = stsz_entries[current_sample].sample_size;

    // Find which chunk this sample belongs to
    uint32_t chunk_idx = 0;
    uint32_t samples_in_chunks_so_far = 0;
    uint32_t samples_per_chunk = 0;

    for (size_t i = 0; i < stsc_entries.size(); i++) {
      StscEntry& entry = stsc_entries[i];
      uint32_t next_first_chunk = (i < stsc_entries.size() - 1)
                                      ? stsc_entries[i + 1].first_chunk
                                      : stco_entries.size() + 1;

      // Calculate number of chunks with this samples_per_chunk value
      uint32_t chunk_count = next_first_chunk - entry.first_chunk;

      // Check if our sample falls in this range
      uint32_t samples_in_range = chunk_count * entry.samples_per_chunk;
      if (current_sample < samples_in_chunks_so_far + samples_in_range) {
        // Found the right entry
        samples_per_chunk = entry.samples_per_chunk;

        // Calculate which chunk within this range
        uint32_t relative_chunk =
            (current_sample - samples_in_chunks_so_far) / samples_per_chunk;
        chunk_idx = entry.first_chunk - 1 +
                    relative_chunk;  // Adjust for 1-based indexing
        break;
      }

      samples_in_chunks_so_far += samples_in_range;
    }

    if (chunk_idx >= stco_entries.size()) {
      LOGE("Invalid chunk index %d (max %d)", chunk_idx,
           stco_entries.size() - 1);
      return;
    }

    // Get chunk offset
    uint64_t chunk_offset = stco_entries[chunk_idx].chunk_offset;

    // Calculate sample offset within chunk
    uint32_t sample_in_chunk =
        (current_sample - samples_in_chunks_so_far) % samples_per_chunk;

    // Sum sizes of preceding samples in this chunk
    uint64_t offset_in_chunk = 0;
    for (uint32_t i = 0; i < sample_in_chunk; i++) {
      uint32_t preceding_sample = current_sample - sample_in_chunk + i;
      offset_in_chunk += stsz_entries[preceding_sample].sample_size;
    }

    // Final sample offset
    current_sample_offset = chunk_offset + offset_in_chunk;
  }

  void createADTSHeader(uint8_t* header, size_t aac_frame_size) {
    // Extract values from AudioSpecificConfig
    uint8_t profile = (audio_config[0] >> 3) & 0x1F;  // 5 bits
    uint8_t sampling_freq_index = ((audio_config[0] & 0x7) << 1) |
                                  ((audio_config[1] >> 7) & 0x1);  // 4 bits
    uint8_t channel_config = (audio_config[1] >> 3) & 0xF;         // 4 bits

    // Adjust profile value for ADTS (profile in AudioSpecificConfig is
    // profile-1)
    profile = profile + 1;

    // Create ADTS header (7 bytes)
    header[0] = 0xFF;  // Sync word
    header[1] = 0xF1;  // MPEG-4, Layer, Protection absent
    header[2] = ((profile - 1) << 6) | (sampling_freq_index << 2) |
                (channel_config >> 2);
    header[3] = ((channel_config & 0x3) << 6) | ((aac_frame_size + 7) >> 11);
    header[4] = ((aac_frame_size + 7) >> 3) & 0xFF;
    header[5] = (((aac_frame_size + 7) & 0x7) << 5) | 0x1F;
    header[6] = 0xFC;
  }

  // Helper methods for reading integers from buffer
  uint32_t readUint32(uint8_t* buffer, size_t offset) {
    return (buffer[offset] << 24) | (buffer[offset + 1] << 16) |
           (buffer[offset + 2] << 8) | buffer[offset + 3];
  }

  uint64_t readUint64(uint8_t* buffer, size_t offset) {
    uint64_t high = readUint32(buffer, offset);
    uint64_t low = readUint32(buffer, offset + 4);
    return (high << 32) | low;
  }
};

}  // namespace audio_tools