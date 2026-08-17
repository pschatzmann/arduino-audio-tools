#pragma once
#include "AudioTools/CoreAudio/AudioBasic/Collections/Vector.h"
#include "AudioTools/CoreAudio/AudioLogger.h"

namespace audio_tools {

/// @brief Video vs. audio track classification - lives here (rather than
/// nested inside DemuxerMP4::Track, where it conceptually belongs) purely
/// so SpoolStorageFactory can reference it: this header is included
/// before DemuxerMP4/Track are defined, so a nested type wouldn't be
/// visible yet. DemuxerMP4::Track::kind uses this same type.
/// @ingroup video
enum class TrackKind { Unknown, Audio, Video };

/**
 * @brief Sequential-access storage for one MP4 sample table (e.g. stsz
 * sample sizes, stco chunk offsets, or the raw stsc/stts RLE entries).
 * Every real access pattern in DemuxerMP4 is append-only while parsing
 * 'moov', then forward-cursor reads while consuming 'mdat' - this
 * interface intentionally only supports that (no random-access mutation,
 * no delete), which is what keeps all three implementations below simple
 * and correct.
 * @ingroup video
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
template <typename T>
class SampleTableStore {
 public:
  virtual ~SampleTableStore() = default;
  /// Resets to empty - call once per begin()/track.
  virtual void clear() = 0;
  /// Appends one entry - call in file order, once per table entry.
  virtual void append(T value) = 0;
  /// Number of entries appended so far.
  virtual size_t size() = 0;
  /// Reads back entry 'index' - index must be < size().
  virtual T get(size_t index) = 0;
  /// Only meaningful for SourceSeekSampleTableStore: the absolute byte
  /// offset (from the start of the original MP4 file) of the entry about
  /// to be appended next. Default no-op - other implementations ignore
  /// it, so callers can always call this unconditionally before append()
  /// without needing to know which implementation is active.
  virtual void setNextEntryOffset(uint64_t fileOffset) {}
  /// Only meaningful for SourceSeekSampleTableStore, and only for tables
  /// whose on-disk width isn't a fixed sizeof(T) - see its comment.
  /// Default no-op.
  virtual void setOnDiskEntrySize(size_t bytes) {}
};

/**
 * @brief Keeps every entry in RAM (a plain Vector<T>) - the simplest and
 * fastest option, but scales with the track's total sample/chunk count
 * (e.g. ~4.5MB total for a 102-minute movie's stsz+stco tables). Default
 * if no other SampleTableStore is configured.
 * @ingroup video
 */
template <typename T>
class RamSampleTableStore : public SampleTableStore<T> {
 public:
  void clear() override { data.clear(); }
  void append(T value) override { data.push_back(value); }
  size_t size() override { return (size_t)data.size(); }
  T get(size_t index) override { return data[(int)index]; }

 protected:
  Vector<T> data;
};

/**
 * @brief RAM-backed like RamSampleTableStore, but grows by allocating
 * additional fixed-size chunks instead of reallocating and copying one
 * single growing array.
 *
 * This project's Vector<T>::push_back() (AudioBasic/Collections/Vector.h)
 * allocates exactly the new size on every growth, with no spare capacity
 * - once the array is full, every single push_back() reallocates *and
 * copies everything already in it*. For the hundreds of thousands of
 * entries a feature-length movie's stsz/stco tables need, that's O(N)
 * work per append, O(N^2) total, which is real, measured wall-clock time
 * spent before a single frame can play.
 *
 * This store sidesteps that by keeping a Vector of chunk pointers rather
 * than a Vector of T directly: growing by one chunk means appending one
 * more pointer (cheap - the pointer vector itself stays tiny, e.g. ~70
 * pointers for 288K entries at the default chunk size) and allocating a
 * fresh fixed-size block, without ever touching or copying any
 * previously-appended data. Same total memory as RamSampleTableStore, but
 * O(1) amortized append instead of O(N) - the same idea behind this
 * project's DynamicMultiBuffer (AudioTools/Sandbox/DynamicMultiBuffer.h),
 * reimplemented directly here rather than wrapped: that class's public
 * API is a sequential read()/write() cursor, not the indexed get()
 * ChunkedSampleTableStore needs, and as Sandbox-status code it's not
 * something to take a hard dependency on from a container decoder.
 * @ingroup video
 */
template <typename T, size_t ChunkSize = 4096>
class ChunkedSampleTableStore : public SampleTableStore<T> {
 public:
  ~ChunkedSampleTableStore() { clear(); }

