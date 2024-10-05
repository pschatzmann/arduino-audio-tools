#pragma once
#include "AudioConfig.h"
#include <iostream>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>

#ifdef IS_NOARDUINO
#  define NOARD_OVR override
#else
#  define NOARD_OVR 
#endif


namespace audio_tools {

enum FileMode { FILE_READ='r', FILE_WRITE='w', FILE_APPEND='a'};
enum SeekMode { SeekSet = 0, SeekCur = 1, SeekEnd = 2 };

/**
 * @brief Arduino File support using std::fstream
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class File : public Stream {
  public:
    File() = default;
    File(const char* fn) {
        open(fn, FILE_READ);
    }
    File(const File &file){
        open(file.name(), FILE_READ);
    }

    File& operator =(File file){
        open(file.name(), FILE_READ);
        return *this;
    }

    void open(const char* name, FileMode mode){
        file_path = name;
        switch(mode){
            case FILE_READ:
                stream.open(name, stream.binary | stream.in);
                is_read = true;
                break;
            case FILE_WRITE:
                stream.open(name, stream.binary | stream.trunc | stream.out);
                is_read = false;
                break;
            case FILE_APPEND:
                stream.open(name, stream.binary |  stream.out);
                is_read = false;
                break;
        }
    }
    
    virtual bool begin(){
        // move to beginning
        return seek(0);
    }

    virtual void end() {
        stream.close();
    }
    
    virtual int print(const char* str) NOARD_OVR {
        stream << str;
        return strlen(str);
    }
    
    virtual int println(const char* str="") NOARD_OVR {
        stream << str << "\n";
        return strlen(str)+1;
    }
    
    virtual int print(int number) NOARD_OVR {
        char buffer[80];
        int len = snprintf(buffer,80, "%d", number);
        print(buffer);
        return len;
    }
    
    virtual int println(int number){
        char buffer[80];
        int len = snprintf(buffer,80, "%d\n", number);
        print(buffer);
        return len;
    }
    
    virtual void flush() override {
        stream.flush();
    }
    
    virtual void write(uint8_t* str, int len) {
        stream.write((const char*)str, len);
    }
    
    virtual size_t write(int32_t value){
        stream.put(value);
        return 1;
    }
    
    virtual size_t write(uint8_t value)override{
        stream.put(value);    
        return 1;
    }
    
    virtual int available() override{
        int result = size()-position();
        return result;
    };
    
    virtual int read() override{
        return stream.get();
    } 

    virtual size_t readBytes(uint8_t* data, size_t len) override {
         stream.read((char*)data, len);
         return stream?len : stream.gcount();
    } 

    virtual int peek() override {
        return stream.peek();
    }
   
    bool seek(uint32_t pos, SeekMode mode){
        if (is_read){
            switch(mode){
                case SeekSet:
                    stream.seekg(pos, std::ios_base::beg);
                    break;
                case SeekCur:
                    stream.seekg(pos, std::ios_base::cur);
                    break;
                case SeekEnd:
                    stream.seekg(pos, std::ios_base::end);
                    break;
            }
        } else {
            switch(mode){
                case SeekSet:
                    stream.seekp(pos, std::ios_base::beg);
                    break;
                case SeekCur:
                    stream.seekp(pos, std::ios_base::cur);
                    break;
                case SeekEnd:
                    stream.seekp(pos, std::ios_base::end);
                    break;
            }
        }
        return stream.fail()==false;
    }

    bool seek(uint32_t pos){
        return seek(pos, SeekSet);
    }

    size_t position() {
        return stream.tellp();
    }

    size_t size() const {
        std::streampos fsize = 0;
        std::ifstream file( file_path, std::ios::binary );

        fsize = file.tellg();
        file.seekg( 0, std::ios::end );
        fsize = file.tellg() - fsize;
        file.close();

        return fsize;
    }

    void close() {
        stream.close();
    }

    const char* name() const {
        return file_path;
    }

    operator bool() {
        return stream.is_open();
    }

  protected:
    std::fstream stream;
    bool is_read=true;
    const char* file_path=nullptr;
};

/**
 * @brief Eumlate FS using C++ or Posix functions
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class FS {
public:
    File open(const char* path, FileMode mode = FILE_READ){
        File file;
        file.open(path, mode);
        return file;
    }
    File open(const std::string &path, FileMode mode = FILE_READ){
        const char* path_str = path.c_str(); 
        return this->open(path_str, mode);
    }
    bool exists(const char* path){
        struct stat buffer;   
        return (stat (path, &buffer) == 0); 
    }
    bool exists(const std::string& path){
        return exists(path.c_str());
    }
    bool remove(const char* path) {
        return ::remove(path)==0; 
    }
    bool remove(const std::string& path){
        return remove(path.c_str());
    }
    bool rename(const char* pathFrom, const char* pathTo) {
        return ::rename(pathFrom, pathTo) == 0;
    }
    bool rename(const std::string& pathFrom, const std::string& pathTo){
        return rename(pathFrom.c_str(), pathTo.c_str());
    }
    bool mkdir(const char *path) {
        return ::mkdir(path, 0777)==0;
    }
    bool mkdir(const std::string &path){
        return mkdir(path.c_str());
    }
    bool rmdir(const char *path){
        return ::rmdir(path)==0;
    }
    bool rmdir(const std::string &path){
        return rmdir(path.c_str());
    }
};

static FS SD;
static FS SDFAT;
    
}
