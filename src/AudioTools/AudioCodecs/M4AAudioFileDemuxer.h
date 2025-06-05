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
class M4AAudioFileDemuxer : public M4AAudioDemuxer {
 public:
  /**
   * @brief Default constructor. Registers parser callbacks for relevant MP4
   * boxes.
   */
  M4AAudioFileDemuxer() = default;

  /**
   * @brief Constructor with decoder.
   * @param decoder Reference to MultiDecoder.
   */
  M4AAudioFileDemuxer(MultiDecoder& decoder) { setDecoder(decoder); }

  /**
   * @brief Sets the decoder to use for audio frames.
   * Make sure that the output has been defined for the decoder!
   * This call registers the frame callback to feed frames to the decoder.
   * @param decoder Reference to MultiDecoder.
   * @return true if set successfully.
   */
  bool setDecoder(MultiDecoder& decoder) {
    this->p_decoder = &decoder;
    if (decoder.getOutput() == nullptr) {
      LOGE("No output defined for MultiDecoder");
      return false;
    }
    // Set the frame callback to feed frames to the decoder
    setCallback([&decoder](const Frame& frame, void* /*ref*/) {
      decoder.selectDecoder(frame.mime);
      decoder.write(frame.data, frame.size);
    });

    return true;
  }

  /**
   * @brief Sets the callback for extracted audio frames.
   * @param cb Frame callback function.
   */
  virtual void setCallback(FrameCallback cb) { frame_callback = cb; }

  /// Sets the size of the samples buffer (in bytes) (default = 1024 bytes =
  /// 256 samples)
  void setSamplesBufferSize(int size) {
    stsz_bufsize = size / 4;        // 4 bytes per sample size
    stsz_buf.resize(stsz_bufsize);  // 4 bytes per sample size
  }

  /**
   * @brief Open and parse the given file.
   * @param file Reference to an open Arduino File object.
   * @return true on success, false on failure.
   */
  bool begin(File& file) {
    this->file = &file;
    if (!file) return false;

    // Reset state for copy()
    end();

    // start the decoder
    if (p_decoder) p_decoder->begin();

    // Read and parse the file in chunks (e.g. 2KB)
    uint8_t buffer[1024];
    file.seek(0);
    while (file.available()) {
      size_t len = file.read(buffer, sizeof(buffer));
      parser.write(buffer, len);
      // Stop early if both offsets are found
      if (stsd_processed && mdat_offset && stsz_offset) break;
    }

    // Read sample count and fixed size from stsz
    if (!readStszHeader()) return false;

    return (mdat_offset > 0 && stsz_offset > 0 && sample_count > 0);
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

    // Seek to the correct position in the mdat box
    uint64_t mdat_sample_pos = mdat_offset + mdat_pos;
    if (!file->seek(mdat_sample_pos)) return false;

    // Read the sample data from the file
    if (buffer.size() < currentSize) buffer.resize(currentSize);
    size_t bytesRead = file->read(buffer.data(), currentSize);
    if (bytesRead != currentSize) return false;
    buffer.setWritePos(bytesRead);

    executeCallback(currentSize, buffer);
    // Advance to next sample
    mdat_pos += currentSize;
    return true;
  }

  /**
   * @brief Close the file (does not actually close Arduino File, just clears
   * pointer and resets state).
   */
  void end() {
    codec = Codec::Unknown;
    alacMagicCookie.clear();
    resize(default_size);
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
      stsz_bufsize};  ///< Buffer for stsz sample sizes
  // stsz header info
  uint32_t sample_count = 0;               ///< Number of samples in stsz
  uint32_t fixed_sample_size = 0;          ///< Fixed sample size (if nonzero)
  bool stsd_processed = false;             ///< True if stsd box processed
  FrameCallback frame_callback = nullptr;  ///< Callback for extracted frames
  MultiDecoder* p_decoder = nullptr;       ///< Pointer to decoder

  void setupParser() override {
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
          self->onStsz(box);  // still needed for codec/sampleExtractor config
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
          self->mdat_offset = box.file_offset + 8;  // skip box header
          self->mdat_size = box.size;
          self->onStsd(box);
          self->stsd_processed;
        },
        false);
  }

  /**
   * @brief Executes the callback for a completed frame.
   * @param size Size of the frame.
   * @param buffer Buffer containing the frame data.
   */
  void executeCallback(size_t size, SingleBuffer<uint8_t>& buffer) {
    Frame frame = sampleExtractor.getFrame(size, buffer);
    if (frame_callback)
      frame_callback(frame, ref);
    else
      LOGE("No callback defined for audio frame extraction");
  }

  /**
   * @brief Provides the next sample size from the stsz box.
   * The sample size is actually a batch of bytes which need to
   * be processed in one call. In many decoders this is also called
   * "frame size".
   * @return stsz sample size in bytes.
   */
  uint32_t getNextSampleSize() {
    if (sample_index >= sample_count) return 0;
    uint32_t currentSize = 0;
    if (fixed_sample_size) {
      currentSize = fixed_sample_size;
    } else {
      // Refill buffer if empty
      if (stsz_buf.isEmpty()) {
        // Calculate offset of the sample size in the stsz table
        uint64_t pos = stsz_offset + 12 + sample_index * 4;
        // Read a new chunk from the stsz table
        if (!file->seek(pos)) return false;
        size_t read_bytes = file->read(
            reinterpret_cast<uint8_t*>(stsz_buf.data()), stsz_bufsize * 4);
        stsz_buf.setWritePos(read_bytes / 4);
        if (stsz_buf.isEmpty()) return 0;
      }
      uint32_t val = 0;
      stsz_buf.read(val);
      // Convert from big endian to host
      currentSize = ((val >> 24) & 0xFF) | ((val >> 8) & 0xFF00) |
                    ((val << 8) & 0xFF0000) | ((val << 24) & 0xFF000000);
    }
    sample_index++;
    return currentSize;
  }

  /**
   * @brief Get the offset of the mdat box payload.
   * @return Offset in bytes from the start of the file.
   */
  uint64_t getMdatOffset() const { return mdat_offset; }

  /**
   * @brief Get the size of the mdat box payload.
   * @return Size in bytes.
   */
  uint64_t getMdatSize() const { return mdat_size; }

  /**
   * @brief Get the offset of the stsz box.
   * @return Offset in bytes from the start of the file.
   */
  uint64_t getStszOffset() const { return stsz_offset; }

  /**
   * @brief Get the size of the stsz box.
   * @return Size in bytes.
   */
  uint64_t getStszSize() const { return stsz_size; }

  /**
   * @brief Reads the stsz header (sample count and fixed sample size) from
   * the file.
   * @return true if successful, false otherwise.
   */
  bool readStszHeader() {
    if (!file || stsz_offset == 0) return false;
    uint8_t buf[12];
    if (!file->seek(stsz_offset)) return false;
    if (file->read(buf, 12) != 12) return false;
    // buf[0..3] = version/flags, buf[4..7] = sample_size, buf[8..11] =
    // sample_count
    fixed_sample_size =
        (buf[4] << 24) | (buf[5] << 16) | (buf[6] << 8) | buf[7];
    sample_count = (buf[8] << 24) | (buf[9] << 16) | (buf[10] << 8) | buf[11];
    return true;
  }
};

}  // namespace audio_tools