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
#include <stdint.h>
#include "esp_at.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "BNCURL_HEAD";

// Structure to collect headers for length calculation and storage
typedef struct {
    char buffer[4096];  // Buffer to store all headers
    size_t total_size;  // Total size of headers
    size_t buffer_used; // How much of buffer is used
} head_collector_t;

// Callback to collect headers and calculate total length
static size_t head_collector_callback(char *buffer, size_t size, size_t nitems, void *userdata)
{
    head_collector_t *collector = (head_collector_t *)userdata;
    size_t total_size = size * nitems;
    
    // Only process HTTP headers (skip status line and empty lines)
    if (total_size > 2 && buffer[0] != '\r' && buffer[0] != '\n' &&
        strncmp(buffer, "HTTP/", 5) != 0) {
        
        // Clean up the header line by removing trailing CRLF
        char header_line[512];
        size_t copy_len = total_size < sizeof(header_line) - 3 ? total_size : sizeof(header_line) - 3;
        memcpy(header_line, buffer, copy_len);
        header_line[copy_len] = '\0';
        
        // Remove trailing \r\n
        char *end = header_line + strlen(header_line) - 1;
        while (end >= header_line && (*end == '\r' || *end == '\n')) {
            *end = '\0';
            end--;
        }
        
        // Add back clean CRLF for output and store in buffer
        if (strlen(header_line) > 0) {
            size_t clean_length = strlen(header_line) + 2; // +2 for \r\n
            
            // Store the cleaned header in our buffer if there's space
            if (collector->buffer_used + clean_length < sizeof(collector->buffer)) {
                memcpy(collector->buffer + collector->buffer_used, header_line, strlen(header_line));
                collector->buffer_used += strlen(header_line);
                collector->buffer[collector->buffer_used++] = '\r';
                collector->buffer[collector->buffer_used++] = '\n';
                collector->buffer[collector->buffer_used] = '\0';
            }
            
            collector->total_size += clean_length;
        }
    }
    
    return total_size; // Return original size to continue processing
}

// Simple verbose debug callback for HEAD requests
static int head_debug_callback(CURL *handle, curl_infotype type, char *data, size_t size, void *userptr)
{
    bncurl_context_t *ctx = (bncurl_context_t *)userptr;
    
    // Only process if verbose mode is enabled
    if (!ctx || !ctx->params.verbose) {
        return 0;
    }
    
    // Create a debug message prefix based on the info type
    const char *prefix = "";
    switch (type) {
        case CURLINFO_TEXT:
            prefix = "* ";
            break;
        case CURLINFO_HEADER_IN:
            prefix = "< ";
            break;
        case CURLINFO_HEADER_OUT:
            prefix = "> ";
            break;
        case CURLINFO_DATA_IN:
            prefix = "<< ";
            break;
        case CURLINFO_DATA_OUT:
            prefix = ">> ";
            break;
        case CURLINFO_SSL_DATA_IN:
        case CURLINFO_SSL_DATA_OUT:
            // Skip SSL data to avoid overwhelming output
            return 0;
        default:
            return 0;
    }
    
    // Process data line by line to add proper formatting
    char *data_copy = malloc(size + 1);
    if (!data_copy) {
        return 0;
    }
    
    memcpy(data_copy, data, size);
    data_copy[size] = '\0';
    
    // Split into lines and send each with prefix
    char *line = strtok(data_copy, "\r\n");
    while (line != NULL) {
        if (strlen(line) > 0) {
            // Create formatted debug line
            char debug_line[BNCURL_MAX_VERBOSE_LINE_LENGTH + 32];
            int line_len = snprintf(debug_line, sizeof(debug_line), "+VERBOSE:%s%s\r\n", prefix, line);
            
            if (line_len > 0 && line_len < sizeof(debug_line)) {
                esp_at_port_write_data((uint8_t *)debug_line, line_len);
            }
        }
        line = strtok(NULL, "\r\n");
    }
    
    free(data_copy);
    return 0;
}

