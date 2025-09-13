/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BNSD_H
#define BNSD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// SD card configuration
#define BNSD_MOUNT_POINT           "/sdcard"
#define BNSD_MAX_PATH_LENGTH       256
#define BNSD_MAX_FILES             5

// SD card status enumeration
typedef enum {
    BNSD_STATUS_UNMOUNTED = 0,
    BNSD_STATUS_MOUNTED = 1,
    BNSD_STATUS_ERROR = 2
} bnsd_status_t;

// SD card information structure
typedef struct {
    bool is_mounted;
    uint64_t total_bytes;
    uint64_t used_bytes;
    uint64_t free_bytes;
    char mount_point[32];
} bnsd_info_t;

/**
 * @brief Initialize SD card module
 * 
 * @return true on success, false on failure
 */
bool bnsd_init(void);

/**
 * @brief Deinitialize SD card module
 */
void bnsd_deinit(void);

/**
 * @brief Mount SD card
 * 
 * @param mount_point Mount point path (optional, uses default if NULL)
 * @return true on success, false on failure
 */
bool bnsd_mount(const char *mount_point);

/**
 * @brief Unmount SD card
 * 
 * @return true on success, false on failure
 */
bool bnsd_unmount(void);

/**
 * @brief Check if SD card is mounted
 * 
 * @return true if mounted, false otherwise
 */
bool bnsd_is_mounted(void);

/**
 * @brief Get SD card status
 * 
 * @return SD card status
 */
bnsd_status_t bnsd_get_status(void);

/**
 * @brief Get SD card space information
 * 
 * @param info Pointer to info structure to fill
 * @return true on success, false on failure
 */
bool bnsd_get_space_info(bnsd_info_t *info);

/**
 * @brief Create directory recursively
 * 
 * @param path Directory path to create
 * @return true on success, false on failure
 */
bool bnsd_mkdir_recursive(const char *path);

/**
 * @brief Get current mount point
 * 
 * @return Mount point string or NULL if not mounted
 */
const char *bnsd_get_mount_point(void);

/**
 * @brief Format SD card with FAT32 filesystem
 * 
 * @return true on success, false on failure
 */
bool bnsd_format(void);

/**
 * @brief Normalize path by replacing @ prefix with mount point
 * 
 * This function transforms paths starting with @ or @/ into absolute paths
 * using the SD card mount point. For example:
 * - "@file.txt" becomes "/sdcard/file.txt"
 * - "@/Downloads/file.txt" becomes "/sdcard/Downloads/file.txt"
 * 
 * @param path Path string to normalize (modified in place)
 * @param max_length Maximum length of the path buffer
 */
void bnsd_normalize_path_with_mount_point(char *path, size_t max_length);

#endif /* BNSD_H */
