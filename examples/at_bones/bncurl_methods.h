/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BNCURL_METHODS_H
#define BNCURL_METHODS_H

#include <stdbool.h>
#include <stdio.h>
#include "bncurl.h"

#include "bncurl_config.h"

// Deferred fsync interval for performance optimization
#define BNCURL_FSYNC_INTERVAL (128 * 1024)  // 128KB (reduced for better data safety)

// Buffer structure for dual-buffer streaming
#define BNCURL_STREAM_BUFFER_SIZE   (4 * 1024)   // 4KB per buffer (8KB total, safe with SSL operations)
#define BNCURL_STREAM_BUFFER_COUNT  2             // Two buffers for ping-pong

// Streaming buffer structure
typedef struct {
    char data[BNCURL_STREAM_BUFFER_SIZE];  // Back to static allocation for performance
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
    int output_fd;          // File descriptor for download (-1 for UART output)
    char *file_path;        // Path to output file (NULL for UART output)
    bool is_range_request;  // True if this is a range request
    size_t deferred_flush_bytes; // Bytes accumulated since last fsync
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
 * @param ctx BNCURL context containing download file path (optional)
 */
void bncurl_stream_init(bncurl_stream_context_t *stream_ctx, bncurl_context_t *ctx);

/**
 * @brief Initialize streaming context with range support
 * 
 * @param stream_ctx Streaming context to initialize
 * @param ctx BNCURL context containing download file path and range parameters
 * @param is_range_request True if this is a range request requiring append mode
 */
void bncurl_stream_init_with_range(bncurl_stream_context_t *stream_ctx, bncurl_context_t *ctx, bool is_range_request);

/**
 * @brief Stream buffer data to output (UART or file)
 * 
 * @param stream_ctx Streaming context
 * @param buffer_index Buffer index to stream (0 or 1)
 * @return true on success, false on failure
 */
bool bncurl_stream_buffer_to_output(bncurl_stream_context_t *stream_ctx, int buffer_index);

/**
 * @brief Finalize streaming and send completion message
 * 
 * @param stream_ctx Streaming context
 * @param success Whether the overall operation was successful
 */
void bncurl_stream_finalize(bncurl_stream_context_t *stream_ctx, bool success);

#endif /* BNCURL_METHODS_H */
