/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BNKILL_H
#define BNKILL_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Kill switch configuration
 * 
 * This module implements a time-based kill switch for demo/trial firmware.
 * After the specified kill date, the firmware will refuse to execute BNCURL commands.
 */

/* ==================== Kill Switch Configuration ==================== */

/** Kill date - firmware expires after this date (YYYY-MM-DD format) */
#define BNKILL_EXPIRY_DATE_YEAR     2025
#define BNKILL_EXPIRY_DATE_MONTH    9       // September
#define BNKILL_EXPIRY_DATE_DAY      20      // 20th

/** Kill date as string for logging */
#define BNKILL_EXPIRY_DATE_STR      "2025-09-20"

/* ==================== Kill Switch States ==================== */

typedef enum {
    BNKILL_STATE_UNCHECKED = 0,     ///< Not yet checked this boot
    BNKILL_STATE_ACTIVE,            ///< Firmware is active (before kill date)
    BNKILL_STATE_EXPIRED,           ///< Firmware has expired (after kill date)
    BNKILL_STATE_CHECK_FAILED       ///< Could not get server time (allowing operation)
} bnkill_state_t;

/* ==================== Public Functions ==================== */

/**
 * @brief Initialize kill switch subsystem
 * 
 * @return true on success, false on failure
 */
bool bnkill_init(void);

/**
 * @brief Deinitialize kill switch subsystem
 * 
 * Cleans up NTP client and resets state.
 */
void bnkill_deinit(void);

/**
 * @brief Check if firmware has expired
 * 
 * This function checks the current time (from NTP server) against the kill date.
 * It only performs the check once per boot session.
 * 
 * @return true if firmware is still valid, false if expired
 */
bool bnkill_check_expiry(void);

/**
 * @brief Get current kill switch state
 * 
 * @return Current state of the kill switch
 */
bnkill_state_t bnkill_get_state(void);

/**
 * @brief Get kill switch status string for logging
 * 
 * @return Status string describing current state
 */
const char* bnkill_get_status_string(void);

/**
 * @brief Force a recheck on next call (for testing)
 * 
 * This resets the state to UNCHECKED, causing the next
 * bnkill_check_expiry() call to perform a fresh check.
 */
void bnkill_reset_state(void);

#endif // BNKILL_H
