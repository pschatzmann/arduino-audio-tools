#pragma once

#include <Arduino.h>
#include <SD.h>

#include "M4AAudioDemuxer.h"

namespace audio_tools {

/**
 * @brief Demuxer for M4A/MP4 files to extract audio data using an Arduino File.
 * This class locates the mdat and stsz boxes using MP4Parser.
 *
 * It provides a copy() method to extract frames from the file by reading
 * sample sizes directly from the stsz box in the file. This class is quite
 * memory efficient because no table of sample sizes are kept in memory. It just
 * reads the sample sizes from the stsz box and uses the mdat offset to read the
 * sample data directly from the file.
 *
 * The result will either be provided via the frame_callback or
 * can be processed by the user.
 *
 * @author Phil Schatzmann
 */
class M4AAudioFileDemuxer : public M4ACommonDemuxer {
 public:
  using M4ACommonDemuxer::Frame;
  using M4ACommonDemuxer::FrameCallback;

  /**
   * @brief Default constructor. Sets up parser callbacks.
   */
  M4AAudioFileDemuxer() { setupParser(); };

  /**
   * @brief Constructor with decoder.
   * @param decoder Reference to MultiDecoder.
   */
  M4AAudioFileDemuxer(MultiDecoder& decoder) : M4AAudioFileDemuxer() {
    setDecoder(decoder);
  }

  /**
   * @brief Sets the decoder to use for audio frames.
   * @param decoder Reference to MultiDecoder.
   * @return true if set successfully.
   */
  bool setDecoder(MultiDecoder& decoder) {
    this->p_decoder = &decoder;
    if (decoder.getOutput() == nullptr) {
      LOGE("No output defined for MultiDecoder");
      return false;
    }
    setCallback([&decoder](const Frame& frame, void* /*ref*/) {
      LOGI("Decoding frame: %s with %d bytes", frame.mime, (int)frame.size);
      if (!decoder.selectDecoder(frame.mime)){
        LOGE("Failed to select decoder for %s", frame.mime);
        return;
      }
      decoder.write(frame.data, frame.size);
    });
    return true;
  }

  /**
   * @brief Sets the callback for extracted audio frames.
   * @param cb Frame callback function.
   */
  void setCallback(FrameCallback cb) override { frame_callback = cb; }

  /**
   * @brief Sets the size of the samples buffer (in bytes).
   * @param size Buffer size in bytes.
   */
  void setSamplesBufferSize(int size) {
    stsz_bufsize = size / 4;
    stsz_buf.resize(stsz_bufsize);
  }

  /**
   * @brief Open and parse the given file.
   * @param file Reference to an open Arduino File object.
   * @return true on success, false on failure.
   */
  bool begin(File& file) {
    this->file = &file;
    parser.begin();
    if (!file) return false;
    end();
    if (p_decoder) p_decoder->begin();
    if (!parseFile(file)) return false;
    if (!readStszHeader()) return false;
    if (!checkMdat()) return false;
    mdat_sample_pos = mdat_offset + mdat_pos;

    return true;
  }

  /**
   * @brief End demuxing and reset state.
   */
  void end() {
    codec = M4ACommonDemuxer::Codec::Unknown;
    alacMagicCookie.clear();
    // resize(default_size);
    sample_index = 0;
    mdat_pos = 0;
    stsd_processed = false;
    mdat_offset = 0;
    mdat_size = 0;
    stsz_offset = 0;
    stsz_size = 0;
    sample_index = 0;
    mdat_pos = 0;
    sample_count = 0;
    fixed_sample_size = 0;
  }

  /**
   * @brief Copies the next audio frame from the file using the sample size
   * table and mdat offset. Calls the frame callback if set.
   * @return true if a frame was copied and callback called, false if end of
   * samples or error.
   */
  bool copy() {
    if (!file || sample_index >= sample_count) return false;
    size_t currentSize = getNextSampleSize();
    if (currentSize == 0) return false;
    if (!file->seek(mdat_sample_pos)) return false;
    if (buffer.size() < currentSize) buffer.resize(currentSize);
    size_t bytesRead = file->read(buffer.data(), currentSize);
    if (bytesRead != currentSize) return false;
    buffer.setWritePos(bytesRead);
    executeCallback(currentSize, buffer);
    mdat_sample_pos += currentSize;
    return true;
  }

