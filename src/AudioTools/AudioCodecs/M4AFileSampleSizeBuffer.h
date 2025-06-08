#pragma once

#include "AudioTools/CoreAudio/AudioPlayer.h"
#include "AudioTools/CoreAudio/Buffers.h"
#include "M4AAudioFileDemuxer.h"

namespace audio_tools {

/**
 * @brief A buffer that reads sample sizes from an M4A file using the
 * M4AAudioFileDemuxer. No RAM is used to store the sample sizes as they are
 * read directly from the file.
 *
 * This buffer is designed to be used with an AudioPlayer instance for audio
 * sources which are file based only. It provides a read interface that fetches
 * the next sample size directly from the file via the demuxer, avoiding the
 * need to store the entire sample size table in RAM.
 *
 * @note This buffer is can not be used for streaming sources; it is intended for
 * the use with file-based playback.
 * @note This class registers a setOnStreamChangeCallback() with the player
 */
class M4AFileSampleSizeBuffer : public BaseBuffer<stsz_sample_size_t> {
 public:
  /**
   * @brief Constructor.
   * @param player Reference to the AudioPlayer instance.
   * @param fileExt File extension to recognize as M4A (default ".m4a").
   */
  M4AFileSampleSizeBuffer(AudioPlayer& player, ContainerM4A& container,
                          const char* fileExt = ".m4a") {
    this->p_player = &player;
    this->p_container = &container;
    player.setReference(this);
    player.setOnStreamChangeCallback(onFileChange);
    addFileExtension(fileExt);
  }

  /**
   * @brief Get the next sample size from the demuxer.
   * @param data Reference to store the sample size.
   * @return true if successful, false otherwise.
   */
  bool read(stsz_sample_size_t& data) override {
    if (p_file != nullptr && demuxer.getMdatOffset() == 0) {
      uint32_t offset = p_container->getDemuxer().getStszFileOffset();
      uint32_t s_count = p_container->getDemuxer().getSampleCount();
      demuxer.beginSampleSizeAccess(p_file, s_count, offset);
    }
    size_t pos = p_file->position();
    data = demuxer.getNextSampleSize();
    p_file->seek(pos);  // reset position after reading
    return demuxer;
  }

  /**
   * @brief Defines how many samples are buffered with each file read.
   * @param size Number of bytes to buffer (will be divided by 4 for sample
   * count).
   */
  void setReadBufferSize(size_t size) { demuxer.setSamplesBufferSize(size); }

  /**
   * @brief Add a file extension to recognize as relevant for this buffer.
   * @param fileExt File extension string (e.g., ".m4a").
   */
  void addFileExtension(const char* fileExt) {
    fileExtensions.push_back(fileExt);
  }

  void reset() {}

  /**
   * @brief Write is ignored; sample sizes are read directly from the file.
   * @param data Sample size value (ignored).
   * @return Always true. This buffer is read-only.
   */
  bool write(stsz_sample_size_t data) { return true; }

  /**
   * @brief Peek is not supported for this buffer.
   * @param result Reference to store the peeked value (unused).
   * @return Always false. Peeking is not supported.
   */
  bool peek(stsz_sample_size_t& result) { return false; }

  /**
   * @brief Returns the number of samples already read (i.e., the current sample
   * index).
   * @return Number of samples read so far.
   */
  int available() { return demuxer.sampleIndex(); };

  /**
   * @brief Returns the available space for writing.
   * @return Always 0, as this buffer does not support writing.
   */
  int availableForWrite() override { return 0; }  ///< No write buffer available

  /**
   * @brief Returns the total number of samples in the file.
   * @return Total sample count.
   */
  size_t size() override { return demuxer.size(); }

  /**
   * @brief Returns a pointer to the buffer's physical address.
   * @return Always nullptr, as this buffer does not have a physical address.
   */
  stsz_sample_size_t* address() override { return nullptr; }

 protected:
  AudioPlayer* p_player = nullptr;     ///< Pointer to the AudioPlayer instance
  File* p_file = nullptr;              ///< Pointer to the currently open file
  M4AAudioFileDemuxer demuxer;         ///< Demuxer used to extract sample sizes
  Vector<const char*> fileExtensions;  ///< List of recognized file extensions
  ContainerM4A* p_container = nullptr;

  /**
   * @brief Checks if the given file name matches any of the registered
   * extensions.
   * @param name File name to check.
   * @return true if the file is relevant, false otherwise.
   */
  bool isRelevantFile(const char* name) {
    for (const auto& ext : fileExtensions) {
      if (StrView(name).endsWith(name)) return true;
    }
    return false;
  }

  /**
   * @brief Static callback for file change events.
   *        Updates the file pointer and re-parses the file if relevant.
   * @param streamPtr Pointer to the new file stream.
   * @param reference Pointer to the M4AFileSampleSizeBuffer instance.
   */
  static void onFileChange(Stream* streamPtr, void* reference) {
    M4AFileSampleSizeBuffer& self =
        *static_cast<M4AFileSampleSizeBuffer*>(reference);
    self.p_file = (File*)streamPtr;
    LOGI("===> M4AFileSampleSizeBuffer onFileChange: %s",
         self.p_file ? self.p_file->name() : "nullptr");
  }
};

}  // namespace audio_tools