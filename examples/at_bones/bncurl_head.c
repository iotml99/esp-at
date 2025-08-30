/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bncurl_head.h"
#include "bncurl_common.h"
#include "bncurl_methods.h"
#include "bncurl_config.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "esp_at.h"
#include "esp_log.h"

static const char *TAG = "BNCURL_HEAD";

bool bncurl_execute_head_request(bncurl_context_t *ctx)
{
    if (!ctx) {
        ESP_LOGE(TAG, "Invalid context");
        return false;
    }
    
    ESP_LOGI(TAG, "Starting HEAD request to: %s", ctx->params.url);
    
    // Initialize streaming context with context for file path (though HEAD won't use it for data)
    bncurl_stream_context_t stream;
    bncurl_stream_init(&stream, ctx);
    
    // Use common functionality for HEAD request
    bool success = bncurl_common_execute_request(ctx, &stream, "HEAD");
    
    // For HEAD requests, headers are streamed using +POST: markers via the streaming system
    // This provides consistent UART output format with GET/POST requests
    
    // Finalize streaming (will send SEND OK/ERROR)
    bncurl_stream_finalize(&stream, success);
    
    return success;
}
