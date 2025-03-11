#pragma once
#include <string.h>

#include "AudioTools/Disk/VFS.h"
#include "driver/sdmmc_host.h"
#include "esp_vfs_fat.h"
#include "sd_protocol_types.h"
#include "sdmmc_cmd.h"

#ifdef SOC_SDMMC_IO_POWER_EXTERNAL
#include "sd_pwr_ctrl_by_on_chip_ldo.h"
#endif

#define SDMMC_FREQ_DEFAULT      20000       /*!< SD/MMC Default speed (limited by clock divider) */
#define SDMMC_FREQ_HIGHSPEED    40000       /*!< SD High speed (limited by clock divider) */
#define SDMMC_FREQ_PROBING      400         /*!< SD/MMC probing speed */
#define SDMMC_FREQ_52M          52000       /*!< MMC 52MHz speed */
#define SDMMC_FREQ_26M          26000       /*!< MMC 26MHz speed */
#define SDMMC_FREQ_DDR50        50000       /*!< MMC 50MHz speed */
#define SDMMC_FREQ_SDR50        100000      /*!< MMC 100MHz speed */

#define DEFAULT_CLK 14
#define DEFAULT_CMD 15
#define DEFAULT_D0 2
#define DEFAULT_D1 4
#define DEFAULT_D2 12
#define DEFAULT_D3 13

#ifndef DEFAULT_ALLOCATION_SIZE
#define DEFAULT_ALLOCATION_SIZE 16 * 1024
#endif
#ifndef DEFAULT_MAX_FILES
#define DEFAULT_MAX_FILES 5
#endif

