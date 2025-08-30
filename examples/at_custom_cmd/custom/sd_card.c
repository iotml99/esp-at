/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sd_card.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "driver/spi_master.h"
#include "driver/sdspi_host.h"
#include "esp_log.h"
#include "esp_at.h"
#include "ff.h"
#include <inttypes.h>

static const char *TAG = "at_sd_card";
static sdmmc_card_t *sd_card = NULL;
static bool sd_mounted = false;
static int spi_host_slot = -1;

esp_err_t sd_card_init()
{
    esp_log_level_set(TAG, ESP_LOG_INFO);
    return ESP_OK;
}


esp_err_t sd_card_mount(void)
{
    if (sd_mounted) {
        ESP_LOGW(TAG, "SD card already mounted");
        return ESP_OK;
    }

    esp_err_t ret;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    const char mount_point[] = MOUNT_POINT;
    ESP_LOGI(TAG, "Initializing SD card");
    ESP_LOGI(TAG, "Using pins - MISO: %d, MOSI: %d, CLK: %d, CS: %d",
             PIN_NUM_MISO, PIN_NUM_MOSI, PIN_NUM_CLK, PIN_NUM_CS);

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize bus.");
        return ret;
    }
    spi_host_slot = host.slot;

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = host.slot;

    ESP_LOGI(TAG, "Mounting filesystem");
    ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &sd_card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. If you want the card to be formatted, set the format_if_mount_failed option.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s) (%d). Make sure SD card lines have pull-ups.", esp_err_to_name(ret), ret);
        }
        spi_bus_free(host.slot);
        return ret;
    }

    sd_mounted = true;
    ESP_LOGI(TAG, "Filesystem mounted");
    sdmmc_card_print_info(stdout, sd_card);
    return ESP_OK;
}

esp_err_t sd_card_unmount(void)
{
    if (!sd_mounted) {
        ESP_LOGW(TAG, "SD card not mounted");
        return ESP_OK;
    }

    esp_err_t ret = esp_vfs_fat_sdcard_unmount(MOUNT_POINT, sd_card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to unmount filesystem (%s)", esp_err_to_name(ret));
        return ret;
    }

    if (spi_host_slot != -1) {
        spi_bus_free(spi_host_slot);
        spi_host_slot = -1;
    }

    sd_mounted = false;
    sd_card = NULL;
    ESP_LOGI(TAG, "Card unmounted");
    return ESP_OK;
}

bool sd_card_is_mounted(void)
{
    return sd_mounted;
}

sdmmc_card_t* sd_card_get_info(void)
{
    return sd_card;
}

const char* sd_card_get_mount_point(void)
{
    return MOUNT_POINT;
}

esp_err_t sd_card_format(void)
{
    esp_err_t ret;
    
    /* Check if SD card is currently mounted, unmount if necessary */
    bool was_mounted = sd_mounted;
    if (sd_mounted) {
        ret = sd_card_unmount();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to unmount SD card before formatting");
            return ret;
        }
    }
    
    /* Initialize SPI bus and SD card for formatting */
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };
    
    ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize bus for formatting");
        return ret;
    }
    spi_host_slot = host.slot;
    
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = host.slot;
    
    /* Mount with format option enabled to initialize the card */
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 5,
        .allocation_unit_size = 64 * 1024  /* Use larger allocation unit for better performance */
    };
    
    ESP_LOGI(TAG, "Initializing SD card for formatting...");
    ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &sd_card);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SD card for formatting (%s)", esp_err_to_name(ret));
        spi_bus_free(host.slot);
        spi_host_slot = -1;
        return ret;
    }
    
    sd_mounted = true;
    ESP_LOGI(TAG, "Formatting SD card to FAT32...");
    
    /* Format the SD card using ESP-IDF function */
    ret = esp_vfs_fat_sdcard_format(MOUNT_POINT, sd_card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to format SD card (%s)", esp_err_to_name(ret));
        /* Cleanup on failure */
        esp_vfs_fat_sdcard_unmount(MOUNT_POINT, sd_card);
        spi_bus_free(host.slot);
        spi_host_slot = -1;
        sd_mounted = false;
        sd_card = NULL;
        return ret;
    }
    
    ESP_LOGI(TAG, "SD card formatted successfully to FAT32");
    
    /* If it wasn't mounted before, unmount it after formatting */
    if (!was_mounted) {
        ret = sd_card_unmount();
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Warning: Failed to unmount after formatting");
        }
    }
    
    return ESP_OK;
}

