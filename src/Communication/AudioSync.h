
#pragma once
#include "AudioTools/AudioStreams.h"
#include "AudioTools/AudioOutput.h"
#include "AudioTools/Buffers.h"
#include "AudioBasic/Str.h"
#include "AudioCodecs/AudioEncoded.h"


namespace audio_tools {


enum class RecordType : uint8_t { Undefined, Begin, Send, Receive, End };
enum class AudioType : uint8_t { PCM, MP3, AAC, WAV, ADPC };
enum class TransmitRole : uint8_t { Sender, Receiver };

/// Common Header for all records
struct AudioHeader {
  AudioHeader() = default;
  uint8_t app = 123;
  RecordType rec = RecordType::Undefined;
  uint16_t seq = 0;
  // record counter
  void increment() {
    static uint16_t static_count = 0;
    seq = static_count++;
  }
};

/// Protocal Record To Start
struct AudioDataBegin : public AudioHeader {
  AudioDataBegin() { rec = RecordType::Begin; }
  AudioInfo info;
  AudioType type = AudioType::PCM;
};

/// Protocol Record for Data
struct AudioSendData : public AudioHeader {
  AudioSendData() {
    rec = RecordType::Send;
  }
  uint16_t size = 0;
};

/// Protocol Record for Request
struct AudioConfirmDataToReceive : public AudioHeader {
  AudioConfirmDataToReceive() { rec = RecordType::Receive; }
  uint16_t size = 0;
};

/// Protocol Record for End
struct AudioDataEnd : public AudioHeader {
  AudioDataEnd() { rec = RecordType::End; }
};


/**
 * @brief Audio Writer which is synchronizing the amount of data
 * that can be processed with the AudioReceiver
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AudioSyncWriter : public AudioOutput {
 public:
  AudioSyncWriter(Stream &dest) { p_dest = &dest; }

  bool begin(AudioInfo &info, AudioType type) {
    is_sync = true;
    AudioDataBegin begin;
    begin.info = info;
    begin.type = type;
    begin.increment();
    int write_len = sizeof(begin);
    int len = p_dest->write((const uint8_t *)&begin, write_len);
    return len == write_len;
  }

  size_t write(const uint8_t *data, size_t len) override {
    int written_len = 0;
    int open_len = len;
    AudioSendData send;
    while (written_len < len) {
      int available_to_write = waitForRequest();
      size_t to_write_len = DEFAULT_BUFFER_SIZE;
      to_write_len = min(open_len, available_to_write);
      send.increment();
      send.size = to_write_len;
      p_dest->write((const uint8_t *)&send, sizeof(send));
      int w = p_dest->write(data + written_len, to_write_len);
      written_len += w;
      open_len -= w;
    }
    return written_len;
  }

  int availableForWrite() override { return available_to_write; }

  void end() {
    AudioDataEnd end;
    end.increment();
    p_dest->write((const uint8_t *)&end, sizeof(end));
  }

 protected:
  Stream *p_dest;
  int available_to_write = 1024;
  bool is_sync;

  /// Waits for the data to be available
  void waitFor(int size) {
    while (p_dest->available() < size) {
      delay(10);
    }
  }

  int waitForRequest() {
    AudioConfirmDataToReceive rcv;
    size_t rcv_len = sizeof(rcv);
    waitFor(rcv_len);
    p_dest->readBytes((uint8_t *)&rcv, rcv_len);
    return rcv.size;
  }
};

/**
 * @brief Receving Audio Data over the wire and requesting for more data when
 * done to synchronize the processing with the sender. The audio data is
 * processed by the EncodedAudioStream; If you have multiple readers, only one
 * receiver should be used as confirmer!
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AudioSyncReader : public AudioStream {
 public:
  AudioSyncReader(Stream &in, EncodedAudioStream &out,
                  bool isConfirmer = true) {
    p_in = &in;
    p_out = &out;
    is_confirmer = isConfirmer;
  }

  size_t copy() {
    int processed = 0;
    int header_size = sizeof(header);
    waitFor(header_size);
    readBytes((uint8_t *)&header, header_size);

    switch (header.rec) {
      case RecordType::Begin:
        audioDataBegin();
        break;
      case RecordType::End:
        audioDataEnd();
        break;
      case RecordType::Send:
        processed = receiveData();
        break;
    }
    return processed;
  }

 protected:
  Stream *p_in;
  EncodedAudioStream *p_out;
  AudioConfirmDataToReceive req;
  AudioHeader header;
  AudioDataBegin begin;
  size_t available = 0;  // initial value
  bool is_started = false;
  bool is_confirmer;
  int last_seq = 0;

  /// Starts the processing
  void audioDataBegin() {
    readProtocol(&begin, sizeof(begin));
    p_out->begin();
    p_out->setAudioInfo(begin.info);
    requestData();
  }

  /// Ends the processing
  void audioDataEnd() {
    AudioDataEnd end;
    readProtocol(&end, sizeof(end));
    p_out->end();
  }

  // Receives audio data
  int receiveData() {
    AudioSendData data;
    readProtocol(&data, sizeof(data));
    available = data.size;
    // receive and process audio data
    waitFor(available);
    int max_gap = 10;
    if (data.seq > last_seq ||
        (data.seq < max_gap && last_seq >= (32767 - max_gap))) {
      uint8_t buffer[available];
      p_in->readBytes((uint8_t *)buffer, available);
      p_out->write((const uint8_t *)buffer, available);
      // only one reader should be used as confirmer
      if (is_confirmer) {
        requestData();
      }
      last_seq = data.seq;
    }
    return available;
  }

  /// Waits for the data to be available
  void waitFor(int size) {
    while (p_in->available() < size) {
      delay(10);
    }
  }

  /// Request new data from writer
  void requestData() {
    req.size = p_out->availableForWrite();
    req.increment();
    p_in->write((const uint8_t *)&req, sizeof(req));
    p_in->flush();
  }

  /// Reads the protocol record
  void readProtocol(AudioHeader *data, int len) {
    static const int header_size = sizeof(header);
    memcpy(data, &header, header_size);
    int read_size = len - header_size;
    waitFor(read_size);
    readBytes((uint8_t *)data + header_size, read_size);
  }
};

}  // namespace audio_tools