  void clear() override {
    for (size_t i = 0; i < chunks.size(); i++) delete[] chunks[i];
    chunks.clear();
    count = 0;
  }

  void append(T value) override {
    size_t chunk_idx = count / ChunkSize;
    size_t local_pos = count % ChunkSize;
    if (chunk_idx >= (size_t)chunks.size()) chunks.push_back(new T[ChunkSize]);
    chunks[(int)chunk_idx][local_pos] = value;
    count++;
  }

  size_t size() override { return count; }

  T get(size_t index) override {
    return chunks[(int)(index / ChunkSize)][index % ChunkSize];
  }

 protected:
  Vector<T*> chunks;  // small - one pointer per ChunkSize entries, not one per entry
  size_t count = 0;
};

/**
 * @brief Minimal seek+read interface a SourceSeekSampleTableStore needs
 * from "the original MP4 source" - kept deliberately tiny (no write, no
 * generic Stream surface) and non-templated, so classes that only need to
 * hold/pass a source around (like DemuxerMP4 itself) don't have to become
 * templates just to support this one storage strategy.
 * @ingroup video
 */
class SeekableSource {
 public:
  virtual ~SeekableSource() = default;
  virtual bool seek(size_t pos) = 0;
  virtual size_t readBytes(uint8_t* data, size_t len) = 0;
};

/**
 * @brief Adapts a single concrete file/stream (Arduino's File, or
 * anything else exposing position(), seek(size_t) and a readBytes(size_t)
 * overload) to SeekableSource, and also drives feeding that same file's
 * bytes into a DemuxerMP4 (or any other AudioWriter with a
 * setSeekSource(SeekableSource&) method) via copy() - replacing the
 * separate CodecCopy/StreamCopy a sketch would otherwise need, and
 * letting this class use just one File for everything instead of
 * requiring two independent handles (one for sequential forward reads,
 * one for the out-of-band seeks SourceSeekSampleTableStore performs).
 *
 * That's safe because seek()/readBytes() always come in a matched pair
 * (SourceSeekSampleTableStore::get() never calls one without the other)
 * - seek() remembers the file's current position before jumping away,
 * and readBytes() restores it immediately after reading, so copy()'s own
 * sequential progress through the file is never disturbed by a table
 * lookup happening in between two copy() calls.
 *
 * This is the one place the concrete file type appears, isolating
 * SD.h/FS.h's include-order requirements to wherever the including
 * sketch instantiates this adapter (after it has already included SD.h),
 * rather than to this header or to DemuxerMP4 itself.
 *
 * WriterT is templated (duck-typed on write()+setSeekSource(), rather
 * than a fixed AudioWriter&/DemuxerMP4&) purely to avoid this header
 * having to depend on ContainerMP4.h, which itself depends on this
 * header - a hard circular include otherwise.
 * @ingroup video
 */
template <typename FileT, typename WriterT>
class FileSeekableSource : public SeekableSource {
 public:
  /// 'file' must already be open, positioned at the start (byte 0) of
  /// the MP4. 'writer' is both what copy() feeds decoded bytes into, and
  /// gets wired up automatically here via writer.setSeekSource(*this) -
  /// no separate setSeekSource() call needed in the sketch.
  FileSeekableSource(FileT& file, WriterT& writer, int bufferSize = 1024)
      : p_file(&file), p_writer(&writer), buffer_len(bufferSize) {
    writer.setSeekSource(*this);
  }

  bool seek(size_t pos) override {
    saved_pos = p_file->position();
    bool ok = p_file->seek(pos);
    if (debug_trace)
      LOGI("FileSeekableSource: seek(%u) saved_pos=%u post-seek position()=%u ok=%d",
           (unsigned)pos, (unsigned)saved_pos, (unsigned)p_file->position(), (int)ok);
    return ok;
  }

