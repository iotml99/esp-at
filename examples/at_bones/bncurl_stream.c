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

void bncurl_stream_init(bncurl_stream_context_t *stream_ctx, bncurl_context_t *ctx)
{
    if (!stream_ctx) {
        return;
    }
    
    memset(stream_ctx, 0, sizeof(bncurl_stream_context_t));
    stream_ctx->active_buffer = 0;
    stream_ctx->streaming_buffer = -1;
    stream_ctx->output_file = NULL;
    stream_ctx->file_path = NULL;
    
    // Check if we have a download file path
    if (ctx && strlen(ctx->params.data_download) > 0) {
        // Set up file output
        stream_ctx->file_path = ctx->params.data_download;
        stream_ctx->output_file = fopen(stream_ctx->file_path, "wb");
        if (!stream_ctx->output_file) {
            ESP_LOGE(TAG, "Failed to open file for writing: %s", stream_ctx->file_path);
            stream_ctx->file_path = NULL;
        } else {
            ESP_LOGI(TAG, "Opened file for download: %s", stream_ctx->file_path);
        }
    }
    
    // Initialize buffers
    for (int i = 0; i < BNCURL_STREAM_BUFFER_COUNT; i++) {
        stream_ctx->buffers[i].size = 0;
        stream_ctx->buffers[i].is_full = false;
        stream_ctx->buffers[i].is_streaming = false;
    }
    
    ESP_LOGI(TAG, "Stream context initialized with %d buffers of %d bytes each, output: %s", 
             BNCURL_STREAM_BUFFER_COUNT, BNCURL_STREAM_BUFFER_SIZE,
             stream_ctx->file_path ? stream_ctx->file_path : "UART");
}

bool bncurl_stream_buffer_to_output(bncurl_stream_context_t *stream_ctx, int buffer_index)
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
    
    bool success = false;
    
    if (stream_ctx->output_file) {
        // Write to file
        size_t written = fwrite(buffer->data, 1, buffer->size, stream_ctx->output_file);
        if (written == buffer->size) {
            // Flush to ensure data is written to disk
            fflush(stream_ctx->output_file);
            success = true;
            ESP_LOGI(TAG, "Wrote %u bytes to file: %s", (unsigned int)buffer->size, stream_ctx->file_path);
        } else {
            ESP_LOGE(TAG, "Failed to write to file: %s (wrote %u of %u bytes)", 
                     stream_ctx->file_path, (unsigned int)written, (unsigned int)buffer->size);
        }
    } else {
        // Write to UART (original behavior)
        // Send chunk length indication: +POST:1024,
        char chunk_header[32];
        snprintf(chunk_header, sizeof(chunk_header), "+POST:%u,", (unsigned int)buffer->size);
        esp_at_port_write_data((uint8_t *)chunk_header, strlen(chunk_header));
        
        // Send the actual data
        esp_at_port_write_data((uint8_t *)buffer->data, buffer->size);
        success = true;
        ESP_LOGI(TAG, "Streamed buffer %d to UART: %u bytes", buffer_index, (unsigned int)buffer->size);
    }
    
    if (success) {
        // Update streaming statistics
        stream_ctx->bytes_streamed += buffer->size;
        ESP_LOGI(TAG, "Total bytes streamed: %u", (unsigned int)stream_ctx->bytes_streamed);
    }
    
    // Reset buffer for reuse
    buffer->size = 0;
    buffer->is_full = false;
    buffer->is_streaming = false;
    stream_ctx->streaming_buffer = -1;
    
    return success;
}

void bncurl_stream_finalize(bncurl_stream_context_t *stream_ctx, bool success)
{
    if (!stream_ctx) {
        return;
    }
    
    // Close file if it was opened
    if (stream_ctx->output_file) {
        fclose(stream_ctx->output_file);
        stream_ctx->output_file = NULL;
        
        if (success) {
            ESP_LOGI(TAG, "File download completed successfully: %s (%u bytes)", 
                     stream_ctx->file_path, (unsigned int)stream_ctx->bytes_streamed);
        } else {
            ESP_LOGE(TAG, "File download failed: %s (%u bytes written)", 
                     stream_ctx->file_path, (unsigned int)stream_ctx->bytes_streamed);
        }
    }
    
    // Send completion message (always to UART for status)
    if (success) {
        const char *send_ok = "\r\nSEND OK\r\n";
        esp_at_port_write_data((uint8_t *)send_ok, strlen(send_ok));
        ESP_LOGI(TAG, "Operation completed successfully. Total bytes: %u", (unsigned int)stream_ctx->bytes_streamed);
    } else {
        const char *send_error = "\r\nSEND ERROR\r\n";
        esp_at_port_write_data((uint8_t *)send_error, strlen(send_error));
        ESP_LOGE(TAG, "Operation completed with error. Bytes processed: %u", (unsigned int)stream_ctx->bytes_streamed);
    }
    
    // Log final statistics
    ESP_LOGI(TAG, "Streaming statistics:");
    ESP_LOGI(TAG, "  Total size (if known): %u bytes", (unsigned int)stream_ctx->total_size);
    ESP_LOGI(TAG, "  Bytes streamed: %u bytes", (unsigned int)stream_ctx->bytes_streamed);
    ESP_LOGI(TAG, "  Output: %s", stream_ctx->file_path ? stream_ctx->file_path : "UART");
}
