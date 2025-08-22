/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include "esp_at_core.h"
#include "esp_at.h"

/**
 * @brief You can include more header files here for your own AT commands.
 */

// SD Card SPI pin configuration - modify these according to your board layout
#ifndef CONFIG_EXAMPLE_PIN_MISO
#define CONFIG_EXAMPLE_PIN_MISO  19
#endif

#ifndef CONFIG_EXAMPLE_PIN_MOSI
#define CONFIG_EXAMPLE_PIN_MOSI  23
#endif

#ifndef CONFIG_EXAMPLE_PIN_CLK
#define CONFIG_EXAMPLE_PIN_CLK   18
#endif

#ifndef CONFIG_EXAMPLE_PIN_CS
#define CONFIG_EXAMPLE_PIN_CS    5
#endif