  size_t readBytes(uint8_t* data, size_t len) override {
    // File::readBytes() on some platforms (incl. this project's desktop
    // emulator) takes char*, not uint8_t* - readBytes(uint8_t*,size_t) is
    // the more common Arduino Stream signature, but not universal.
    size_t pre_pos = p_file->position();
    size_t n = p_file->readBytes((char*)data, len);
    if (debug_trace)
      LOGI("FileSeekableSource: readBytes len=%u pre_pos=%u got=%u restoring to saved_pos=%u",
           (unsigned)len, (unsigned)pre_pos, (unsigned)n, (unsigned)saved_pos);
    p_file->seek(saved_pos);  // restore the sequential-read position
    return n;
  }

  /// Set true to LOGI every seek()/readBytes()/copy() call with the raw
  /// file positions involved - off by default (adds overhead on what can
  /// be a hot path during playback); a debugging aid, not something a
  /// normal sketch needs to touch.
  bool debug_trace = false;

  /// Reads the next chunk from the file and feeds it to the configured
  /// AudioWriter, retrying on a partial write - the same contract
  /// CodecCopy/StreamCopy follow, since DemuxerMP4::write() (like most
  /// Print::write() implementations) may accept fewer bytes than
  /// requested in one call. Returns 0 once the file is exhausted.
  size_t copy() {
    uint8_t buffer[buffer_len];
    size_t to_read = p_file->readBytes((char*)buffer, buffer_len);
    if (to_read == 0) return 0;
    size_t written = 0;
    int attempts = 0;
    while (written < to_read) {
      size_t w = p_writer->write(buffer + written, to_read - written);
      attempts++;
      if (w == 0) {
        LOGW("FileSeekableSource::copy(): writer stalled after accepting %u of %u "
             "bytes (%d attempt(s)) - dropping the remaining %u bytes",
             (unsigned)written, (unsigned)to_read, attempts, (unsigned)(to_read - written));
        break;
      }
      written += w;
    }
    if (debug_trace && attempts > 1)
      LOGI("FileSeekableSource::copy() needed %d attempts for to_read=%u", attempts, (unsigned)to_read);
    return written;
  }

 protected:
  FileT* p_file;
  WriterT* p_writer;
  int buffer_len;
  size_t saved_pos = 0;
};

/**
 * @brief Discards every value right after appending it, keeping only the
 * file offset of the first entry - get() seeks back into the original MP4
 * source and re-reads the entry directly (entries are fixed-size and
 * contiguous on disk, so entry N sits at start_offset + N*sizeof(T)),
 * trading RAM for repeated small seeks.
 *
 * Needs the original source to (a) support seek()/read() (a local File,
 * not a live network stream) and (b) stay open for the lifetime of
 * playback, since entries are re-read from it on demand while consuming
 * 'mdat' - long after 'moov' parsing has moved past them.
 *
 * MP4's on-disk integers are big-endian, and the box handlers already
 * decode them field-by-field (readU32()/readU64()) rather than via a
 * plain memcpy - get() has to reproduce that exact decoding independently
 * later, since it can't call back into the box handler at playback time.
 * A raw byte-for-byte copy into T would silently read wrong values on any
 * little-endian host (i.e. virtually everything). 'decoder' plus
 * 'onDiskSize' let the caller supply the same decoding logic and on-disk
 * width the corresponding box handler used - onDiskSize matters because
 * it isn't always sizeof(T): e.g. chunk offsets are always decoded into a
 * uint64_t in memory, but are 4 bytes on disk for 'stco' vs. 8 for
 * 'co64' - see setOnDiskEntrySize().
 *
 * get() is called once per sample, per table, during 'mdat' playback, and
 * real playback access is overwhelmingly sequential (index, then index+1,
 * then index+2, ...) since samples are consumed in roughly presentation
 * order per track. A naive one-seek-one-read-per-get() (the original
 * implementation) turns that into a syscall pair per single 4-8 byte
 * value, which measurably slowed real playback down on a desktop
 * filesystem - so get() keeps a small read-ahead window (cacheEntries
 * decoded values, default 64) instead: a cache miss reads a whole block
 * of consecutive entries in one seek+read and decodes them all at once,
 * so a sequential scan pays that cost only once per 'cacheEntries' calls
 * rather than once per call. Backward/random access still works (a miss
 * just re-centers the window on the new index) but won't benefit from the
 * caching the way sequential access does.
 * @ingroup video
 */
template <typename T>
class SourceSeekSampleTableStore : public SampleTableStore<T> {
 public:
  /// Upper bound on on-disk entry width this store can decode - matches
  /// the widest entry actually used (chunk offsets: 8 bytes for 'co64';
  /// stsc/stts: 8 bytes each) with headroom.
  static constexpr size_t kMaxOnDiskSize = 16;

