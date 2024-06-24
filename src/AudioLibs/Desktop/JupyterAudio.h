#pragma once
#include "AudioLibs/Desktop/NoArduino.h"
#include "AudioTools/AudioStreams.h"
#include "AudioTools/AudioOutput.h"
#include "AudioCodecs/CodecWAV.h"
#include <string.h>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <stdio.h>
#include "nlohmann/json.hpp"
#include "xtl/xbase64.hpp"

namespace audio_tools {

/**
 * @brief Simple layer for Print object to write to a c++ file
 */
class FileOutput : public Print {
public:
    FileOutput(std::fstream &stream){
        p_audio_stream = &stream;
    }
    size_t write(const uint8_t *data, size_t len) override {
         p_audio_stream->write((const char*)data,len);
         return len;
    }
    int availableForWrite() override {
      return 1024;
    }
protected:
    std::fstream *p_audio_stream=nullptr;
};


/**
 * @brief Displays audio in a Jupyter as chart
 * Just wrapps a stream to provide the chart data
 */
template <typename T> 
class ChartT {
public:
  void setup(std::string fName, int channelCount, int channelNo) {
    this->fname = fName;
    this->channels = channelCount;
    if (this->channels==0){
      LOGE("Setting channels to 0");
    }
    this->channel = channelNo;
  }

  int getChannels() {
    return this->channels;
  }

  int getChannel() {
    return this->channel;
  }

  /// Provides data as svg polyline
  const std::string chartData() {
    str.clear();
    str.str("");
    // reset buffer;
    if (channel<channels){
        ifstream is;
        is.open(fname, is.binary);
        is.seekg(wav_header_size, is.beg);
        std::list<int16_t> audioList;
        T buffer[channels];
        size_t rec_size = channels*sizeof(T);
        while(is.read((char *)buffer, rec_size)){
            audioList.push_back(transform(buffer[channel]));
        }
        string str_size = "102400"; //std::to_string(audioList.size());
        str << "<style>div.x-svg {width: "<< str_size <<"px; }</style>";
        str << "<div class='x-svg'><svg viewBox='0 0 "<< str_size << " 100'> <polyline fill='none' stroke='blue' stroke-width='1' points ='";
        // copy data from input stream
        size_t idx = 0;
        for(int16_t sample: audioList){
            str << idx++ << "," << sample  << " ";
        }
        str << "'/></svg></div>";
    } else {
        str << "<p>Channel " << channel << " of " << channels <<  " does not exist!</p>";
    }
    return str.str();
  }

protected:
  std::stringstream str;
  std::string fname;
  const int wav_header_size = 44;
  int channels=0;
  int channel=0;

  int transform(int x){
    int result = x / 1000; // scale -32 to 32
    result += 60; // shift down
    return result;
  }
};

using Chart = ChartT<int16_t>;

/**
 * @brief Output to Jupyter. We write the data just to a file from where we can
 * load the data again for different representations.
 */
template <typename T> 
class JupyterAudioT : public AudioStream {
public:

  JupyterAudioT(const char* fileName, AudioStream &stream, int bufferCount=20, int bufferSize=1024) {
    buffer_count = bufferCount;
    p_audio_stream = &stream;
    cfg = stream.audioInfo();
    copier.resize(bufferSize);
    fname = fileName;
    if (fileExists()){
        remove(fileName);
    }
  }

  ChartT<T> &chart(int channel=0) {
    createWAVFile();
    assert(cfg.channels>0);
    chrt.setup(fname, cfg.channels, channel);
    return chrt;
  }

  // provide the file name
  const std::string &name() const {
    return fname;
  }

  // provides the absolute file path as string
  const std::string path() const {
    std::filesystem::path p = fname;
    std::string result = std::filesystem::absolute(p);
    return result;
  }

  // fills a wav file with data once, the first time it was requested
  void createWAVFile(){
    try{
      if (!fileExists()){
          std::fstream fstream(fname, fstream.binary | fstream.trunc | fstream.out);
          FileOutput fp(fstream);
          wave_encoder.setAudioInfo(audioInfo());
          out.setOutput(&fp);
          out.setEncoder(&wave_encoder);
          out.begin(); // output to decoder
          copier.begin(out, *p_audio_stream);
          copier.copyN(buffer_count);
          fstream.close();
      }
    } catch(const std::exception& ex){
        std::cerr << ex.what();
    }
  }

  bool fileExists() {
    ifstream f(fname.c_str());
    return f.good();
  }

  int bufferCount(){
    return buffer_count;
  }

  // provides the wav data as bas64 encded string
  std::string audio() {
    std::ifstream fin(fname, std::ios::binary);
    std::stringstream m_buffer;
    m_buffer << fin.rdbuf();
    return xtl::base64encode(m_buffer.str());
  }

  // Provides the audion information
  AudioInfo audioInfo() {
    return cfg;
  }

protected:
  AudioStream *p_audio_stream=nullptr;
  ChartT<T> chrt;
  WAVEncoder wave_encoder;
  EncodedAudioOutput out;
  StreamCopyT<T> copier;
  AudioInfo cfg;
  string fname;
  size_t buffer_count=0;
};

using JupyterAudio = JupyterAudioT<int16_t>;

} // namespace audio_tools

/// Disply Chart in Jupyterlab xeus
nl::json mime_bundle_repr(Chart &in) {
  auto bundle = nl::json::object();
  bundle["text/html"] =  in.chartData();
  return bundle;
}

/// Disply Audio player in Jupyterlab xeus
nl::json mime_bundle_repr(JupyterAudio &in) {
  auto bundle = nl::json::object();
  in.createWAVFile();
  bundle["text/html"] = "<audio controls "
                        "src='data:audio/wav;base64," +
                        in.audio() + "'/>";
  return bundle;
}

