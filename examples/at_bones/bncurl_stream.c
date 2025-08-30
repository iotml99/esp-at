/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bncurl_methods.h"
#include "bncurl_config.h"
#include <stdio.h>
#include <string.h>
#include "esp_at.h"
#include "esp_log.h"

static const char *TAG = "BNCURL_STREAM";

void bncurl_stream_init(bncurl_stream_context_t *stream_ctx)
{
    if (!stream_ctx) {
        return;
    }
    
    memset(stream_ctx, 0, sizeof(bncurl_stream_context_t));
    stream_ctx->active_buffer = 0;
    stream_ctx->streaming_buffer = -1;
    
    // Initialize buffers
    for (int i = 0; i < BNCURL_STREAM_BUFFER_COUNT; i++) {
        stream_ctx->buffers[i].size = 0;
        stream_ctx->buffers[i].is_full = false;
        stream_ctx->buffers[i].is_streaming = false;
    }
    
    ESP_LOGI(TAG, "Stream context initialized with %d buffers of %d bytes each", 
             BNCURL_STREAM_BUFFER_COUNT, BNCURL_STREAM_BUFFER_SIZE);
}

bool bncurl_stream_buffer_to_uart(bncurl_stream_context_t *stream_ctx, int buffer_index)
{
    if (!stream_ctx || buffer_index < 0 || buffer_index >= BNCURL_STREAM_BUFFER_COUNT) {
        ESP_LOGE(TAG, "Invalid stream context or buffer index");
        return false;
    }
    
    bncurl_stream_buffer_t *buffer = &stream_ctx->buffers[buffer_index];
    
    if (buffer->size == 0) {
        ESP_LOGW(TAG, "Attempted to stream empty buffer %d", buffer_index);
        return true; // Not an error, just nothing to stream
    }
    
    // Mark buffer as streaming
    buffer->is_streaming = true;
    stream_ctx->streaming_buffer = buffer_index;
    
    // Send chunk length indication: +POST:1024, (using %u instead of %zu)
    char chunk_header[32];
    snprintf(chunk_header, sizeof(chunk_header), "+POST:%u,", (unsigned int)buffer->size);
    esp_at_port_write_data((uint8_t *)chunk_header, strlen(chunk_header));
    
    // Send the actual data
    esp_at_port_write_data((uint8_t *)buffer->data, buffer->size);
    
    // Update streaming statistics
    stream_ctx->bytes_streamed += buffer->size;
    
    ESP_LOGI(TAG, "Streamed buffer %d: %u bytes (total streamed: %u)", 
             buffer_index, (unsigned int)buffer->size, (unsigned int)stream_ctx->bytes_streamed);
    
    // Reset buffer for reuse
    buffer->size = 0;
    buffer->is_full = false;
    buffer->is_streaming = false;
    stream_ctx->streaming_buffer = -1;
    
    return true;
}

void bncurl_stream_finalize(bncurl_stream_context_t *stream_ctx, bool success)
{
    if (!stream_ctx) {
        return;
    }
    
    // Send completion message
    if (success) {
        const char *send_ok = "\r\nSEND OK\r\n";
        esp_at_port_write_data((uint8_t *)send_ok, strlen(send_ok));
        ESP_LOGI(TAG, "Stream completed successfully. Total bytes: %u", (unsigned int)stream_ctx->bytes_streamed);
    } else {
        const char *send_error = "\r\nSEND ERROR\r\n";
        esp_at_port_write_data((uint8_t *)send_error, strlen(send_error));
        ESP_LOGE(TAG, "Stream completed with error. Bytes streamed: %u", (unsigned int)stream_ctx->bytes_streamed);
    }
    
    // Log final statistics
    ESP_LOGI(TAG, "Streaming statistics:");
    ESP_LOGI(TAG, "  Total size (if known): %u bytes", (unsigned int)stream_ctx->total_size);
    ESP_LOGI(TAG, "  Bytes streamed: %u bytes", (unsigned int)stream_ctx->bytes_streamed);
}
