#pragma once
#include "AudioTools/CoreAudio/AudioBasic/Str.h"
#include "AudioTools/CoreAudio/AudioMetaData/AbstractMetaData.h"

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

}