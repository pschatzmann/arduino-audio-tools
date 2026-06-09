#pragma once

#ifndef IS_ZEPHYR
#  error("ZephyrSD only supported by zephyr")
#endif

#include "AudioTools/CoreAudio/AudioRuntime.h"
#include <zephyr/fs/fs.h>
#include <zephyr/storage/disk_access.h>

namespace audio_tools {

/**
 * @brief Arduino File API for Zephyr
 * @ingroup io
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class ZephyrFile : public Stream {
public:
    ZephyrFile() {
        fs_file_t_init(&f);
    }

    ~ZephyrFile() {
        close();
    }

    // non-copyable
    ZephyrFile(const ZephyrFile&) = delete;
    ZephyrFile& operator=(const ZephyrFile&) = delete;

    // movable
    ZephyrFile(ZephyrFile&& other) noexcept {
        moveFrom(other);
    }

    ZephyrFile& operator=(ZephyrFile&& other) noexcept {
        if (this != &other) {
            close();
            moveFrom(other);
        }
        return *this;
    }

    bool open(const char *file_path, fs_mode_t mode = FS_O_READ) {
        close();

        fs_file_t_init(&f);

        int rc = fs_open(&f, file_path, mode);
        if (rc != 0) {
            LOGE("fs_open(%s) failed: %d", file_path, rc);
            return false;
        }

        is_open = true;
        path = file_path;

        updateSize();

        if (mode & FS_O_APPEND) {
            fs_seek(&f, 0, FS_SEEK_END);
        }

        return true;
    }

    void close() {
        if (!is_open) {
            return;
        }

        fs_close(&f);

        fs_file_t_init(&f);

        is_open = false;
        path = nullptr;
        file_size = 0;
    }

    bool isOpen() const {
        return is_open;
    }

    operator bool() const {
        return is_open;
    }

    //--------------------------------------------------------------------
    // Stream API
    //--------------------------------------------------------------------

    int available() override {
        if (!is_open) {
            return 0;
        }

        size_t pos = position();

        if (pos >= file_size) {
            return 0;
        }

        return static_cast<int>(file_size - pos);
    }

    int read() override {
        if (!is_open) {
            return -1;
        }

        uint8_t value;

        ssize_t rc = fs_read(&f, &value, 1);

        if (rc != 1) {
            return -1;
        }

        return value;
    }

    size_t readBytes(char *buffer, size_t len)  {
        if (!is_open || buffer == nullptr || len == 0) {
            return 0;
        }

        ssize_t rc = fs_read(&f, buffer, len);

        return rc > 0 ? static_cast<size_t>(rc) : 0;
    }

    size_t write(uint8_t value) override {
        return write(&value, 1);
    }

    size_t write(const uint8_t *buffer, size_t len) override {
        if (!is_open || buffer == nullptr || len == 0) {
            return 0;
        }

        ssize_t rc = fs_write(&f, buffer, len);

        if (rc <= 0) {
            return 0;
        }

        size_t written = static_cast<size_t>(rc);

        size_t pos = position();
        if (pos > file_size) {
            file_size = pos;
        }

        return written;
    }

    void flush() override {
        if (is_open) {
            fs_sync(&f);
        }
    }

    int peek() override {
        if (!is_open) {
            return -1;
        }

        uint8_t value;

        ssize_t rc = fs_read(&f, &value, 1);

        if (rc != 1) {
            return -1;
        }

        fs_seek(&f, -1, FS_SEEK_CUR);

        return value;
    }

    //--------------------------------------------------------------------
    // File API
    //--------------------------------------------------------------------

    bool seek(size_t pos) {
        if (!is_open) {
            return false;
        }

        return fs_seek(&f, static_cast<off_t>(pos), FS_SEEK_SET) == 0;
    }

    size_t position() const {
        if (!is_open) {
            return 0;
        }

        off_t pos = fs_tell(const_cast<fs_file_t*>(&f));

        return pos >= 0 ? static_cast<size_t>(pos) : 0;
    }

    size_t size() const {
        return file_size;
    }

    const char* name() const {
        return path;
    }

private:
    fs_file_t f{};
    bool is_open = false;
    const char* path = nullptr;
    size_t file_size = 0;

    void updateSize() {
        file_size = 0;

        if (path == nullptr) {
            return;
        }

        struct fs_dirent entry;

        if (fs_stat(path, &entry) == 0) {
            file_size = entry.size;
        }
    }

    void moveFrom(ZephyrFile& other) {
        f = other.f;
        is_open = other.is_open;
        path = other.path;
        file_size = other.file_size;

        fs_file_t_init(&other.f);
        other.is_open = false;
        other.path = nullptr;
        other.file_size = 0;
    }
};

using File = ZephyrFile;

} // namespace audio_tools