 protected:
  File* file = nullptr;          ///< Pointer to the open file
  uint64_t mdat_offset = 0;      ///< Offset of mdat box payload
  uint64_t mdat_size = 0;        ///< Size of mdat box payload
  uint64_t stsz_offset = 0;      ///< Offset of stsz box
  uint64_t stsz_size = 0;        ///< Size of stsz box
  size_t sample_index = 0;       ///< Current sample index
  uint64_t mdat_pos = 0;         ///< Current position in mdat box
  SingleBuffer<uint8_t> buffer;  ///< Buffer for sample data
  int stsz_bufsize = 256;        ///< Number of sample sizes to buffer
  SingleBuffer<uint32_t> stsz_buf{
      stsz_bufsize};                  ///< Buffer for stsz sample sizes
  uint32_t sample_count = 0;          ///< Number of samples in stsz
  uint32_t fixed_sample_size = 0;     ///< Fixed sample size (if nonzero)
  bool stsd_processed = false;        ///< True if stsd box processed
  MultiDecoder* p_decoder = nullptr;  ///< Pointer to decoder
uint64_t mdat_sample_pos = 0;
  /**
   * @brief Sets up the MP4 parser and registers box callbacks.
   */
  void setupParser() {
    parser.setReference(this);

    // Callback for ESDS box (AAC config)
    parser.setCallback(
        "esds",
        [](MP4Parser::Box& box, void* ref) {
          static_cast<M4AAudioFileDemuxer*>(ref)->onEsds(box);
        },
        false);

    // Callback for MP4A box (AAC sample entry)
    parser.setCallback(
        "mp4a",
        [](MP4Parser::Box& box, void* ref) {
          static_cast<M4AAudioFileDemuxer*>(ref)->onMp4a(box);
        },
        false);

    // Callback for ALAC box (ALAC sample entry)
    parser.setCallback(
        "alac",
        [](MP4Parser::Box& box, void* ref) {
          static_cast<M4AAudioFileDemuxer*>(ref)->onAlac(box);
        },
        false);

    // Callback for STSZ box (sample sizes)
    parser.setCallback(
        "stsz",
        [](MP4Parser::Box& box, void* ref) {
          auto* self = static_cast<M4AAudioFileDemuxer*>(ref);
          self->stsz_offset = box.file_offset;
          self->stsz_size = box.size;
        },
        false);

    // Callback for MDAT box (media data)
    parser.setCallback(
        "mdat",
        [](MP4Parser::Box& box, void* ref) {
          auto* self = static_cast<M4AAudioFileDemuxer*>(ref);
          self->mdat_offset = box.file_offset + 8;  // skip box header
          self->mdat_size = box.size;
        },
        false);

    // Callback for STSD box (sample description)
    parser.setCallback(
        "stsd",
        [](MP4Parser::Box& box, void* ref) {
          auto* self = static_cast<M4AAudioFileDemuxer*>(ref);
          self->onStsd(box);  // for aac and alac
          self->stsd_processed = true;
        },
        false);
  }

  /**
   * @brief Parses the file and feeds data to the parser.
   * @param file Reference to the file to parse.
   */
  bool parseFile(File& file) {
    uint8_t buffer[1024];
    file.seek(0);
    while (file.available()) {
      int to_read = min(sizeof(buffer), parser.availableForWrite());
      size_t len = file.read(buffer, to_read);
      parser.write(buffer, len);
      // stop if we have all the data
      if (stsd_processed && mdat_offset && stsz_offset) return true;
    }
    return false;
  }

  /**
   * @brief Executes the callback for a completed frame.
   * @param size Size of the frame.
   * @param buffer Buffer containing the frame data.
   */
  void executeCallback(size_t size, SingleBuffer<uint8_t>& buffer) {
    Frame frame = sampleExtractor.getFrame(size, buffer);
    if (frame_callback)
      frame_callback(frame, nullptr);
    else
      LOGW("No frame callback defined");
  }

  /**
   * @brief Provides the next sample size from the stsz box.
   * @return stsz sample size in bytes.
   */
  uint32_t getNextSampleSize() {
    if (sample_index >= sample_count) return 0;
    uint32_t currentSize = 0;
    if (fixed_sample_size) {
      currentSize = fixed_sample_size;
    } else {
      if (stsz_buf.isEmpty()) {
        uint64_t pos = stsz_offset + 20 + sample_index * 4;
        if (!file->seek(pos)) return false;
        stsz_buf.clear();
        size_t read_bytes = file->read(
            reinterpret_cast<uint8_t*>(stsz_buf.data()), stsz_bufsize * 4);
        stsz_buf.setWritePos(read_bytes / 4);
        if (stsz_buf.isEmpty()) return 0;
      }
      uint32_t val = 0;
      stsz_buf.read(val);
      currentSize = readU32(val);
    }
    sample_index++;
    return currentSize;
  }

  /**
   * @brief Reads the stsz header (sample count and fixed sample size) from
   * the file.
   * @return true if successful, false otherwise.
   */
  bool readStszHeader() {
    if (!file || stsz_offset == 0) return false;
    uint8_t buffer[20];
    if (!file->seek(stsz_offset)) return false;
    if (file->read(buffer, 20) != 20) return false;
    if (!checkType(buffer, "stsz", 4)) return false;
    uint8_t* cont = buffer + 8;
    fixed_sample_size =
        (cont[4] << 24) | (cont[5] << 16) | (cont[6] << 8) | cont[7];
    sample_count =
        (cont[8] << 24) | (cont[9] << 16) | (cont[10] << 8) | cont[11];
    return true;
  }

  bool checkMdat() {
    file->seek(mdat_offset - 8);
    uint8_t buffer[8];
    if (file->read(buffer, 8) != 8) return false;
    return checkType(buffer, "mdat", 4);
  }
};

}  // namespace audio_tools