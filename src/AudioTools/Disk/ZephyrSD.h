#pragma once

#ifndef IS_ZEPHYR
#  error("ZephyrSD only supported by zephyr")
#endif

#include <zephyr/fs/fs.h>
#include <zephyr/storage/disk_access.h>
#include <string>
#include "ZephyrFile.h"

namespace audio_tools {
/**
 * @brief Arduino SD API for Zephyr
 * @ingroup io
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class ZephyrSDClass {
public:
    ZephyrSDClass() = default;
    ~ZephyrSDClass() {
        unmountSD();
    }

    bool begin(const char* diskName="SD", const char* mountPoint="/SD" ) {
        mp.mnt_point = mountPoint;
        return begin(diskName, mp);
    }

    bool begin(const char* diskName="SD", fs_mount_t moutInfo){
        disk_name = diskName ? diskName : "";
        mp = mountInfo;
        return mountSD();
    }

    void end() {
        unmountSD();
    }

    bool isMounted() const {
        return mounted;
    }

    ZephyrFile open(const char *path, const char *mode = "r", bool create = false) {
        if (!mounted) {
            LOGE("SD not mounted");
            return ZephyrFile();
        }

        fs_mode_t fs_mode = parseMode(mode, create);

        ZephyrFile file;
        file.open(path, fs_mode);
        return file;
    }

    ZephyrFile open(const char *path, fs_mode_t mode) {
        if (!mounted) {
            LOGE("SD not mounted");
            return ZephyrFile();
        }

        ZephyrFile file;
        file.open(path, mode);
        return file;
    }

    bool exists(const char *path) const {
        struct fs_dirent entry;
        return fs_stat(path, &entry) == 0;
    }

    bool remove(const char *path) {
        int ret = fs_unlink(path);
        return logErr(ret, "fs_unlink", path);
    }

    bool rename(const char *from, const char *to) {
        int ret = fs_rename(from, to);
        return logErr(ret, "fs_rename", from, to);
    }

    bool mkdir(const char *path) {
        int ret = fs_mkdir(path);
        return logErr(ret, "fs_mkdir", path);
    }

    bool rmdir(const char *path) {
        int ret = fs_rmdir(path);
        return logErr(ret, "fs_rmdir", path);
    }

    size_t fileSize(const char *path) const {
        struct fs_dirent entry;
        if (fs_stat(path, &entry) == 0) {
            return entry.size;
        }
        return 0;
    }

    bool isDirectory(const char *path) {
        struct fs_dirent entry;

        int rc = fs_stat(path, &entry);
        if (rc != 0) {
            return false; // doesn't exist or not accessible
        }

        return (entry.type == FS_DIR_ENTRY_DIR);
    }

    const char* mountPoint() {
        return mp.mnt_point;
    }

    fs_mount_t &mountInfo(){ return mp;}

protected:
    std::string disk_name;
    bool mounted = false;
    fs_mount_t mp = {
        .type = FS_FATFS,
        .mnt_point = "/SD",
        .fs_data = nullptr
    };

    fs_mode_t parseMode(const char *mode, bool create) {
        fs_mode_t fs_mode = FS_O_READ;

        if (!mode) return FS_O_READ;

        if (mode[0] == 'r') {
            fs_mode = FS_O_READ;
        }
        else if (mode[0] == 'w') {
            fs_mode = FS_O_WRITE | FS_O_TRUNC;
        }
        else if (mode[0] == 'a') {
            fs_mode = FS_O_WRITE | FS_O_APPEND;
        }

        if (create) {
            fs_mode |= FS_O_CREATE;
        }

        return fs_mode;
    }

    bool mountSD() {
        int ret = disk_access_init(disk_name.c_str());
        if (ret != 0 && ret != -EALREADY) {
            LOGE("disk_access_init(%s) failed: %d", disk_name.c_str(), ret);
            return false;
        }

        uint32_t sector_count = 0;
        uint32_t sector_size  = 0;

        disk_access_ioctl(disk_name.c_str(), DISK_IOCTL_GET_SECTOR_COUNT, &sector_count);
        disk_access_ioctl(disk_name.c_str(), DISK_IOCTL_GET_SECTOR_SIZE, &sector_size);

        if (sector_count && sector_size) {
            uint64_t mb = ((uint64_t)sector_count * sector_size) >> 20;
            LOGI("SD: %llu MiB", (unsigned long long)mb);
        }

        int ret2 = fs_mount(&mp);
        if (ret2 != 0) {
            LOGE("fs_mount failed: %d", ret2);
            return false;
        }

        mounted = true;
        LOGI("SD mounted at %s", mp.mnt_point);
        return true;
    }

    void unmountSD() {
        if (!mounted) return;

        int ret = fs_unmount(&mp);
        if (ret != 0) {
            LOGE("fs_unmount failed: %d", ret);
        }

        mounted = false;
    }

    bool logErr(int ret, const char *op, const char *path) {
        if (ret != 0) {
            LOGE("%s(%s) failed: %d", op, path, ret);
            return false;
        }
        return true;
    }

    bool logErr(int ret, const char *op, const char *a, const char *b) {
        if (ret != 0) {
            LOGE("%s(%s -> %s) failed: %d", op, a, b, ret);
            return false;
        }
        return true;
    }

};

/// Access to SD object
static ZephyrSDClass SD;

} // namespace audio_tools