namespace audio_tools {

/**
 * @brief ESP32 Virtual File System for SDMMC. The default mount point is "/sdcard"
 * 
 * DRAFT implementation: not tested
 * see
 * https://github.com/espressif/esp-idf/blob/master/examples/storage/sd_card/sdmmc/README.md
 * @ingroup player
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class VFS_SDMMC : public VFS {
 public:
  enum class Speed { HS, UHS_SDR, UHS_DDR };
  enum class BusWidth { Byte1 = 1, Byte4 = 4 };
  VFS_SDMMC() = default;
  VFS_SDMMC(int pinClk, int pinCmd, int pinD0, int pinD1, int pinD2 = -1,
            int pinD3 = -1) {
    setPins(pinClk, pinCmd, pinD0, pinD1, pinD2, pinD3);
  }

  void setPins(int pinClk, int pinCmd, int pinD0, int pinD1, int pinD2 = -1,
               int pinD3 = -1) {
    setClk(pinClk);
    setCmd(pinCmd);
    setD0(pinD0);
    setD1(pinD1);
    setD2(pinD2);
    setD3(pinD3);
  }
  void setClk(int pin) { pin_clk = (gpio_num_t)pin; }
  void setCmd(int pin) { pin_clk = (gpio_num_t)pin; }
  void setD0(int pin) { pin_d0 = (gpio_num_t)pin; }
  void setD1(int pin) { pin_d1 = (gpio_num_t)pin; }
  void setD2(int pin) { pin_d2 = (gpio_num_t)pin; }
  void setD3(int pin) { pin_d3 = (gpio_num_t)pin; }

  void setMountPoint(const char *mp) { mount_point = mp; }
  void setSpeed(Speed speed) { this->speed = speed; }
  void setBusWidth(BusWidth bits) { bus_width = bits; }

  bool begin() {
    esp_err_t ret;

    // Options for mounting the filesystem.
    // If format_if_mount_failed is set to true, SD card will be partitioned and
    // formatted in case when mounting fails.
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = max_files,
        .allocation_unit_size = allocation_unit_size};
    LOGI("Initializing SD card");

    // Use settings defined above to initialize SD card and mount FAT
    // filesystem. Note: esp_vfs_fat_sdmmc/sdspi_mount is all-in-one convenience
    // functions. Please check its source code and implement error recovery when
    // developing production applications.

    LOGI("Using SDMMC peripheral");

    // By default, SD card frequency is initialized to SDMMC_FREQ_DEFAULT
    // (20MHz) For setting a specific frequency, use host.max_freq_khz (range
    // 400kHz - 40MHz for SDMMC) Example: for fixed frequency of 10MHz, use
    // host.max_freq_khz = 10000;
    host = SDMMC_HOST_DEFAULT();

    switch (speed) {
      case Speed::HS:
        host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;
        break;
      case Speed::UHS_SDR:
        host.slot = SDMMC_HOST_SLOT_0;
        host.max_freq_khz = SDMMC_FREQ_SDR50;
        host.flags &= ~SDMMC_HOST_FLAG_DDR;
        break;
      case Speed::UHS_DDR:
        host.slot = SDMMC_HOST_SLOT_0;
        host.max_freq_khz = SDMMC_FREQ_DDR50;
        break;
    }

      // For SoCs where the SD power can be supplied both via an internal or
      // external (e.g. on-board LDO) power supply. When using specific IO pins
      // (which can be used for ultra high-speed SDMMC) to connect to the SD
      // card and the internal LDO power supply, we need to initialize the power
      // supply first.
#ifdef CONFIG_SD_PWR_CTRL_LDO_IO_ID

    sd_pwr_ctrl_ldo_config_t ldo_config = {
        .ldo_chan_id = CONFIG_SD_PWR_CTRL_LDO_IO_ID,
    };
    pwr_ctrl_handle = NULL;

    ret = sd_pwr_ctrl_new_on_chip_ldo(&ldo_config, &pwr_ctrl_handle);
    if (ret != ESP_OK) {
      LOGE("Failed to create a new on-chip LDO power control driver");
      return;
    }
    host.pwr_ctrl_handle = pwr_ctrl_handle;
#endif

    // This initializes the slot without card detect (CD) and write protect (WP)
    // signals. Modify slot_config.gpio_cd and slot_config.gpio_wp if your board
    // has these signals.
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
// #if EXAMPLE_IS_UHS1
//     slot_config.flags |= SDMMC_SLOT_FLAG_UHS1;
// #endif

    // Set bus width to use:
    slot_config.width = (int)bus_width;

    // On chips where the GPIOs used for SD card can be configured, set them in
    // the slot_config structure:
#ifdef CONFIG_SOC_SDMMC_USE_GPIO_MATRIX
    slot_config.clk = pin_clk;
    slot_config.cmd = pin_cmd;
    slot_config.d0 = pin_d0;
    slot_config.d1 = pin_d1;
    slot_config.d2 = pin_d2;
    slot_config.d3 = pin_d3;
#endif  // CONFIG_SOC_SDMMC_USE_GPIO_MATRIX

    // Enable internal pullups on enabled pins. The internal pullups
    // are insufficient however, please make sure 10k external pullups are
    // connected on the bus. This is for debug / example purpose only.
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    LOGI("Mounting filesystem");
    ret = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config,
                                  &mount_config, &card);

    if (ret != ESP_OK) {
      if (ret == ESP_FAIL) {
        LOGE("Failed to mount filesystem. ");
      } else {
        LOGE("Failed to initialize the card (%s). ", esp_err_to_name(ret));
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
    // All done, unmount partition and disable SDMMC peripheral
    esp_vfs_fat_sdcard_unmount(mount_point, card);
    LOGI("Card unmounted");
    card = nullptr;

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
  sdmmc_card_t *card = nullptr;
  const char *mount_point = "/sdcard";
  sdmmc_host_t host;
  sd_pwr_ctrl_handle_t pwr_ctrl_handle;
  int max_files = DEFAULT_MAX_FILES;
  size_t allocation_unit_size = DEFAULT_ALLOCATION_SIZE;
  Speed speed = Speed::HS;
  BusWidth bus_width = BusWidth::Byte1;
  gpio_num_t pin_clk = (gpio_num_t)DEFAULT_CLK;
  gpio_num_t pin_cmd = (gpio_num_t)DEFAULT_CMD;
  gpio_num_t pin_d0 = (gpio_num_t)DEFAULT_D0;
  gpio_num_t pin_d1 = (gpio_num_t)DEFAULT_D1;
  gpio_num_t pin_d2 = (gpio_num_t)DEFAULT_D2;
  gpio_num_t pin_d3 = (gpio_num_t)DEFAULT_D3;
};
}  // namespace audio_tools