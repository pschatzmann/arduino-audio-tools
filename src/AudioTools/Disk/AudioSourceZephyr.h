#pragma once

#ifndef IS_ZEPHYR
#  error("ZephyrSD only supported by zephyr")
#endif

#include "AudioLogger.h"
#include "AudioTools/Disk/AudioSource.h"
#include "AudioTools/CoreAudio/AudioBasic/StrView.h"
#include "ZephyrFile.h"

#include <zephyr/fs/fs.h>

namespace audio_tools {

/**
 * @brief AudioSource using Zephyr FS API (NO mounting, only uses mounted FS).
 * Mount using the ZephyrSD class.
 * @ingroup player
 * @note only for ESP32
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AudioSourceZephyr : public AudioSource {
public:

    AudioSourceZephyr(const char *mountPoint = "/SD",
                      const char *ext = ".mp3") {

        mount_point = mountPoint ? mountPoint : "/";
        start_path  = mount_point;
        extension   = ext ? ext : "";

        timeout_auto_next_value = 600000;
    }

    ~AudioSourceZephyr() {
        end();
    }

    // ------------------------------------------------------------
    // lifecycle
    // ------------------------------------------------------------

    bool begin() override {
        TRACED();

        if (!isMounted()) {
            LOGE("Filesystem not mounted: %s", mount_point);
            return false;
        }

        idx_pos = 0;
        count = 0;
        return true;
    }

    void end() override {
        closeFile();
    }

    // ------------------------------------------------------------
    // playback control
    // ------------------------------------------------------------

    Stream *nextStream(int offset = 1) override {
        LOGI("nextStream: %d", offset);

        Stream *s = selectStream(idx_pos + offset);

        if (!s && offset > 0) {
            LOGI("wrap to start");
            idx_pos = 0;
            s = selectStream(idx_pos);
        }

        return s;
    }

    Stream *selectStream(int index) override {

        long cnt = size();

        if (cnt <= 0) {
            LOGW("No audio files in: %s", start_path);
            return nullptr;
        }

        int norm = index % cnt;
        if (norm < 0) norm += cnt;

        idx_pos = norm;

        const char *path = get(norm);
        if (!path) {
            LOGW("Invalid index: %d", norm);
            return nullptr;
        }

        LOGI("Playing: %s", path);

        closeFile();
        return openFile(path);
    }

    Stream *selectStream(const char *path) override {
        LOGI("selectStream(path): %s", path);

        closeFile();
        return openFile(path);
    }

    // ------------------------------------------------------------
    // configuration
    // ------------------------------------------------------------

    void setFileFilter(const char *filter) {
        file_name_pattern = filter ? filter : "*";
    }

    void setPath(const char *p) {
        start_path = p ? p : "/";
    }

    int index() const {
        return idx_pos;
    }

    const char *toStr() const {
        return current_path[0] ? current_path : nullptr;
    }

    bool isAutoNext() override {
        return true;
    }

    // ------------------------------------------------------------
    // file enumeration
    // ------------------------------------------------------------

    long size(bool force_rescan = false) {

        if (force_rescan) {
            count = 0;
        }

        if (count == 0) {
            scanCount(start_path);
        }

        return count;
    }

protected:

    // ------------------------------------------------------------
    // state
    // ------------------------------------------------------------

    ZephyrFile file_stream;

    size_t idx_pos = 0;
    long count = 0;

    char current_path[CONFIG_FS_FATFS_MAX_LFN + 1] = {};

    const char *extension = ".mp3";
    const char *start_path = "/";
    const char *file_name_pattern = "*";
    const char *mount_point = "/SD";

    // ------------------------------------------------------------
    // mount check ONLY (no ownership)
    // ------------------------------------------------------------

    bool isMounted() const {
        struct fs_dir_t dir;
        fs_dir_t_init(&dir);
        return fs_opendir(&dir, mount_point) == 0;
    }

    // ------------------------------------------------------------
    // file handling
    // ------------------------------------------------------------

    void closeFile() {
        file_stream.close();
        current_path[0] = '\0';
    }

    Stream *openFile(const char *path) {
        if (!path) return nullptr;

        closeFile();

        strncpy(current_path, path, sizeof(current_path) - 1);
        current_path[sizeof(current_path) - 1] = '\0';

        if (file_stream.open(current_path, FS_O_READ)) {
            return &file_stream;
        }

        LOGE("Failed to open: %s", current_path);
        return nullptr;
    }

    // ------------------------------------------------------------
    // directory traversal
    // ------------------------------------------------------------

    const char *get(int idx) {
        int running = 0;
        return scanForIndex(start_path, idx, running);
    }

    const char *scanForIndex(const char *path,
                             int target,
                             int &running) {

        struct fs_dir_t dir;
        fs_dir_t_init(&dir);

        if (fs_opendir(&dir, path) != 0) {
            return nullptr;
        }

        struct fs_dirent entry;
        const char *result = nullptr;

        while (true) {
            int rc = fs_readdir(&dir, &entry);
            if (rc != 0 || entry.name[0] == '\0') break;

            char full[CONFIG_FS_FATFS_MAX_LFN + 2];

            if (snprintf(full, sizeof(full),
                         "%s/%s", path, entry.name) >= (int)sizeof(full)) {
                continue;
            }

            if (entry.type == FS_DIR_ENTRY_DIR) {
                result = scanForIndex(full, target, running);
                if (result) break;
            }
            else if (isValidAudioFile(entry.name)) {

                if (running == target) {
                    strncpy(current_path, full,
                            sizeof(current_path) - 1);
                    current_path[sizeof(current_path) - 1] = '\0';

                    result = current_path;
                    break;
                }

                running++;
            }
        }

        fs_closedir(&dir);
        return result;
    }

    void scanCount(const char *path) {

        struct fs_dir_t dir;
        fs_dir_t_init(&dir);

        if (fs_opendir(&dir, path) != 0) {
            return;
        }

        struct fs_dirent entry;

        while (true) {
            int rc = fs_readdir(&dir, &entry);
            if (rc != 0 || entry.name[0] == '\0') break;

            char full[CONFIG_FS_FATFS_MAX_LFN + 2];

            if (snprintf(full, sizeof(full),
                         "%s/%s", path, entry.name) >= (int)sizeof(full)) {
                continue;
            }

            if (entry.type == FS_DIR_ENTRY_DIR) {
                scanCount(full);
            }
            else if (isValidAudioFile(entry.name)) {
                count++;
            }
        }

        fs_closedir(&dir);
    }

    // ------------------------------------------------------------
    // filtering
    // ------------------------------------------------------------

    bool isValidAudioFile(const char *name) {

        if (!name || !extension) return false;

        StrView fname(name);

        return fname.endsWithIgnoreCase(extension)
            && fname.matches(file_name_pattern);
    }
};

} // namespace audio_tools