  /// 'source' must wrap the same open file DemuxerMP4 is (directly or
  /// indirectly) being fed from, positioned so that byte 0 of the file
  /// corresponds to the very first byte ever passed to
  /// DemuxerMP4::write() - entry offsets are computed relative to that.
  /// 'decoder' converts 'onDiskSize' raw big-endian bytes into a T value.
  /// 'cacheEntries' is the read-ahead window size - see the class comment.
  SourceSeekSampleTableStore(SeekableSource& source, size_t onDiskSize,
                              T (*decoder)(const uint8_t*),
                              size_t cacheEntries = 64)
      : p_source(&source),
        on_disk_size(onDiskSize),
        decode(decoder),
        cache_capacity(cacheEntries),
        cache(new T[cacheEntries]),
        raw_buf(new uint8_t[cacheEntries * kMaxOnDiskSize]) {}

  ~SourceSeekSampleTableStore() {
    delete[] cache;
    delete[] raw_buf;
  }

  void clear() override {
    count = 0;
    start_offset = -1;
    cache_len = 0;
  }

  void setNextEntryOffset(uint64_t fileOffset) override {
    if (start_offset < 0) start_offset = (int64_t)fileOffset;
  }

  /// Overrides the on-disk entry width assumed by get() - needed only for
  /// tables whose on-disk width can differ from sizeof(T) (chunk_offsets:
  /// 4 bytes for 'stco', 8 for 'co64', always stored as uint64_t). Safe
  /// to call any time before the first get(); has no effect after.
  void setOnDiskEntrySize(size_t bytes) override {
    on_disk_size = bytes;
    cache_len = 0;  // invalidate: any cached block used the old width
  }

  void append(T) override {
    // value itself is intentionally discarded - only its position matters
    count++;
  }

  size_t size() override { return count; }

  T get(size_t index) override {
    if (start_offset < 0 || p_source == nullptr) return T{};
    if (index < cache_start || index >= cache_start + cache_len)
      refillCache(index);
    if (index >= cache_start && index < cache_start + cache_len)
      return cache[index - cache_start];
    return T{};
  }

 protected:
  void refillCache(size_t index) {
    p_source->seek((size_t)(start_offset + (int64_t)(index * on_disk_size)));
    size_t got = p_source->readBytes(raw_buf, cache_capacity * on_disk_size);
    size_t got_entries = got / on_disk_size;
    for (size_t i = 0; i < got_entries; i++)
      cache[i] = decode(raw_buf + i * on_disk_size);
    cache_start = index;
    cache_len = got_entries;
  }

  SeekableSource* p_source;
  size_t on_disk_size;
  T (*decode)(const uint8_t*);
  int64_t start_offset = -1;
  size_t count = 0;

  size_t cache_capacity;
  T* cache;
  uint8_t* raw_buf;
  size_t cache_start = 0;
  size_t cache_len = 0;
};

/**
 * @brief Minimal seek+read+write interface a SpoolFileSampleTableStore
 * needs from its scratch file - SeekableSource plus write(), kept
 * non-templated for the same reason as SeekableSource (see its comment):
 * so DemuxerMP4/SpoolStorageFactory don't need to become templates just
 * to hold/pass one of these around.
 * @ingroup video
 */
class SpoolStorage : public SeekableSource {
 public:
  virtual size_t write(const uint8_t* data, size_t len) = 0;
};

/**
 * @brief Adapts any concrete file/stream type to SpoolStorage - same role
 * as FileSeekableSource, just adding write(). This is the one place the
 * concrete file type appears for spool-backed storage.
 * @ingroup video
 */
template <typename FileT>
class FileSpoolStorage : public SpoolStorage {
 public:
  FileSpoolStorage(FileT& file) : p_file(&file) {}
  bool seek(size_t pos) override { return p_file->seek(pos); }
  size_t readBytes(uint8_t* data, size_t len) override {
    // see the identical comment in FileSeekableSource::readBytes()
    return p_file->readBytes((char*)data, len);
  }
  size_t write(const uint8_t* data, size_t len) override {
    return p_file->write(data, len);
  }

