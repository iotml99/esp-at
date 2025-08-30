/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BNCURL_GET_H
#define BNCURL_GET_H

#include <stdbool.h>
#include "bncurl.h"
#include "bncurl_methods.h"

// GET request context structure
typedef struct {
    bncurl_context_t *ctx;              // Main BNCURL context
    bncurl_stream_context_t stream;     // Streaming context for dual-buffer management
} bncurl_get_context_t;

/**
 * @brief Execute GET request with dual-buffer streaming
 * 
 * This function performs an HTTP GET request using curl and streams the response
 * data to UART using a dual-buffer approach. Data is streamed in 1KB chunks
 * as each buffer fills up.
 * 
 * @param ctx BNCURL context containing request parameters
 * @return true on success (HTTP 200), false on failure
 */
bool bncurl_execute_get_request(bncurl_context_t *ctx);

#endif /* BNCURL_GET_H */
