/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_at.h"
#include "at_custom_cmd.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/spi_master.h"
#include "driver/sdspi_host.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include <curl/curl.h>

/* ========================= SD Card bits (unchanged) ========================= */

#define MOUNT_POINT "/sdcard"
#define PIN_NUM_MISO  19
#define PIN_NUM_MOSI  23
#define PIN_NUM_CLK   18
#define PIN_NUM_CS    5

static const char *TAG = "at_sd_card";
static sdmmc_card_t *sd_card = NULL;
static bool sd_mounted = false;
static int spi_host_slot = -1;

static esp_err_t sd_card_mount(void)
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

static esp_err_t sd_card_unmount(void)
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

static uint8_t at_bnsd_mount_cmd_test(uint8_t *cmd_name)
{
    uint8_t buffer[64] = {0};
    snprintf((char *)buffer, 64, "AT%s=? - Test SD card mount command\r\n", cmd_name);
    esp_at_port_write_data(buffer, strlen((char *)buffer));
    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_bnsd_mount_cmd_query(uint8_t *cmd_name)
{
    uint8_t buffer[64] = {0};
    snprintf((char *)buffer, 64, "AT%s? - SD card mount status: %s\r\n", cmd_name, sd_mounted ? "MOUNTED" : "UNMOUNTED");
    esp_at_port_write_data(buffer, strlen((char *)buffer));
    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_bnsd_mount_cmd_exe(uint8_t *cmd_name)
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

static uint8_t at_bnsd_unmount_cmd_test(uint8_t *cmd_name)
{
    uint8_t buffer[64] = {0};
    snprintf((char *)buffer, 64, "AT%s=? - Test SD card unmount command\r\n", cmd_name);
    esp_at_port_write_data(buffer, strlen((char *)buffer));
    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_bnsd_unmount_cmd_query(uint8_t *cmd_name)
{
    uint8_t buffer[64] = {0};
    snprintf((char *)buffer, 64, "AT%s? - SD card mount status: %s\r\n", cmd_name, sd_mounted ? "MOUNTED" : "UNMOUNTED");
    esp_at_port_write_data(buffer, strlen((char *)buffer));
    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_bnsd_unmount_cmd_exe(uint8_t *cmd_name)
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

/* ========================= Simple demo cmds (unchanged) ========================= */

static uint8_t at_test_cmd_test(uint8_t *cmd_name)
{
    uint8_t buffer[64] = {0};
    snprintf((char *)buffer, 64, "test command: <AT%s=?> is executed\r\n", cmd_name);
    esp_at_port_write_data(buffer, strlen((char *)buffer));
    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_query_cmd_test(uint8_t *cmd_name)
{
    uint8_t buffer[64] = {0};
    snprintf((char *)buffer, 64, "query command: <AT%s?> is executed\r\n", cmd_name);
    esp_at_port_write_data(buffer, strlen((char *)buffer));
    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_setup_cmd_test(uint8_t para_num)
{
    uint8_t index = 0;

    int32_t digit = 0;
    if (esp_at_get_para_as_digit(index++, &digit) != ESP_AT_PARA_PARSE_RESULT_OK) {
        return ESP_AT_RESULT_CODE_ERROR;
    }

    uint8_t *str = NULL;
    if (esp_at_get_para_as_str(index++, &str) != ESP_AT_PARA_PARSE_RESULT_OK) {
        return ESP_AT_RESULT_CODE_ERROR;
    }

    uint8_t *buffer = (uint8_t *)malloc(512);
    if (!buffer) {
        return ESP_AT_RESULT_CODE_ERROR;
    }
    int len = snprintf((char *)buffer, 512, "setup command: <AT%s=%d,\"%s\"> is executed\r\n",
                       esp_at_get_current_cmd_name(), digit, str);
    esp_at_port_write_data(buffer, len);
    free(buffer);
    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_exe_cmd_test(uint8_t *cmd_name)
{
    uint8_t buffer[64] = {0};
    snprintf((char *)buffer, 64, "execute command: <AT%s> is executed\r\n", cmd_name);
    esp_at_port_write_data(buffer, strlen((char *)buffer));
    return ESP_AT_RESULT_CODE_OK;
}

/* ========================= +BNCURL (blocking, safe) ========================= */

static long  bncurl_last_http_code = -1;
static char  bncurl_last_url[128]  = {0};
static bool  bncurl_curl_inited    = false;

typedef struct {
    size_t total_bytes;
} bncurl_ctx_t;

/* Worker request object:
   - done: semaphore the AT handler waits on (so command is user-visible blocking)
   - result_code: set by worker to OK/ERROR
*/
typedef struct {
    char url[256];
    SemaphoreHandle_t done;
    uint8_t result_code;
} bncurl_req_t;

static QueueHandle_t     bncurl_q      = NULL;
static TaskHandle_t      bncurl_task   = NULL;
static SemaphoreHandle_t at_uart_lock  = NULL;

/* Thread-safe write to AT UART */
static inline void at_uart_write_locked(const uint8_t *data, size_t len) {
    if (at_uart_lock) xSemaphoreTake(at_uart_lock, portMAX_DELAY);
    esp_at_port_write_data(data, len);
    if (at_uart_lock) xSemaphoreGive(at_uart_lock);
}

/* Stream body to UART in small chunks, sanitize control chars */
static size_t bncurl_sink(void *ptr, size_t size, size_t nmemb, void *userdata) {
    size_t total = size * nmemb;
    bncurl_ctx_t *ctx = (bncurl_ctx_t *)userdata;
    if (!ptr || total == 0) return 0;
    if (ctx) ctx->total_bytes += total;

    const unsigned char *src = (const unsigned char *)ptr;
    while (total) {
        size_t chunk = total > 256 ? 256 : total;
        char buf[256];
        memcpy(buf, src, chunk);
        for (size_t i = 0; i < chunk; i++) {
            unsigned char c = (unsigned char)buf[i];
            if (c < 0x20 && c != '\r' && c != '\n' && c != '\t') buf[i] = '.';
        }
        at_uart_write_locked((const uint8_t*)buf, chunk);
        src += chunk;
        total -= chunk;
    }
    return size * nmemb;
}

static uint8_t bncurl_perform_internal(const char *url) {
    if (!bncurl_curl_inited) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        bncurl_curl_inited = true;
    }
    CURL *h = curl_easy_init();
    if (!h) {
        const char *err = "+BNCURL: init failed\r\n";
        at_uart_write_locked((const uint8_t*)err, strlen(err));
        return ESP_AT_RESULT_CODE_ERROR;
    }

    bncurl_ctx_t ctx = { .total_bytes = 0 };
    at_uart_write_locked((const uint8_t*)"+BNCURL: BEGIN\r\n", 16);

    curl_easy_setopt(h, CURLOPT_URL, url);
    curl_easy_setopt(h, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(h, CURLOPT_WRITEFUNCTION, bncurl_sink);
    curl_easy_setopt(h, CURLOPT_WRITEDATA, &ctx);
    curl_easy_setopt(h, CURLOPT_USERAGENT, "esp-at-libcurl/1.0");

    /* Timeouts to prevent wedging the AT task */
    curl_easy_setopt(h, CURLOPT_CONNECTTIMEOUT_MS, 15000L);
    curl_easy_setopt(h, CURLOPT_TIMEOUT_MS,        60000L);

    /* For bring-up only; DO NOT SHIP with verification disabled:
       curl_easy_setopt(h, CURLOPT_SSL_VERIFYPEER, 0L);
       curl_easy_setopt(h, CURLOPT_SSL_VERIFYHOST, 0L);
       Production: embed a CA bundle and use CURLOPT_CAINFO_BLOB.
    */

    CURLcode rc = curl_easy_perform(h);
    long http_code = 0;
    if (rc == CURLE_OK) curl_easy_getinfo(h, CURLINFO_RESPONSE_CODE, &http_code);

    bncurl_last_http_code = (rc == CURLE_OK) ? http_code : -1;
    strncpy(bncurl_last_url, url, sizeof(bncurl_last_url) - 1);
    bncurl_last_url[sizeof(bncurl_last_url) - 1] = '\0';
    curl_easy_cleanup(h);

    char footer[128];
    int n = (rc != CURLE_OK)
        ? snprintf(footer, sizeof(footer), "\r\n+BNCURL: ERROR %d %s (bytes %u)\r\n",
                   rc, curl_easy_strerror(rc), (unsigned)ctx.total_bytes)
        : snprintf(footer, sizeof(footer), "\r\n+BNCURL: END HTTP %ld, %u bytes\r\n",
                   http_code, (unsigned)ctx.total_bytes);
    at_uart_write_locked((uint8_t*)footer, n);

    return (rc == CURLE_OK) ? ESP_AT_RESULT_CODE_OK : ESP_AT_RESULT_CODE_ERROR;
}

static void bncurl_worker(void *arg) {
    bncurl_req_t req;
    for (;;) {
        if (xQueueReceive(bncurl_q, &req, portMAX_DELAY)) {
            req.result_code = bncurl_perform_internal(req.url);
            if (req.done) xSemaphoreGive(req.done);  /* wake waiting AT handler */
        }
    }
}

static uint8_t at_bncurl_cmd_test(uint8_t *cmd_name) {
    const char *msg = "Usage: AT+BNCURL? (last result) | AT+BNCURL (default URL) | AT+BNCURL=\"https://host/path\"\r\n";
    at_uart_write_locked((const uint8_t*)msg, strlen(msg));
    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_bncurl_cmd_query(uint8_t *cmd_name) {
    char out[196];
    snprintf(out, sizeof(out), "+BNCURL: last_code=%ld, last_url=\"%s\"\r\n",
             bncurl_last_http_code, bncurl_last_url);
    at_uart_write_locked((uint8_t*)out, strlen(out));
    return ESP_AT_RESULT_CODE_OK;
}

/* Blocking semantics: handler waits on req.done until worker finishes */
static uint8_t at_bncurl_cmd_setup(uint8_t para_num) {
    if (para_num != 1 || !bncurl_q) return ESP_AT_RESULT_CODE_ERROR;

    uint8_t *url = NULL;
    if (esp_at_get_para_as_str(0, &url) != ESP_AT_PARA_PARSE_RESULT_OK) return ESP_AT_RESULT_CODE_ERROR;

    bncurl_req_t req = {0};
    strncpy(req.url, (const char*)url, sizeof(req.url)-1);
    req.done = xSemaphoreCreateBinary();
    if (!req.done) return ESP_AT_RESULT_CODE_ERROR;

    if (xQueueSend(bncurl_q, &req, pdMS_TO_TICKS(100)) != pdTRUE) {
        vSemaphoreDelete(req.done);
        return ESP_AT_RESULT_CODE_ERROR;
    }

    /* Wait max 120s for the transfer to finish */
    if (xSemaphoreTake(req.done, pdMS_TO_TICKS(120000)) != pdTRUE) {
        vSemaphoreDelete(req.done);
        return ESP_AT_RESULT_CODE_ERROR;
    }
    vSemaphoreDelete(req.done);
    return req.result_code;
}

static uint8_t at_bncurl_cmd_exe(uint8_t *cmd_name) {
    if (!bncurl_q) return ESP_AT_RESULT_CODE_ERROR;

    bncurl_req_t req = {0};
    strcpy(req.url, "https://example.com/");
    req.done = xSemaphoreCreateBinary();
    if (!req.done) return ESP_AT_RESULT_CODE_ERROR;

    if (xQueueSend(bncurl_q, &req, pdMS_TO_TICKS(100)) != pdTRUE) {
        vSemaphoreDelete(req.done);
        return ESP_AT_RESULT_CODE_ERROR;
    }

    if (xSemaphoreTake(req.done, pdMS_TO_TICKS(120000)) != pdTRUE) {
        vSemaphoreDelete(req.done);
        return ESP_AT_RESULT_CODE_ERROR;
    }
    vSemaphoreDelete(req.done);
    return req.result_code;
}

/* ----------------------- Command table & init ----------------------- */

static const esp_at_cmd_struct at_custom_cmd[] = {
    {"+TEST",         at_test_cmd_test,       at_query_cmd_test,    at_setup_cmd_test,   at_exe_cmd_test},
    {"+BNSD_MOUNT",   at_bnsd_mount_cmd_test, at_bnsd_mount_cmd_query, NULL,             at_bnsd_mount_cmd_exe},
    {"+BNSD_UNMOUNT", at_bnsd_unmount_cmd_test, at_bnsd_unmount_cmd_query, NULL,         at_bnsd_unmount_cmd_exe},
    {"+BNCURL",       at_bncurl_cmd_test,     at_bncurl_cmd_query,  at_bncurl_cmd_setup, at_bncurl_cmd_exe},
    /* add further custom AT commands here */
};

bool esp_at_custom_cmd_register(void)
{
    bool ok = esp_at_custom_cmd_array_regist(at_custom_cmd, sizeof(at_custom_cmd) / sizeof(esp_at_cmd_struct));
    if (!ok) return false;

    if (!at_uart_lock) at_uart_lock = xSemaphoreCreateMutex();
    if (!bncurl_q)     bncurl_q = xQueueCreate(2, sizeof(bncurl_req_t));
    if (!bncurl_task) {
        /* TLS + libcurl + printf ==> give it a big stack; tune later */
        xTaskCreatePinnedToCore(bncurl_worker, "bncurl", 16384, NULL, 5, &bncurl_task, 0);
    }
    return true;
}

ESP_AT_CMD_SET_INIT_FN(esp_at_custom_cmd_register, 1);
