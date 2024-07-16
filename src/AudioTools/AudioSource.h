#pragma once

namespace audio_tools {

/**
 * @brief Abstract Audio Data Source for the AudioPlayer which is used by the Audio Players
 * @ingroup player
 * @author Phil Schatzmann
 * @copyright GPLv3
 *
 */
class AudioSource {
public:
    /// Reset actual stream and move to root
    virtual void begin() = 0;

    /// Returns next audio stream
    virtual Stream* nextStream(int offset) = 0;

    /// Returns previous audio stream
    virtual Stream* previousStream(int offset) {
        return nextStream(-offset);
    };

    /// Returns audio stream at the indicated index (the index is zero based, so the first value is 0!)
    virtual Stream* selectStream(int index) {
        LOGE("Not Supported!");
        return nullptr;
    }

    /// same as selectStream - I just prefer this name
    virtual Stream* setIndex(int index) {
        return selectStream(index);
    }

    /// Returns audio stream by path
    virtual Stream* selectStream(const char* path) = 0;

    /// Sets the timeout which is triggering to move to the next stream. - the default value is 500 ms
    virtual void setTimeoutAutoNext(int millisec) {
        timeout_auto_next_value = millisec;
    }

    /// Provides the timeout which is triggering to move to the next stream.
    virtual int timeoutAutoNext() {
        return timeout_auto_next_value;
    }

    // only the ICYStream supports this
    virtual bool setMetadataCallback(void (*fn)(MetaDataType info, const char* str, int len), ID3TypeSelection sel=SELECT_ICY) {
        return false;
    }

    /// Sets the timeout of Stream in milliseconds
    virtual void setTimeout(int millisec) {};

    /// Returns default setting go to the next
    virtual bool isAutoNext() {return true; }

