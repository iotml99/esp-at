/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AT_SD_H
#define AT_SD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// SD card configuration
#define AT_SD_MOUNT_POINT           "/sdcard"
#define AT_SD_MAX_PATH_LENGTH       256
#define AT_SD_MAX_FILES             5

// SD card status enumeration
typedef enum {
    AT_SD_STATUS_UNMOUNTED = 0,
    AT_SD_STATUS_MOUNTED = 1,
    AT_SD_STATUS_ERROR = 2
} at_sd_status_t;

// SD card information structure
typedef struct {
    bool is_mounted;
    uint64_t total_bytes;
    uint64_t used_bytes;
    uint64_t free_bytes;
    char mount_point[32];
} at_sd_info_t;

/**
 * @brief Initialize SD card module
 * 
 * @return true on success, false on failure
 */
bool at_sd_init(void);

/**
 * @brief Deinitialize SD card module
 */
void at_sd_deinit(void);

/**
 * @brief Mount SD card
 * 
 * @param mount_point Mount point path (optional, uses default if NULL)
 * @return true on success, false on failure
 */
bool at_sd_mount(const char *mount_point);

/**
 * @brief Unmount SD card
 * 
 * @return true on success, false on failure
 */
bool at_sd_unmount(void);

/**
 * @brief Check if SD card is mounted
 * 
 * @return true if mounted, false otherwise
 */
bool at_sd_is_mounted(void);

/**
 * @brief Get SD card status
 * 
 * @return SD card status
 */
at_sd_status_t at_sd_get_status(void);

/**
 * @brief Get SD card space information
 * 
 * @param info Pointer to info structure to fill
 * @return true on success, false on failure
 */
bool at_sd_get_space_info(at_sd_info_t *info);

/**
 * @brief Create directory recursively
 * 
 * @param path Directory path to create
 * @return true on success, false on failure
 */
bool at_sd_mkdir_recursive(const char *path);

/**
 * @brief Get current mount point
 * 
 * @return Mount point string or NULL if not mounted
 */
const char *at_sd_get_mount_point(void);

#endif /* AT_SD_H */
