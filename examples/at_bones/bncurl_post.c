/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bncurl_post.h"
#include "bncurl_common.h"
#include "bncurl_methods.h"
#include "bncurl_config.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "esp_at.h"
#include "esp_log.h"

static const char *TAG = "BNCURL_POST";

bool bncurl_execute_post_request(bncurl_context_t *ctx)
{
    if (!ctx) {
        ESP_LOGE(TAG, "Invalid context");
        return false;
    }
    
    ESP_LOGI(TAG, "Starting POST request to: %s", ctx->params.url);
    
    // First, make a HEAD request to get the content length
    size_t expected_content_length = SIZE_MAX; // SIZE_MAX indicates unknown/error
    bool has_content_length = bncurl_common_get_content_length(ctx, &expected_content_length);
    
    // Always output +LEN: marker with appropriate value
    char len_marker[64];
    if (expected_content_length == SIZE_MAX) {
        // HEAD failed or no content length available
        snprintf(len_marker, sizeof(len_marker), "+LEN:-1\r\n");
        ESP_LOGI(TAG, "Content length unknown, sending +LEN:-1");
    } else {
        // HEAD succeeded and we have content length (including 0)
        snprintf(len_marker, sizeof(len_marker), "+LEN:%u\r\n", (unsigned int)expected_content_length);
        ESP_LOGI(TAG, "Content length determined: %u bytes", (unsigned int)expected_content_length);
    }
    esp_at_port_write_data((uint8_t *)len_marker, strlen(len_marker));
    
    // Initialize streaming context
    bncurl_stream_context_t stream;
    bncurl_stream_init(&stream, ctx);
    
    // Set expected content length if available (ignore SIZE_MAX which indicates unknown)
    if (expected_content_length != SIZE_MAX) {
        stream.total_size = expected_content_length;
        ctx->bytes_total = expected_content_length;
    }
    
    // Use common functionality for POST request
    bool success = bncurl_common_execute_request(ctx, &stream, "POST");
    
    // Finalize streaming
    bncurl_stream_finalize(&stream, success);
    
    return success;
}