    /// access with array syntax
    Stream* operator[](int idx){
        return setIndex(idx);
    }


protected:
    int timeout_auto_next_value = 500;
};

/**
 * @brief Callback Audio Data Source which is used by the Audio Players
 * @ingroup player
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AudioSourceCallback : public AudioSource {
public:
    AudioSourceCallback() {
    }

    AudioSourceCallback(Stream* (*nextStreamCallback)(int offset), void (*onStartCallback)() = nullptr) {
        TRACED();
        this->onStartCallback = onStartCallback;
        this->nextStreamCallback = nextStreamCallback;
    }

    /// Reset actual stream and move to root
    virtual void begin() override {
        TRACED();
        if (onStartCallback != nullptr) onStartCallback();
    };

    /// Returns next (with positive index) or previous stream (with negative index)
    virtual Stream* nextStream(int offset) override {
        TRACED();
        return nextStreamCallback == nullptr ? nullptr : nextStreamCallback(offset);
    }

    /// Returns selected audio stream
    virtual Stream* selectStream(int index) override {
        LOGI("selectStream: %d", index);
        if (indexStreamCallback==nullptr){
            LOGI("setCallbackSelectStream not provided");
            if (index>0) {
                begin();
                return nextStream(index);
            } else {
                // nextStream(0) will return the directory but we need a file
                return nextStream(1);
            }
        }
        return indexStreamCallback(index);
    }
    /// Returns audio stream by path
    virtual Stream* selectStream(const char* path) override {
        this->path = path;
        return indexStreamCallback == nullptr ? nullptr : indexStreamCallback(-1);
    };

    void setCallbackOnStart(void (*callback)()) {
        onStartCallback = callback;
    }

    void setCallbackNextStream(Stream* (*callback)(int offset)) {
        nextStreamCallback = callback;
    }

    void setCallbackSelectStream(Stream* (*callback)(int idx)) {
        indexStreamCallback = callback;
    }

    virtual bool isAutoNext() override {
        return auto_next;
    }

    virtual void setAutoNext(bool a){
        auto_next = a;
    }

    // returns the requested path: relevant when provided idx in callback is -1
    virtual const char* getPath() {
        return path;
    }

protected:
    void (*onStartCallback)() = nullptr;
    bool auto_next = true;
    Stream* (*nextStreamCallback)(int offset) = nullptr;
    Stream* (*indexStreamCallback)(int index) = nullptr;
    const char*path=nullptr;
};

#if defined(USE_URL_ARDUINO) && ( defined(ESP32) || defined(ESP8266) )

/**
 * @brief Audio Source which provides the data via the network from an URL
 * @ingroup player
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AudioSourceURL : public AudioSource {
public:
    template<typename T, size_t N>
    AudioSourceURL(AbstractURLStream& urlStream, T(&urlArray)[N], const char* mime, int startPos = 0) {
        TRACED();
        this->actual_stream = &urlStream;
        this->mime = mime;
        this->urlArray = urlArray;
        this->max = N;
        this->pos = startPos - 1;
        this->timeout_auto_next_value = 20000;
    }

    /// Setup Wifi URL
    virtual void begin() override {
        TRACED();
        this->pos = 0;
    }

    /// Opens the selected url from the array
    Stream* selectStream(int idx) override {
        pos = idx;
        if (pos < 0) {
            pos = 0;
            LOGI("url array out of limits: %d -> %d", idx, pos);
        }
        if (pos >= size()) {
            pos = size() - 1;
            LOGI("url array out of limits: %d -> %d", idx, pos);
        }
        LOGI("selectStream: %d/%d -> %s", pos, size() - 1, value(pos));
        if (started) actual_stream->end();
        actual_stream->begin(value(pos), mime);
        started = true;
        return actual_stream;
    }

    /// Opens the next url from the array
    Stream* nextStream(int offset) override {
        pos += offset;
        if (pos < 0 || pos >= size()) {
            pos = 0;
        }
        LOGI("nextStream: %d/%d -> %s", pos, max-1, value(pos));
        return selectStream(pos);
    }

    /// Opens the Previous url from the array
    Stream* previousStream(int offset) override {
        pos -= offset;
        if (pos < 0 || pos >= size()) {
            pos = size() - 1;
        }
        LOGI("previousStream: %d/%d -> %s", pos, size() - 1, value(pos));
        return selectStream(pos);
    }

    /// Opens the selected url
    Stream* selectStream(const char* path) override {
        LOGI("selectStream: %s", path);
        if (started) actual_stream->end();
        actual_stream->begin(path, mime);
        started = true;
        return actual_stream;
    }

    int index() {
        return pos;
    }

    const char *toStr() {
        return value(pos);
    }

    /// Sets the timeout of the URL Stream in milliseconds
    void setTimeout(int millisec){
        actual_stream->setTimeout(millisec);
    }

    // provides go not to the next on error
    virtual bool isAutoNext() {
        return true;
    };

    // only the ICYStream supports this
    bool setMetadataCallback(void (*fn)(MetaDataType info, const char* str, int len), ID3TypeSelection sel=SELECT_ICY) {
        TRACEI();
        return actual_stream->setMetadataCallback(fn);
    }


protected:
    AbstractURLStream* actual_stream = nullptr;
    const char** urlArray;
    int pos = 0;
    int max = 0;
    const char* mime = nullptr;
    bool started = false;

    /// allow use with empty constructor in subclasses
    AudioSourceURL() = default;

    virtual const char* value(int pos){
        return urlArray[pos];
    }

    virtual int size(){
        return max;
    }

};

/**
 * @brief Audio Source which provides the data via the network from an URL.
 * The URLs are stored in an Vector of dynamically allocated strings
 * @ingroup player
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class AudioSourceDynamicURL : public AudioSourceURL {
public:
    template<typename T, size_t N>
    AudioSourceDynamicURL(AbstractURLStream& urlStream, T(&urlArray)[N], const char* mime, int startPos = 0) {
        this->actual_stream = &urlStream;
        this->mime = mime;
        this->pos = startPos - 1;
        this->timeout_auto_next_value = 20000;   
        for (int j=0;j<N;j++){
            addURL(urlArray[j]);
        }    
    }

    AudioSourceDynamicURL(AbstractURLStream& urlStream, const char* mime, int startPos = 0) {
        this->actual_stream = &urlStream;
        this->mime = mime;
        this->pos = startPos - 1;
        this->timeout_auto_next_value = 20000;   
    }

    /// add a new url: a copy of the string will be stored on the heap
    void addURL(const char* url){
        url_vector.push_back(url);
    }

    void clear(){
        url_vector.clear();
    }

protected:
    Vector<StrExt> url_vector;

    const char* value(int pos) override {
        return url_vector[pos].c_str();
    }

    int size() override {
        return url_vector.size();
    }
 
};


#endif

}