esp_err_t sd_card_get_space_info(uint64_t *total_bytes, uint64_t *used_bytes)
{
    if (!total_bytes || !used_bytes) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!sd_mounted || !sd_card) {
        ESP_LOGE(TAG, "SD card not mounted");
        return ESP_ERR_INVALID_STATE;
    }
    
    /* Get physical card capacity from SD card info */
    uint64_t card_size_bytes = ((uint64_t)sd_card->csd.capacity) * sd_card->csd.sector_size;
    
    FATFS *fs;
    DWORD free_clusters;
    
    /* Get filesystem free space */
    FRESULT res = f_getfree("0:", &free_clusters, &fs);
    if (res != FR_OK) {
        ESP_LOGE(TAG, "Failed to get filesystem statistics (FatFS error: %d)", res);
        return ESP_FAIL;
    }
    
    /* Calculate filesystem space */
    uint64_t cluster_size = (uint64_t)fs->csize * 512; /* 512 bytes per sector */
    uint64_t filesystem_total = (uint64_t)(fs->n_fatent - 2) * cluster_size;
    uint64_t free_bytes = (uint64_t)free_clusters * cluster_size;
    uint64_t filesystem_used = filesystem_total - free_bytes;
    
    /* Use physical card size as total, filesystem used space as used */
    *total_bytes = card_size_bytes;
    *used_bytes = filesystem_used;
    
    /* Debug logging */
    ESP_LOGI(TAG, "SD card debug - Card capacity: %llu bytes (%.2f GB)", 
             card_size_bytes, card_size_bytes / (1024.0 * 1024.0 * 1024.0));
    ESP_LOGI(TAG, "SD card debug - Filesystem total: %llu bytes (%.2f GB), Free: %llu bytes (%.2f GB)", 
             filesystem_total, filesystem_total / (1024.0 * 1024.0 * 1024.0),
             free_bytes, free_bytes / (1024.0 * 1024.0 * 1024.0));
    ESP_LOGI(TAG, "SD card space - Total: %llu bytes (%.2f GB), Used: %llu bytes (%.2f GB)", 
             *total_bytes, *total_bytes / (1024.0 * 1024.0 * 1024.0),
             *used_bytes, *used_bytes / (1024.0 * 1024.0 * 1024.0));
    
    return ESP_OK;
}

esp_err_t sd_card_create_directory_recursive(const char *path)
{
    if (!path) return ESP_ERR_INVALID_ARG;
    
    char temp_path[256];
    strncpy(temp_path, path, sizeof(temp_path) - 1);
    temp_path[sizeof(temp_path) - 1] = '\0';
    
    /* Remove filename from path to get directory */
    char *last_slash = strrchr(temp_path, '/');
    if (!last_slash) return ESP_OK; /* No directory separator found */
    
    *last_slash = '\0'; /* Terminate at last slash to get directory path */
    
    /* Check if directory already exists */
    struct stat st = {0};
    if (stat(temp_path, &st) == 0) {
        return ESP_OK; /* Directory already exists */
    }
    
    /* Log directory creation for debugging */
    ESP_LOGI(TAG, "Creating directory: %s", temp_path);
    
    /* Notify user that directories will be created */
    char msg[128];
    int n = snprintf(msg, sizeof(msg), "+BNCURL: Creating directory: %s\r\n", temp_path);
    esp_at_port_write_data((uint8_t*)msg, n);
    
    /* Create parent directories first */
    char *pos = temp_path + 1; /* Skip leading slash */
    while ((pos = strchr(pos, '/')) != NULL) {
        *pos = '\0';
        
        if (stat(temp_path, &st) != 0) {
            if (mkdir(temp_path, 0755) != 0) {
                ESP_LOGE(TAG, "Failed to create directory: %s", temp_path);
                return ESP_FAIL;
            }
        }
        
        *pos = '/';
        pos++;
    }
    
    /* Create the final directory */
    if (stat(temp_path, &st) != 0) {
        if (mkdir(temp_path, 0755) != 0) {
            ESP_LOGE(TAG, "Failed to create directory: %s", temp_path);
            return ESP_FAIL;
        }
    }
    
    return ESP_OK;
}

/* AT command handlers */
uint8_t at_bnsd_mount_cmd_test(uint8_t *cmd_name)
{
    uint8_t buffer[64] = {0};
    snprintf((char *)buffer, 64, "AT%s=? - Test SD card mount command\r\n", cmd_name);
    esp_at_port_write_data(buffer, strlen((char *)buffer));
    return ESP_AT_RESULT_CODE_OK;
}

uint8_t at_bnsd_mount_cmd_query(uint8_t *cmd_name)
{
    uint8_t buffer[64] = {0};
    snprintf((char *)buffer, 64, "AT%s? - SD card mount status: %s\r\n", cmd_name, sd_mounted ? "MOUNTED" : "UNMOUNTED");
    esp_at_port_write_data(buffer, strlen((char *)buffer));
    return ESP_AT_RESULT_CODE_OK;
}

uint8_t at_bnsd_mount_cmd_exe(uint8_t *cmd_name)
{
    esp_err_t ret = sd_card_mount();
    uint8_t buffer[128] = {0};
    if (ret == ESP_OK) {
        snprintf((char *)buffer, 128, "SD card mounted successfully at %s\r\n", MOUNT_POINT);
        esp_at_port_write_data(buffer, strlen((char *)buffer));
        return ESP_AT_RESULT_CODE_OK;
    } else {
        snprintf((char *)buffer, 128, "Failed to mount SD card: %s\r\n", esp_err_to_name(ret));
        esp_at_port_write_data(buffer, strlen((char *)buffer));
        return ESP_AT_RESULT_CODE_ERROR;
    }
}

