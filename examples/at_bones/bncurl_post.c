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
    
    // Initialize streaming context with context for file path
    bncurl_stream_context_t stream;
    bncurl_stream_init(&stream, ctx);
    
    // Use common functionality for POST request
    bool success = bncurl_common_execute_request(ctx, &stream, "POST");
    
    // Finalize streaming
    bncurl_stream_finalize(&stream, success);
    
    return success;
}
