#pragma once
#include <string.h>
#include <sys/stat.h>
#include <sys/unistd.h>

#include "AudioTools/Disk/VFS.h"
#include "esp_err.h"
#include "esp_spiffs.h"

namespace audio_tools {

/**
 * @brief ESP32 Virtual File System for SPI SD. The default mount point is "/spiffs"
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
    LOGI("Initializing SPIFFS");

    conf = {.base_path = mount_point,
            .partition_label = NULL,
            .max_files = max_files,
            .format_if_mount_failed = format_if_mount_failed};

    // Use settings defined above to initialize and mount SPIFFS filesystem.
    // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
      if (ret == ESP_FAIL) {
        LOGE("Failed to mount or format filesystem");
      } else if (ret == ESP_ERR_NOT_FOUND) {
        LOGE("Failed to find SPIFFS partition");
      } else {
        LOGE("Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
      }
      return false;
    }

    LOGI("Performing SPIFFS_check().");
    ret = esp_spiffs_check(conf.partition_label);
    if (ret != ESP_OK) {
      LOGE("SPIFFS_check() failed (%s)", esp_err_to_name(ret));
      return false;
    } else {
      LOGI("SPIFFS_check() successful");
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK) {
      LOGE("Failed to get SPIFFS partition information (%s). Formatting...",
           esp_err_to_name(ret));
      //esp_spiffs_format(conf.partition_label);
      return false;
    } else {
      LOGI("Partition size: total: %d, used: %d", total, used);
    }

    // Check consistency of reported partition size info.
    if (used > total) {
      LOGW(
          "Number of used bytes cannot be larger than total. Performing "
          "SPIFFS_check().");
      ret = esp_spiffs_check(conf.partition_label);
      // Could be also used to mend broken files, to clean unreferenced pages,
      // etc. More info at
      // https://github.com/pellepl/spiffs/wiki/FAQ#powerlosses-contd-when-should-i-run-spiffs_check
      if (ret != ESP_OK) {
        LOGE("SPIFFS_check() failed (%s)", esp_err_to_name(ret));
        return false;
      } else {
        LOGI("SPIFFS_check() successful");
      }
    }
    return true;
  }

  void end() {
    // All done, unmount partition and disable SPIFFS
    esp_vfs_spiffs_unregister(conf.partition_label);
    LOGI("SPIFFS unmounted");
  }

 protected:
  esp_vfs_spiffs_conf_t conf;
  const char* mount_point = "/spiffs";
  int max_files = 5;
  bool format_if_mount_failed = true;
};
}  // namespace audio_tools