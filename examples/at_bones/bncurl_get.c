/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bncurl_get.h"
#include "bncurl_common.h"
#include "bncurl_methods.h"
#include "bncurl_config.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "esp_log.h"

static const char *TAG = "BNCURL_GET";

bool bncurl_execute_get_request(bncurl_context_t *ctx)
{
    if (!ctx) {
        ESP_LOGE(TAG, "Invalid context");
        return false;
    }
    
    // Initialize GET context
    bncurl_get_context_t get_ctx;
    memset(&get_ctx, 0, sizeof(get_ctx));
    get_ctx.ctx = ctx;
    bncurl_stream_init(&get_ctx.stream);
    
    ESP_LOGI(TAG, "Starting GET request to: %s", ctx->params.url);
    
    // Use common execution function
    bool success = bncurl_common_execute_request(ctx, &get_ctx.stream, "GET");
    
    // Finalize streaming
    bncurl_stream_finalize(&get_ctx.stream, success);
    
    return success;
}
