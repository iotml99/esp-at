/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "sdmmc_cmd.h"

#ifdef __cplusplus
extern "C" {
#endif

/* SD Card configuration */
#define MOUNT_POINT "/sdcard"

/* SD Card SPI pin configuration - update these for your board */
#define PIN_NUM_CS    20
#define PIN_NUM_MOSI  21
#define PIN_NUM_CLK   17
#define PIN_NUM_MISO  16

/**
 * @brief Initialize and mount SD card
 *
 * @return
 *     - ESP_OK on success
 *     - ESP_FAIL if failed to mount filesystem
 *     - Other ESP error codes on failure
 */
esp_err_t sd_card_mount(void);

/**
 * @brief Unmount SD card and free resources
 *
 * @return
 *     - ESP_OK on success
 *     - ESP error codes on failure
 */
esp_err_t sd_card_unmount(void);

/**
 * @brief Check if SD card is currently mounted
 *
 * @return true if mounted, false otherwise
 */
bool sd_card_is_mounted(void);

/**
 * @brief Get SD card info
 *
 * @return Pointer to SD card structure, NULL if not mounted
 */
sdmmc_card_t* sd_card_get_info(void);

/**
 * @brief Get mount point path
 *
 * @return Mount point string
 */
const char* sd_card_get_mount_point(void);

/* AT command handlers for SD card operations */
uint8_t at_bnsd_mount_cmd_test(uint8_t *cmd_name);
uint8_t at_bnsd_mount_cmd_query(uint8_t *cmd_name);
uint8_t at_bnsd_mount_cmd_exe(uint8_t *cmd_name);
uint8_t at_bnsd_unmount_cmd_test(uint8_t *cmd_name);
uint8_t at_bnsd_unmount_cmd_query(uint8_t *cmd_name);
uint8_t at_bnsd_unmount_cmd_exe(uint8_t *cmd_name);

#ifdef __cplusplus
}
#endif
