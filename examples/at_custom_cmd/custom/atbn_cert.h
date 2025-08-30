/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Get the CA bundle PEM data
 * 
 * @return Pointer to the CA bundle PEM string
 */
const char* atbn_cert_get_ca_bundle(void);

/**
 * @brief Get the size of the CA bundle PEM data
 * 
 * @return Size of the CA bundle PEM data in bytes
 */
size_t atbn_cert_get_ca_bundle_size(void);

#ifdef __cplusplus
}
#endif
