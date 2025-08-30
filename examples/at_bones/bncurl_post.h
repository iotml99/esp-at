/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BNCURL_POST_H
#define BNCURL_POST_H

#include <stdbool.h>
#include "bncurl.h"

/**
 * @brief Execute POST request
 * 
 * @param ctx BNCURL context containing request parameters
 * @return true on success, false on failure
 */
bool bncurl_execute_post_request(bncurl_context_t *ctx);

#endif /* BNCURL_POST_H */
