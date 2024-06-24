/**
 * @file CodecGGWave.h
 * @author Phil Schatzmann
 * @brief GGWve Codec 
 * Codec using https://github.com/ggerganov/ggwave-arduinop
 * @version 0.1
 * @date 2022-12-14
 * @defgroup codec-ggwave ggwave
 * @ingroup codecs
 */
#pragma once

#include "AudioCodecs/AudioEncoded.h"
#include "AudioTools/Buffers.h"
#include "AudioEffects/SoundGenerator.h"
#include <ggwave.h>

#define GGWAVE_DEFAULT_SAMPLE_RATE 48000
#define GGWAVE_DEFAULT_PAYLOAD_LEN 16
#define GGWAVE_DEFAULT_SAMPLES_PER_FRAME 256
#define GGWAVE_DEFAULT_BYTES_PER_FRAME GGWAVE_DEFAULT_SAMPLES_PER_FRAME*GGWAVE_DEFAULT_PAYLOAD_LEN
#define GGWAVE_DEFAULT_PROTOCOL GGWAVE_PROTOCOL_AUDIBLE_FAST 
#define GGWAVE_DEFAULT_SAMPLE_BYTESIZE 2

//GGWAVE_PROTOCOL_DT_FAST

namespace audio_tools {

/**
 * @brief GGWaveDecoder: Translates audio into text
 * Codec using https://github.com/ggerganov/ggwave-arduino
 * @ingroup codec-ggwave
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class GGWaveDecoder : public AudioDecoder {
public:
  GGWaveDecoder() { 
    info.sample_rate = GGWAVE_DEFAULT_SAMPLE_RATE;
   }

  GGWaveDecoder(Print &out_stream) : GGWaveDecoder() { TRACED(); pt_print=&out_stream; }

  virtual void setOutput(Print &out_stream) {pt_print=&out_stream;}

  AudioInfo audioInfo() { return info; }
  
  void setAudioInfo(AudioInfo ai) { info = ai; }

  /// @brief  Activate multiple protocols
  /// @param protocol 
  template  <int N> 
  void setProtocols(ggwave_ProtocolId protocolsArray[N]){
    for (int j=0;j<N;j++){
      protocols.push_back(protocolsArray[j]);
    }
  }

  void setSamplesFormatInput(ggwave_SampleFormat fmt){
    samples_format_input = fmt;
  }

  void setSamplesFromatOutput(ggwave_SampleFormat fmt) {
    samples_format_output = fmt;
  }

  void setSamplesPerFrame(int samples){
   samples_per_frame = samples;
  }

  void setPayloadLen(int len){
    playload_len = len;
  }

  void setSampleByteSize(int size){
    sample_byte_size = size;
  }

  void begin() {
    if (pt_print==nullptr){
      LOGE("final destination not defined");
      return;
    }

    receive_buffer.resize(samples_per_frame*sample_byte_size);
    ggwave.setLogFile(nullptr);

    auto p = GGWave::getDefaultParameters();
    p.payloadLength   = playload_len;
    p.sampleRateInp   = info.sample_rate;
    p.sampleRateOut   = info.sample_rate;
    p.sampleRate      = info.sample_rate;
    p.samplesPerFrame = samples_per_frame;
    p.sampleFormatInp = samples_format_input;
    p.sampleFormatOut = samples_format_output;
    p.operatingMode   = GGWAVE_OPERATING_MODE_RX | GGWAVE_OPERATING_MODE_USE_DSS ;


    // Remove the ones that you don't need to reduce memory usage
    GGWave::Protocols::tx().disableAll();
    GGWave::Protocols::rx().disableAll();

    // activate additional protocols
    GGWave::Protocols::rx().toggle(GGWAVE_DEFAULT_PROTOCOL, true);
    for (auto protocol: protocols){
      GGWave::Protocols::rx().toggle(protocol,  true);
    }

    // Initialize the "ggwave" instance:
    active = true;
    if (!ggwave.prepare(p, true)){
        active = false;
        LOGE("prepare failed");
    }
  }


  void end() {
    ggwave.rxStopReceiving();
    active = false;
  }

  size_t write(const uint8_t *data, size_t len) { 
    if (!active) return 0;
    uint8_t *p_byte = (uint8_t *)data;
    for (int j=0;j<len;j++){
        receive_buffer.write(p_byte[j]);
        if (receive_buffer.availableForWrite()==0){
          decode();
        }
    }
    return len;
   }

  operator bool() { return active; }

  // The result is encoded data
  virtual bool isResultPCM() { return false;} 

protected:
  Print *pt_print=nullptr;
  AudioInfo info;
  GGWave ggwave;
  GGWave::TxRxData data;
  SingleBuffer<uint8_t> receive_buffer{0};
  Vector<ggwave_ProtocolId> protocols;
  ggwave_SampleFormat samples_format_input = GGWAVE_SAMPLE_FORMAT_I16;
  ggwave_SampleFormat samples_format_output = GGWAVE_SAMPLE_FORMAT_U8;
  int samples_per_frame = 0; //GGWAVE_DEFAULT_SAMPLES_PER_FRAME;
  int playload_len = GGWAVE_DEFAULT_PAYLOAD_LEN;
  int sample_byte_size = GGWAVE_DEFAULT_SAMPLE_BYTESIZE;
  bool active = false;

  void decode() {
    if (receive_buffer.available()>0){
      if (ggwave.decode(receive_buffer.address(), receive_buffer.available()/(info.bits_per_sample/8))) {
          // Check if we have successfully decoded any data:
          int nr = ggwave.rxTakeData(data);
          if (nr > 0) {
              pt_print->write((uint8_t*) &data[0], nr);
          }
      } else {
          LOGW("decoding error");
      }
      receive_buffer.reset();
    }
  }
};

/**
 * @brief GGWaveEncoder: Translates text into audio
 * Codec using https://github.com/ggerganov/ggwave-arduino
 * @ingroup codec-ggwave
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class GGWaveEncoder : public AudioEncoder {
public:
  GGWaveEncoder() { 
    info.sample_rate = GGWAVE_DEFAULT_SAMPLE_RATE;
   }

  GGWaveEncoder(Print &out_stream) : GGWaveEncoder() { 
    TRACED(); 
    pt_print=&out_stream;
   }

  void setOutput(Print &out_stream) {pt_print=&out_stream;}

  void setSamplesFormatInput(ggwave_SampleFormat fmt){
    samples_format_input = fmt;
  }

  void setSamplesFromatOutput(ggwave_SampleFormat fmt) {
    samples_format_output = fmt;
  }

  void setSamplesPerFrame(int samples){
   samples_per_frame = samples;
  }

  void setPayloadLen(int len){
    playload_len = len;
  }

  void setProtocol(ggwave_ProtocolId protocol){
    protocolId = protocol;
  }

  void setSampleByteSize(int size){
    sample_byte_size = size;
  }

  void begin() {
    TRACEI();
    ggwave.setLogFile(nullptr);
    if (pt_print==nullptr){
      LOGE("final destination not defined");
      return;
    }
    sine_wave.begin(info.channels, info.sample_rate, 0);

    auto parameters = GGWave::getDefaultParameters();
    parameters.payloadLength   = playload_len;
    parameters.sampleRateInp   = info.sample_rate;
    parameters.sampleRateOut   = info.sample_rate;
    parameters.sampleRate      = info.sample_rate;
    parameters.samplesPerFrame = samples_per_frame;
    parameters.sampleFormatInp = samples_format_input;
    parameters.sampleFormatOut = samples_format_output;
    parameters.operatingMode   = GGWAVE_OPERATING_MODE_TX | GGWAVE_OPERATING_MODE_USE_DSS;

    // Remove the ones that you don't need to reduce memory usage
    GGWave::Protocols::tx().only(protocolId);

    // Sometimes, the speaker might not be able to produce frequencies in the Mono-tone range of 1-2 kHz.
    // In such cases, you can shift the base frequency up by changing the "freqStart" parameter of the protocol.
    // Make sure that in the receiver (for example, the "Waver" app) the base frequency is shifted by the same amount.
    // Here we shift the frequency by +48 bins. Each bin is equal to 48000/1024 = 46.875 Hz.
    // So the base frequency is shifted by +2250 Hz.
    //GGWave::Protocols::tx()[protocolId].freqStart += 48;

    ggwave.prepare(parameters);
    active = true;
  }

  void end() {
    TRACEI();
    active = false;
  }

  size_t write(const uint8_t *data, size_t len) { 
    if (!active) return 0;

    if (!ggwave.init(len, (const char*)data, protocolId)){
      LOGE("init faild");
      return 0;
    }
    size_t bytes = ggwave.encode();
    LOGI("write %d", bytes);
    //int written = pt_print->write((uint8_t*)ggwave.txWaveform(), bytes);
    
    const auto &protocol = GGWave::Protocols::tx()[protocolId];
    const auto tones = ggwave.txTones();
    const auto duration_ms = protocol.txDuration_ms(ggwave.samplesPerFrame(), ggwave.sampleRateOut());
    for (auto & curTone : tones) {
        const auto freq_hz = (protocol.freqStart + curTone)*ggwave.hzPerSample();
        play(freq_hz, duration_ms);
    }

    silence(info.sample_rate / 2); // 500 ms
    return len;
  }

  operator bool(){
    return active;
  }

   virtual const char *mime() {
    return "audio/pcm";
   }

protected:
  Print *pt_print=nullptr;
  GGWave ggwave;
  ggwave_ProtocolId protocolId = GGWAVE_DEFAULT_PROTOCOL; 
  int samples_per_frame = GGWAVE_DEFAULT_SAMPLES_PER_FRAME;
  ggwave_SampleFormat samples_format_input = GGWAVE_SAMPLE_FORMAT_I16;
  ggwave_SampleFormat samples_format_output = GGWAVE_SAMPLE_FORMAT_U8;
  int playload_len = GGWAVE_DEFAULT_PAYLOAD_LEN;
  int volume = GGWave::kDefaultVolume;
  int sample_byte_size = GGWAVE_DEFAULT_SAMPLE_BYTESIZE;
  bool active = false;
  //SineFromTable<int16_t> sine_wave;
  FastSineGenerator<int16_t> sine_wave; 

  virtual void play(int freq, int ms){
    // adjust amplitude by pitch bcause high values are too loud
    int amplitude = 10000; // / (freq/1000);
    sine_wave.setAmplitude(amplitude);
    sine_wave.setFrequency(freq);
    uint64_t end = millis()+ms;
    while(millis()<end){
      int16_t sample = sine_wave.readSample();
      for (int ch=0;ch<info.channels;ch++){
        pt_print->write((uint8_t*)&sample,2);
      }
    }
  }

  virtual void silence(int samples){
    // adjust amplitude by pitch bcause high values are too loud
    int16_t sample = 0;
    for (int ch=0;ch<info.channels;ch++){
      pt_print->write((uint8_t*)&sample,2);
    }    
  }

};

}