 protected:
  FileT* p_file;
};

/**
 * @brief Writes every entry to a caller-provided scratch file as it's
 * appended, instead of keeping it in RAM - get() seeks that file and reads
 * the entry back. Unlike SourceSeekSampleTableStore this needs no
 * knowledge of (or seekability in) the *original* MP4 source - it works
 * even when that source is a live, non-seekable stream, since it's
 * writing its own local, sequential copy as data streams past. The
 * tradeoff is disk I/O (both a write during parsing and a seek+read
 * during playback) instead of a pure RAM cost.
 *
 * Unlike SourceSeekSampleTableStore there's no endianness/on-disk-width
 * concern here - this store's on-disk layout is its own private format
 * (whatever bytes T's native in-memory representation happens to be), not
 * MP4's, so a plain byte-for-byte round-trip is correct as long as the
 * same host reads back what it wrote (true here - never persisted across
 * runs).
 *
 * Each instance needs its own dedicated SpoolStorage - see
 * DemuxerMP4::setSpoolStorageFactory() for how one gets created per
 * table per track.
 * @ingroup video
 */
template <typename T>
class SpoolFileSampleTableStore : public SampleTableStore<T> {
 public:
  /// 'spool' must be open for reading, writing and seeking, and
  /// empty/truncated at construction time.
  SpoolFileSampleTableStore(SpoolStorage& spool) : p_spool(&spool) {}

  void clear() override {
    entry_count = 0;
    if (p_spool != nullptr) p_spool->seek(0);
  }

  void append(T value) override {
    if (p_spool == nullptr) return;
    p_spool->seek(entry_count * sizeof(T));
    p_spool->write((const uint8_t*)&value, sizeof(T));
    entry_count++;
  }

  size_t size() override { return entry_count; }

  T get(size_t index) override {
    T value{};
    if (p_spool == nullptr) return value;
    p_spool->seek(index * sizeof(T));
    p_spool->readBytes((uint8_t*)&value, sizeof(T));
    return value;
  }

 protected:
  SpoolStorage* p_spool;
  size_t entry_count = 0;
};

/**
 * @brief Identifies which of a track's four sample tables a
 * SpoolStorageFactory is being asked to provide storage for.
 * @ingroup video
 */
enum class SampleTableKind { SampleSizes, ChunkOffsets, Stsc, Stts };

/**
 * @brief Factory DemuxerMP4 calls once per table, per track, to obtain a
 * SpoolStorage for spool-backed sample table storage - needed (rather
 * than a single shared file/setter, the way setSeekSource() works)
 * because up to two tracks (video + audio) times four tables means up to
 * eight distinct spool regions are needed, and DemuxerMP4 doesn't know
 * how many tracks a file has, or which kind each one is, until it's
 * actually parsing 'moov' - implement this to open/return whatever
 * files or regions your storage scheme uses (e.g. one file per call,
 * named by trackKind+tableKind).
 * @ingroup video
 */
class SpoolStorageFactory {
 public:
  virtual ~SpoolStorageFactory() = default;
  /// Called once per table as each track's kind becomes known (i.e. once
  /// 'hdlr' has been parsed) - must return a freshly usable, empty
  /// SpoolStorage dedicated to this table; ownership stays with the
  /// factory (DemuxerMP4 only uses it, never deletes it).
  virtual SpoolStorage* createSpoolStorage(TrackKind trackKind,
                                            SampleTableKind tableKind) = 0;
};

}  // namespace audio_tools
