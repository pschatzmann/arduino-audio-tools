#pragma once

#ifdef ESP32
#undef MIN
#define SOC_SDMMC_USE_GPIO_MATRIX

#include <string.h>
#include <sys/stat.h>
#include <sys/unistd.h>

#include "driver/sdmmc_host.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

static const char* TAG = "ESP32_SD";

struct SDMMCConfig {
  SDMMCConfig() {
    ESP_LOGI(TAG, "Initializing SD card");
    slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 4;

    // // On chips where the GPIOs used for SD card can be configured, set them
    // in
    // // the slot_config structure:
    // slot_config.clk = GPIO_NUM_14;
    // slot_config.cmd = GPIO_NUM_15;
    // slot_config.d0 = GPIO_NUM_2;
    // slot_config.d1 = GPIO_NUM_4;
    // slot_config.d2 = GPIO_NUM_12;
    // slot_config.d3 = GPIO_NUM_13;

    // Enable internal pullups on enabled pins. The internal pullups
    // are insufficient however, please make sure 10k external pullups are
    // connected on the bus. This is for debug / example purpose only.
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    mount_config.format_if_mount_failed = false;
    mount_config.max_files = 5;
    mount_config.allocation_unit_size = 16 * 1024;

    host = SDMMC_HOST_DEFAULT();
    mount_point = (char*)"/sdcard";
  }

  // Options for mounting the filesystem.
  // If format_if_mount_failed is set to true, SD card will be partitioned and
  // formatted in case when mounting fails.
  esp_vfs_fat_sdmmc_mount_config_t mount_config;

  // This initializes the slot without card detect (CD) and write protect (WP)
  // signals. Modify slot_config.gpio_cd and slot_config.gpio_wp if your board
  // has these signals.
  sdmmc_slot_config_t slot_config;
  sdmmc_host_t host;
  char* mount_point = nullptr;
};

/**
 * @brief Setup of SDCard on ESP32 to prepare file access
 *
 */
class SDMMCCard {
 public:
  SDMMCCard() = default;

  SDMMCConfig defaultConfig() {
    SDMMCConfig cfg;
    return cfg;
  }

  /**
   * Use settings defined in SDConfig to initialize SD card and mount FAT
   * filesystem. Note: esp_vfs_fat_sdmmc/sdspi_mount is all-in-one convenience
   * functions. Please check its source code and implement error recovery when
   * developing production applications.
   **/

  bool begin(SDMMCConfig cfg) {
    this->cfg = cfg;
    esp_err_t ret;

    ESP_LOGI(TAG, "Mounting filesystem");
    ret = esp_vfs_fat_sdmmc_mount(cfg.mount_point, &cfg.host, &cfg.slot_config,
                                  &cfg.mount_config, &card);
    if (ret == ESP_OK) {
      ESP_LOGI(TAG, "Filesystem mounted: %s", cfg.mount_point);
      sdmmc_card_print_info(stdout, card);
    } else {
      if (ret == ESP_FAIL) {
        ESP_LOGE(TAG,
                 "Failed to mount filesystem. "
                 "If you want the card to be formatted, set the "
                 "EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
      } else {
        ESP_LOGE(TAG,
                 "Failed to initialize the card (%s). "
                 "Make sure SD card lines have pull-up resistors in place.",
                 esp_err_to_name(ret));
      }
    }
    return ret == ESP_OK;
  }

  void end() {
    // esp_vfs_fat_sdcard_unmount(cfg.mount_point, card);
    // ESP_LOGI(TAG, "Card unmounted");
  }

 protected:
  sdmmc_card_t* card;
  SDMMCConfig cfg;
};

#endif