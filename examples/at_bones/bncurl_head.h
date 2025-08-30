/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BNCURL_HEAD_H
#define BNCURL_HEAD_H

#include <stdbool.h>
#include "bncurl.h"

/**
 * @brief Execute HEAD request
 * 
 * @param ctx BNCURL context containing request parameters
 * @return true on success, false on failure
 */
bool bncurl_execute_head_request(bncurl_context_t *ctx);

#endif /* BNCURL_HEAD_H */
