/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BNCURL_METHODS_H
#define BNCURL_METHODS_H

#include <stdbool.h>
#include "bncurl.h"

// Buffer configuration for dual-buffer streaming
#define BNCURL_STREAM_BUFFER_SIZE   512   // 512 bytes per buffer (or lower as requested)
#define BNCURL_STREAM_BUFFER_COUNT  2     // Two buffers for ping-pong

// Streaming buffer structure
typedef struct {
    char data[BNCURL_STREAM_BUFFER_SIZE];
    size_t size;
    bool is_full;
    bool is_streaming;
} bncurl_stream_buffer_t;

// Streaming context for dual-buffer management
typedef struct {
    bncurl_stream_buffer_t buffers[BNCURL_STREAM_BUFFER_COUNT];
    int active_buffer;      // Currently filling buffer (0 or 1)
    int streaming_buffer;   // Currently streaming buffer (-1 if none)
    size_t total_size;      // Total content size (if known)
    size_t bytes_streamed;  // Total bytes already streamed
} bncurl_stream_context_t;

/**
 * @brief Execute GET request with dual-buffer streaming
 * 
 * @param ctx BNCURL context containing request parameters
 * @return true on success, false on failure
 */
bool bncurl_execute_get_request(bncurl_context_t *ctx);

/**
 * @brief Execute POST request with dual-buffer streaming
 * 
 * @param ctx BNCURL context containing request parameters
 * @return true on success, false on failure
 */
bool bncurl_execute_post_request(bncurl_context_t *ctx);

/**
 * @brief Execute HEAD request
 * 
 * @param ctx BNCURL context containing request parameters
 * @return true on success, false on failure
 */
bool bncurl_execute_head_request(bncurl_context_t *ctx);

/**
 * @brief Initialize streaming context
 * 
 * @param stream_ctx Streaming context to initialize
 */
void bncurl_stream_init(bncurl_stream_context_t *stream_ctx);

/**
 * @brief Stream buffer data to UART
 * 
 * @param stream_ctx Streaming context
 * @param buffer_index Buffer index to stream (0 or 1)
 * @return true on success, false on failure
 */
bool bncurl_stream_buffer_to_uart(bncurl_stream_context_t *stream_ctx, int buffer_index);

/**
 * @brief Finalize streaming and send completion message
 * 
 * @param stream_ctx Streaming context
 * @param success Whether the overall operation was successful
 */
void bncurl_stream_finalize(bncurl_stream_context_t *stream_ctx, bool success);

#endif /* BNCURL_METHODS_H */
