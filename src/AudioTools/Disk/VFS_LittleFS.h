#pragma once
#include <string.h>
#include <sys/stat.h>
#include <sys/unistd.h>

#include "AudioTools/Disk/VFS.h"
#include "esp_err.h"
#include "esp_littlefs.h"
#include "esp_system.h"

namespace audio_tools {

/**
 * @brief ESP32 Virtual File System for SPI SD. The default mount point is "/littlefs"
 * 
 * DRAFT implementation: not tested
 * See
 * https://github.com/espressif/esp-idf/tree/master/examples/storage/sd_card/sdspi
 * @ingroup player
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class VFS_SDSPI : public VFS {
 public:
  VFS_SDSPI() = default;
  void setMountPoint(const char* mp) { mount_point = mp; }
  bool begin() {
    LOGI("Initializing LittleFS");

    esp_vfs_littlefs_conf_t conf = {
        .base_path = mount_point,
        .partition_label = "storage",
        .format_if_mount_failed = format_if_mount_failed,
        .dont_mount = false,
    };

    // Use settings defined above to initialize and mount LittleFS filesystem.
    // Note: esp_vfs_littlefs_register is an all-in-one convenience function.
    esp_err_t ret = esp_vfs_littlefs_register(&conf);

    if (ret != ESP_OK) {
      if (ret == ESP_FAIL) {
        LOGE("Failed to mount or format filesystem");
      } else if (ret == ESP_ERR_NOT_FOUND) {
        LOGE("Failed to find LittleFS partition");
      } else {
        LOGE("Failed to initialize LittleFS (%s)", esp_err_to_name(ret));
      }
      return false;
    }

    size_t total = 0, used = 0;
    ret = esp_littlefs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK) {
      LOGE("Failed to get LittleFS partition information (%s)",
           esp_err_to_name(ret));
      //esp_littlefs_format(conf.partition_label);
      return false;
    } else {
      LOGI("Partition size: total: %d, used: %d", total, used);
    }

    return true;
  }

  void end() {
    // All done, unmount partition and disable SPIFFS
    // All done, unmount partition and disable LittleFS
    esp_vfs_littlefs_unregister(conf.partition_label);
    LOGI("LittleFS unmounted");
  }

 protected:
 esp_vfs_littlefs_conf_t conf ;
  const char* mount_point = "/littlefs";
  int max_files = 5;
  bool format_if_mount_failed = true;
};
}  // namespace audio_tools