bool bncurl_execute_head_request(bncurl_context_t *ctx)
{
    if (!ctx) {
        ESP_LOGE(TAG, "Invalid context");
        return false;
    }
    
    ESP_LOGI(TAG, "Starting HEAD request to: %s", ctx->params.url);
    
    // Single HEAD request to collect headers and calculate length
    CURL *curl = curl_easy_init();
    if (!curl) {
        ESP_LOGE(TAG, "Failed to initialize curl");
        return false;
    }
    
    head_collector_t collector = {0};
    
    // Configure curl
    curl_easy_setopt(curl, CURLOPT_URL, ctx->params.url);
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L); // HEAD request
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, head_collector_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &collector);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "ESP32-BN-Module/1.0");
    
    // Configure verbose debug output if -v parameter is enabled
    if (ctx->params.verbose) {
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, head_debug_callback);
        curl_easy_setopt(curl, CURLOPT_DEBUGDATA, ctx);
        ESP_LOGI(TAG, "Verbose mode enabled for HEAD request");
    }
    
    // Configure HTTPS settings if needed
    bool is_https = (strncmp(ctx->params.url, "https://", 8) == 0);
    if (is_https) {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    }
    
    ESP_LOGI(TAG, "Executing HEAD request...");
    CURLcode res = curl_easy_perform(curl);
    
    size_t header_length = SIZE_MAX; // Default to -1 if failed
    bool success = false;
    
    if (res == CURLE_OK) {
        long response_code;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
        
        if (response_code >= 200 && response_code < 300) {
            header_length = collector.total_size;
            success = true;
            ESP_LOGI(TAG, "HEAD request successful: %u bytes of headers", (unsigned int)header_length);
        } else {
            ESP_LOGW(TAG, "HEAD request failed with HTTP code: %ld", response_code);
        }
    } else {
        ESP_LOGW(TAG, "HEAD request failed: %s", curl_easy_strerror(res));
    }
    
    curl_easy_cleanup(curl);
    
    // Output +LEN marker with header length
    char len_marker[64];
    if (header_length == SIZE_MAX) {
        snprintf(len_marker, sizeof(len_marker), "+LEN:-1,\r\n");
        ESP_LOGI(TAG, "Header length unknown, sending +LEN:-1");
    } else {
        snprintf(len_marker, sizeof(len_marker), "+LEN:%u,\r\n", (unsigned int)header_length);
        ESP_LOGI(TAG, "Sending +LEN:%u for headers", (unsigned int)header_length);
    }
    esp_at_port_write_data((uint8_t *)len_marker, strlen(len_marker));
    
    // Stream the collected headers via +POST markers if we have them
    if (success && collector.buffer_used > 0) {
        ESP_LOGI(TAG, "Streaming %u bytes of headers", (unsigned int)collector.buffer_used);
        
        // Stream headers in chunks using the same format as other requests
        size_t bytes_sent = 0;
        const size_t chunk_size = 512; // Stream in chunks
        
        while (bytes_sent < collector.buffer_used) {
            size_t remaining = collector.buffer_used - bytes_sent;
            size_t current_chunk = remaining > chunk_size ? chunk_size : remaining;
            
            // Format: +POST:<chunk_size>,<data>
            char post_marker[32];
            snprintf(post_marker, sizeof(post_marker), "+POST:%u,", (unsigned int)current_chunk);
            esp_at_port_write_data((uint8_t *)post_marker, strlen(post_marker));
            
            // Send the header data chunk
            esp_at_port_write_data((uint8_t *)(collector.buffer + bytes_sent), current_chunk);
            
            bytes_sent += current_chunk;
            
            // Small delay between chunks to prevent overwhelming UART
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        
        ESP_LOGI(TAG, "Header streaming completed");
    }
    
    // Send completion status
    if (success) {
        const char *send_ok = "\r\nSEND OK\r\n";
        esp_at_port_write_data((uint8_t *)send_ok, strlen(send_ok));
        ESP_LOGI(TAG, "HEAD request completed successfully");
    } else {
        const char *send_error = "\r\nSEND ERROR\r\n";
        esp_at_port_write_data((uint8_t *)send_error, strlen(send_error));
        ESP_LOGE(TAG, "HEAD request completed with error");
    }
    
    return success;
}