uint8_t at_bnsd_unmount_cmd_test(uint8_t *cmd_name)
{
    uint8_t buffer[64] = {0};
    snprintf((char *)buffer, 64, "AT%s=? - Test SD card unmount command\r\n", cmd_name);
    esp_at_port_write_data(buffer, strlen((char *)buffer));
    return ESP_AT_RESULT_CODE_OK;
}

uint8_t at_bnsd_unmount_cmd_query(uint8_t *cmd_name)
{
    uint8_t buffer[64] = {0};
    snprintf((char *)buffer, 64, "AT%s? - SD card mount status: %s\r\n", cmd_name, sd_mounted ? "MOUNTED" : "UNMOUNTED");
    esp_at_port_write_data(buffer, strlen((char *)buffer));
    return ESP_AT_RESULT_CODE_OK;
}

uint8_t at_bnsd_unmount_cmd_exe(uint8_t *cmd_name)
{
    esp_err_t ret = sd_card_unmount();
    uint8_t buffer[128] = {0};
    if (ret == ESP_OK) {
        snprintf((char *)buffer, 128, "SD card unmounted successfully\r\n");
        esp_at_port_write_data(buffer, strlen((char *)buffer));
        return ESP_AT_RESULT_CODE_OK;
    } else {
        snprintf((char *)buffer, 128, "Failed to unmount SD card: %s\r\n", esp_err_to_name(ret));
        esp_at_port_write_data(buffer, strlen((char *)buffer));
        return ESP_AT_RESULT_CODE_ERROR;
    }
}

uint8_t at_bnsd_format_cmd_test(uint8_t *cmd_name)
{
    uint8_t buffer[64] = {0};
    snprintf((char *)buffer, 64, "AT%s=? - Test SD card format command\r\n", cmd_name);
    esp_at_port_write_data(buffer, strlen((char *)buffer));
    return ESP_AT_RESULT_CODE_OK;
}

uint8_t at_bnsd_format_cmd_query(uint8_t *cmd_name)
{
    uint8_t buffer[64] = {0};
    snprintf((char *)buffer, 64, "AT%s? - SD card format command (formats to FAT32)\r\n", cmd_name);
    esp_at_port_write_data(buffer, strlen((char *)buffer));
    return ESP_AT_RESULT_CODE_OK;
}

uint8_t at_bnsd_format_cmd_exe(uint8_t *cmd_name)
{
    esp_err_t ret = sd_card_format();
    uint8_t buffer[128] = {0};
    if (ret == ESP_OK) {
        snprintf((char *)buffer, 128, "SD card formatted successfully (FAT32)\r\n");
        esp_at_port_write_data(buffer, strlen((char *)buffer));
        return ESP_AT_RESULT_CODE_OK;
    } else {
        snprintf((char *)buffer, 128, "Failed to format SD card: %s\r\n", esp_err_to_name(ret));
        esp_at_port_write_data(buffer, strlen((char *)buffer));
        return ESP_AT_RESULT_CODE_ERROR;
    }
}

uint8_t at_bnsd_space_cmd_test(uint8_t *cmd_name)
{
    uint8_t buffer[64] = {0};
    snprintf((char *)buffer, 64, "AT%s=? - Test SD card space command\r\n", cmd_name);
    esp_at_port_write_data(buffer, strlen((char *)buffer));
    return ESP_AT_RESULT_CODE_OK;
}

uint8_t at_bnsd_space_cmd_query(uint8_t *cmd_name)
{
    uint8_t buffer[64] = {0};
    snprintf((char *)buffer, 64, "AT%s? - Get SD card space information\r\n", cmd_name);
    esp_at_port_write_data(buffer, strlen((char *)buffer));
    return ESP_AT_RESULT_CODE_OK;
}

uint8_t at_bnsd_space_cmd_exe(uint8_t *cmd_name)
{
    uint64_t total_bytes, used_bytes;
    esp_err_t ret = sd_card_get_space_info(&total_bytes, &used_bytes);
    uint8_t buffer[128] = {0};
    
    if (ret == ESP_OK) {
        /* Convert to MB (1024*1024 = 1048576) */
        uint32_t total_mb = (uint32_t)(total_bytes / 1048576ULL);
        uint32_t used_mb = (uint32_t)(used_bytes / 1048576ULL);
        
        snprintf((char *)buffer, sizeof(buffer),
         "+BNSD_SIZE: %lu/%lu\r\n",
         (unsigned long)total_mb, (unsigned long)used_mb);
        esp_at_port_write_data(buffer, strlen((char *)buffer));
        return ESP_AT_RESULT_CODE_OK;
    } else {
        snprintf((char *)buffer, 128, "Failed to get SD card space info: %s\r\n", esp_err_to_name(ret));
        esp_at_port_write_data(buffer, strlen((char *)buffer));
        return ESP_AT_RESULT_CODE_ERROR;
    }
}
