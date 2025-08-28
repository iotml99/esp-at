/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sd_card.h"
#include <stdio.h>
#include <string.h>
#include "esp_vfs_fat.h"
#include "driver/spi_master.h"
#include "driver/sdspi_host.h"
#include "esp_log.h"
#include "esp_at.h"

static const char *TAG = "at_sd_card";
static sdmmc_card_t *sd_card = NULL;
static bool sd_mounted = false;
static int spi_host_slot = -1;

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
