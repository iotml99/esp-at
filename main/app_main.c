/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_err.h"

#include "esp_at.h"
#include "esp_at_init.h"

#include <curl/curl.h>

#include "esp_log.h"
#include <string.h>


static const char *TAG = "curl";

static size_t sink(void *ptr, size_t size, size_t nmemb, void *userdata) {
    size_t total = size * nmemb;
    if (total == 0 || ptr == NULL) {
        return 0;
    }

    /* Ensure printable / bounded logging: copy to a local buffer, add NUL */
    const size_t MAX_LOG_CHUNK = 256; /* per log line */
    const unsigned char *src = (const unsigned char *)ptr;
    while (total) {
        size_t this_len = total > MAX_LOG_CHUNK ? MAX_LOG_CHUNK : total;
        char buf[256 + 1];
        memcpy(buf, src, this_len);
        buf[this_len] = '\0';
        /* Replace any non-printable chars (except common whitespace) to keep log clean */
        for (size_t i = 0; i < this_len; ++i) {
            unsigned char c = (unsigned char)buf[i];
            if (c < 0x20 && c != '\r' && c != '\n' && c != '\t') {
                buf[i] = '.'; /* dot placeholder */
            }
        }
        ESP_LOGE(TAG, "RX (%u bytes): %s", (unsigned)this_len, buf);
        src += this_len;
        total -= this_len;
    }

    return size * nmemb;
}

void do_https_get(void) {
    ESP_LOGE(TAG, "curl global init");
    curl_global_init(CURL_GLOBAL_DEFAULT);
    ESP_LOGE(TAG, "curl easy init");
    CURL *h = curl_easy_init();
    if (h) {
        ESP_LOGE(TAG, "Setting URL & callbacks");
        curl_easy_setopt(h, CURLOPT_URL, "https://example.com/");
        curl_easy_setopt(h, CURLOPT_WRITEFUNCTION, sink);

        // For first tests only; DO NOT ship with these disabled:
        // curl_easy_setopt(h, CURLOPT_SSL_VERIFYPEER, 0L);
        // curl_easy_setopt(h, CURLOPT_SSL_VERIFYHOST, 0L);

        ESP_LOGE(TAG, "Performing request");
        CURLcode rc = curl_easy_perform(h);
        if (rc != CURLE_OK) {
            ESP_LOGE(TAG, "curl_easy_perform failed: %s", curl_easy_strerror(rc));
        } else {
            long http_code = 0;
            curl_easy_getinfo(h, CURLINFO_RESPONSE_CODE, &http_code);
            ESP_LOGE(TAG, "HTTP response code: %ld", http_code);
        }
        ESP_LOGE(TAG, "Cleaning up curl handle");
        curl_easy_cleanup(h);
    } else {
        ESP_LOGE(TAG, "curl_easy_init returned NULL");
    }
    ESP_LOGE(TAG, "curl global cleanup");
    curl_global_cleanup();
}

void app_main(void)
{
    esp_at_main_preprocess();


    ESP_ERROR_CHECK(nvs_flash_init());

    ESP_ERROR_CHECK(esp_at_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_at_init();

    ESP_LOGE(TAG, "Running HTTPS GET test");
    do_https_get();
    ESP_LOGE(TAG, "Done app_main");
}
