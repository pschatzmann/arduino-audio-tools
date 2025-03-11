#pragma once
#include <string.h>

#include "AudioTools/Disk/VFS.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#ifdef SOC_SDMMC_IO_POWER_EXTERNAL
#include "sd_pwr_ctrl_by_on_chip_ldo.h"
#endif

#if !defined(DEFAULT_CS)
#if defined(AUDIOBOARD_SD)
#define DEFAULT_CS 13
#define DEFAULT_MOSI 15
#define DEFAULT_MISO 2
#define DEFAULT_CLK 14
#else
#define DEFAULT_CS SS
#define DEFAULT_MOSI MOSI
#define DEFAULT_MISO MISO
#define DEFAULT_CLK SCK
#endif
#endif

#ifndef DEFAULT_MAX_TRANSFER_SIZE
#define DEFAULT_MAX_TRANSFER_SIZE 4000
#endif

namespace audio_tools {

/**
 * @brief ESP32 Virtual File System for SPI SD. The default mount point is "/sdcard"
 * 
 * DRAFT implementation: not tested
 * See https://github.com/espressif/esp-idf/tree/master/examples/storage/sd_card/sdspi
 * @ingroup player
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class VFS_SDSPI : public VFS {
 public:
  VFS_SDSPI() = default;
  VFS_SDSPI(int CS, int MOSI, int MISO, int SCK) {
    setPins(CS, MOSI, MISO, SCK);
  }
  void setPins(int CS, int MOSI, int MISO, int SCK) {
    setCS(CS);
    setMosi(MOSI);
    setMiso(MISO);
    setClk(SCK);
  }
  void setCS(int pin) { pin_cs = (gpio_num_t)pin; }
  void setMosi(int pin) { pin_mosi = (gpio_num_t)pin; }
  void setMiso(int pin) { pin_miso = (gpio_num_t)pin; }
  void setClk(int pin) { pin_clk = (gpio_num_t)pin; }
  void setMountPoint(const char* mp) { mount_point = mp; }
  bool begin() {
    esp_err_t ret;

    // Options for mounting the filesystem.
    // If format_if_mount_failed is set to true, SD card will be partitioned and
    // formatted in case when mounting fails.
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024};
    LOGI("Initializing SD card");

    // Use settings defined above to initialize SD card and mount FAT
    // filesystem. Note: esp_vfs_fat_sdmmc/sdspi_mount is all-in-one convenience
    // functions. Please check its source code and implement error recovery when
    // developing production applications.
    LOGI("Using SPI peripheral");

    // By default, SD card frequency is initialized to SDMMC_FREQ_DEFAULT
    // (20MHz) For setting a specific frequency, use host.max_freq_khz (range
    // 400kHz - 20MHz for SDSPI) Example: for fixed frequency of 10MHz, use
    // host.max_freq_khz = 10000;
    host = SDSPI_HOST_DEFAULT();

    // For SoCs where the SD power can be supplied both via an internal or
    // external (e.g. on-board LDO) power supply. When using specific IO pins
    // (which can be used for ultra high-speed SDMMC) to connect to the SD card
    // and the internal LDO power supply, we need to initialize the power supply
    // first.
#ifdef CONFIG_SD_PWR_CTRL_LDO_IO_ID

    sd_pwr_ctrl_ldo_config_t ldo_config = {
        .ldo_chan_id = CONFIG_SD_PWR_CTRL_LDO_IO_ID,
    };
    pwr_ctrl_handle = NULL;

    ret = sd_pwr_ctrl_new_on_chip_ldo(&ldo_config, &pwr_ctrl_handle);
    if (ret != ESP_OK) {
      LOGE("Failed to create a new on-chip LDO power control driver");
      return false;
    }
    host.pwr_ctrl_handle = pwr_ctrl_handle;
#endif

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = pin_mosi,
        .miso_io_num = pin_miso,
        .sclk_io_num = pin_clk,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = max_transfer_sz,
    };

    ret = spi_bus_initialize((spi_host_device_t)host.slot, &bus_cfg,
                             SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
      LOGE("Failed to initialize bus.");
      return false;
    }

    // This initializes the slot without card detect (CD) and write protect (WP)
    // signals. Modify slot_config.gpio_cd and slot_config.gpio_wp if your board
    // has these signals.
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = pin_cs;
    slot_config.host_id = (spi_host_device_t)host.slot;

    LOGI("Mounting filesystem");
    ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config,
                                  &mount_config, &card);

    if (ret != ESP_OK) {
      if (ret == ESP_FAIL) {
        LOGE("Failed to mount filesystem");
      } else {
        LOGE("Failed to initialize the card (%s)", esp_err_to_name(ret));
      }
      return false;
    }
    LOGI("Filesystem mounted");

    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, card);
    return true;
  }

  void end() {
    if (card == nullptr) return;
    // All done, unmount partition and disable SPI peripheral
    esp_vfs_fat_sdcard_unmount(mount_point, card);
    card = nullptr;

    LOGI("Card unmounted");

    // deinitialize the bus after all devices are removed
    spi_bus_free((spi_host_device_t)host.slot);

    // Deinitialize the power control driver if it was used
#ifdef CONFIG_SD_PWR_CTRL_LDO_IO_ID

    ret = sd_pwr_ctrl_del_on_chip_ldo(pwr_ctrl_handle);
    if (ret != ESP_OK) {
      LOGE("Failed to delete the on-chip LDO power control driver");
      return;
    }
#endif
  }

 protected:
  sdmmc_card_t* card = nullptr;
  sd_pwr_ctrl_handle_t pwr_ctrl_handle;
  sdmmc_host_t host;
  const char* mount_point = "/sdcard";
  gpio_num_t pin_cs = (gpio_num_t)DEFAULT_CS;
  gpio_num_t pin_mosi = (gpio_num_t)DEFAULT_MOSI;
  gpio_num_t pin_miso = (gpio_num_t)DEFAULT_MISO;
  gpio_num_t pin_clk = (gpio_num_t)DEFAULT_CLK;
  int max_transfer_sz = DEFAULT_MAX_TRANSFER_SIZE;
};
}  // namespace audio_tools