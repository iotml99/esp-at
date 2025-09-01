/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bnkill.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

static const char *TAG = "BNKILL";

// NTP configuration
#define BNKILL_NTP_SERVER_1     "pool.ntp.org"
#define BNKILL_NTP_SERVER_2     "time.nist.gov"
#define BNKILL_NTP_TIMEOUT_MS   10000  // 10 seconds timeout for NTP sync

// Kill switch state - persists for the entire boot session
static bnkill_state_t s_kill_state = BNKILL_STATE_UNCHECKED;
static bool s_kill_initialized = false;
static bool s_ntp_initialized = false;
static bool s_ntp_init_attempted = false;

// Month names for HTTP date parsing
static const char* month_names[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

/**
 * @brief Initialize NTP client
 * 
 * @return true on success, false on failure
 */
static bool bnkill_init_ntp(void)
{
    if (s_ntp_initialized) {
        ESP_LOGI(TAG, "NTP already initialized for kill switch");
        return true;
    }
    
    // Prevent multiple initialization attempts that could cause conflicts
    if (s_ntp_init_attempted) {
        ESP_LOGW(TAG, "NTP initialization already attempted, using cached result");
        return s_ntp_initialized;
    }
    
    s_ntp_init_attempted = true;
    ESP_LOGI(TAG, "Attempting SNTP client initialization for kill switch");
    
    // Check if SNTP is already running (from other parts of the system)
    if (esp_sntp_enabled()) {
        ESP_LOGI(TAG, "SNTP already enabled by another component, reusing existing configuration");
        s_ntp_initialized = true;
        return true;
    }
    
    // Set timezone to UTC for consistent kill switch operation before SNTP init
    setenv("TZ", "UTC0", 1);
    tzset();
    
    // Wrap SNTP initialization in try-catch equivalent
    ESP_LOGI(TAG, "Setting SNTP operating mode and servers");
    
    // Initialize SNTP configuration
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, BNKILL_NTP_SERVER_1);
    esp_sntp_setservername(1, BNKILL_NTP_SERVER_2);
    
    ESP_LOGI(TAG, "Starting SNTP service");
    
    // Start SNTP service
    esp_sntp_init();
    
    // Verify initialization with a small delay
    vTaskDelay(pdMS_TO_TICKS(100));
    
    if (!esp_sntp_enabled()) {
        ESP_LOGE(TAG, "Failed to enable SNTP client after initialization");
        return false;
    }
    
    s_ntp_initialized = true;
    ESP_LOGI(TAG, "SNTP client successfully initialized with servers: %s, %s", 
             BNKILL_NTP_SERVER_1, BNKILL_NTP_SERVER_2);
    
    return true;
}

/**
 * @brief Get current time from NTP server
 * 
 * @param current_time Output timestamp
 * @return true if time successfully obtained, false otherwise
 */
static bool bnkill_get_ntp_time(time_t *current_time)
{
    if (!current_time) {
        return false;
    }
    
    // Initialize NTP if not already done
    if (!bnkill_init_ntp()) {
        ESP_LOGE(TAG, "Failed to initialize NTP");
        return false;
    }
    
    // Wait for time to be set by NTP
    printf("BNKILL: Waiting for NTP time synchronization...\n");
    ESP_LOGW(TAG, "Waiting for NTP time synchronization...");
    
    int retry = 0;
    const int max_retry = BNKILL_NTP_TIMEOUT_MS / 100;  // Check every 100ms
    
    while (retry < max_retry) {
        time_t now = 0;
        struct tm timeinfo = {0};
        time(&now);
        localtime_r(&now, &timeinfo);
        
        // Check if time is set (year should be > 2020)
        if (timeinfo.tm_year > (2020 - 1900)) {
            *current_time = now;
            printf("BNKILL: NTP time synchronized: %04d-%02d-%02d %02d:%02d:%02d UTC\n",
                     timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                     timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
            ESP_LOGI(TAG, "NTP time synchronized: %04d-%02d-%02d %02d:%02d:%02d UTC",
                     timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                     timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
            return true;
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
        retry++;
    }
    
    printf("BNKILL: NTP synchronization timeout after %d ms\n", BNKILL_NTP_TIMEOUT_MS);
    ESP_LOGW(TAG, "NTP synchronization timeout after %d ms", BNKILL_NTP_TIMEOUT_MS);
    return false;
}

/**
 * @brief Parse HTTP date header to extract timestamp
 * 
 * Supports format: "Wed, 20 Sep 2025 14:30:00 GMT"
 * 
 * @param http_date HTTP date string
 * @param timestamp Output timestamp
 * @return true on successful parsing, false otherwise
 */
static bool parse_http_date(const char *http_date, time_t *timestamp)
{
    if (!http_date || !timestamp) {
        return false;
    }
    
    struct tm tm_time = {0};
    char day_name[4] = {0};
    char month_name[4] = {0};
    char tz[4] = {0};
    
    // Parse: "Wed, 20 Sep 2025 14:30:00 GMT"
    int parsed = sscanf(http_date, "%3s, %d %3s %d %d:%d:%d %3s",
                       day_name, &tm_time.tm_mday, month_name, 
                       &tm_time.tm_year, &tm_time.tm_hour, 
                       &tm_time.tm_min, &tm_time.tm_sec, tz);
                       
    if (parsed != 8) {
        ESP_LOGW(TAG, "Failed to parse HTTP date: %s", http_date);
        return false;
    }
    
    // Convert year (2025 -> 125)
    tm_time.tm_year -= 1900;
    
    // Convert month name to number
    tm_time.tm_mon = -1;
    for (int i = 0; i < 12; i++) {
        if (strcmp(month_name, month_names[i]) == 0) {
            tm_time.tm_mon = i;
            break;
        }
    }
    
    if (tm_time.tm_mon == -1) {
        ESP_LOGW(TAG, "Unknown month: %s", month_name);
        return false;
    }
    
    // Convert to timestamp (assuming GMT/UTC)
    *timestamp = mktime(&tm_time);
    
    if (*timestamp == -1) {
        ESP_LOGW(TAG, "Failed to convert to timestamp");
        return false;
    }
    
    ESP_LOGI(TAG, "Parsed server date: %04d-%02d-%02d %02d:%02d:%02d", 
             tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
             tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec);
    
    return true;
}

/**
 * @brief Create kill date timestamp
 * 
 * @return Kill date as timestamp
 */
static time_t create_kill_timestamp(void)
{
    struct tm kill_tm = {0};
    kill_tm.tm_year = BNKILL_EXPIRY_DATE_YEAR - 1900;
    kill_tm.tm_mon = BNKILL_EXPIRY_DATE_MONTH - 1;
    kill_tm.tm_mday = BNKILL_EXPIRY_DATE_DAY;
    kill_tm.tm_hour = 0;
    kill_tm.tm_min = 0;
    kill_tm.tm_sec = 0;
    
    time_t kill_time = mktime(&kill_tm);
    ESP_LOGI(TAG, "Kill date configured: %s (timestamp: %ld)", 
             BNKILL_EXPIRY_DATE_STR, kill_time);
    
    return kill_time;
}

bool bnkill_init(void)
{
    if (s_kill_initialized) {
        ESP_LOGW(TAG, "Kill switch already initialized");
        return true;
    }
    
    ESP_LOGI(TAG, "Initializing kill switch subsystem");
    ESP_LOGI(TAG, "Firmware expiry date: %s", BNKILL_EXPIRY_DATE_STR);
    ESP_LOGI(TAG, "Kill switch policy: Check once per boot, fail-open on time error");
    
    s_kill_state = BNKILL_STATE_UNCHECKED;
    s_kill_initialized = true;
    
    return true;
}

bool bnkill_check_expiry(void)
{
    if (!s_kill_initialized) {
        ESP_LOGE(TAG, "Kill switch not initialized");
        return false;
    }
    
    // If already checked this boot, return cached result
    if (s_kill_state != BNKILL_STATE_UNCHECKED) {
        bool is_active = (s_kill_state == BNKILL_STATE_ACTIVE || 
                         s_kill_state == BNKILL_STATE_CHECK_FAILED);
        
        // Always log current time for debugging even when using cached result
        time_t current_time = 0;
        time(&current_time);
        struct tm current_tm = {0};
        localtime_r(&current_time, &current_tm);
        
        // printf("BNKILL: Kill switch check (CACHED): Current time %04d-%02d-%02d %02d:%02d:%02d UTC\n",
        //          current_tm.tm_year + 1900, current_tm.tm_mon + 1, current_tm.tm_mday,
        //          current_tm.tm_hour, current_tm.tm_min, current_tm.tm_sec);
        
        // ESP_LOGI(TAG, "Kill switch check (CACHED): Current time %04d-%02d-%02d %02d:%02d:%02d UTC",
        //          current_tm.tm_year + 1900, current_tm.tm_mon + 1, current_tm.tm_mday,
        //          current_tm.tm_hour, current_tm.tm_min, current_tm.tm_sec);
        
        if (s_kill_state == BNKILL_STATE_EXPIRED) {
            // printf("BNKILL: FIRMWARE EXPIRED - Operation denied (cached result)\n");
            // ESP_LOGE(TAG, "FIRMWARE EXPIRED - Operation denied (cached result)");
        } else {
            // printf("BNKILL: Kill switch status: %s (cached)\n", is_active ? "ACTIVE" : "EXPIRED");
            // ESP_LOGI(TAG, "Kill switch status: %s (cached)", 
            //          is_active ? "ACTIVE" : "EXPIRED");
        }
        
        return is_active;
    }
    
    // First check this boot
    // printf("BNKILL: Performing kill switch check...\n");
    // ESP_LOGI(TAG, "Performing kill switch check...");
    
    // Get kill date timestamp
    time_t kill_timestamp = create_kill_timestamp();
    if (kill_timestamp == -1) {
        // printf("BNKILL: Failed to create kill timestamp - allowing operation\n");
        // ESP_LOGE(TAG, "Failed to create kill timestamp - allowing operation");
        s_kill_state = BNKILL_STATE_CHECK_FAILED;
        return true;
    }
    
    // Always log expiry date for reference
    struct tm kill_tm = {0};
    localtime_r(&kill_timestamp, &kill_tm);
    
    // printf("BNKILL: Kill switch expiry: %04d-%02d-%02d %02d:%02d:%02d UTC (timestamp: %ld)\n",
    //          kill_tm.tm_year + 1900, kill_tm.tm_mon + 1, kill_tm.tm_mday,
    //          kill_tm.tm_hour, kill_tm.tm_min, kill_tm.tm_sec, kill_timestamp);
    
    // ESP_LOGI(TAG, "Kill switch expiry: %04d-%02d-%02d %02d:%02d:%02d UTC (timestamp: %ld)",
    //          kill_tm.tm_year + 1900, kill_tm.tm_mon + 1, kill_tm.tm_mday,
    //          kill_tm.tm_hour, kill_tm.tm_min, kill_tm.tm_sec, kill_timestamp);
    
    // Try to get current time from NTP
    time_t current_timestamp = 0;
    if (bnkill_get_ntp_time(&current_timestamp)) {
        // Successfully got NTP time - log both times for comparison
        struct tm current_tm = {0};
        localtime_r(&current_timestamp, &current_tm);
        
        // printf("BNKILL: Current NTP time: %04d-%02d-%02d %02d:%02d:%02d UTC (timestamp: %ld)\n",
        //          current_tm.tm_year + 1900, current_tm.tm_mon + 1, current_tm.tm_mday,
        //          current_tm.tm_hour, current_tm.tm_min, current_tm.tm_sec, current_timestamp);
        
        // printf("BNKILL: Time comparison: Current=%ld, Expiry=%ld, Difference=%ld seconds\n",
        //          current_timestamp, kill_timestamp, kill_timestamp - current_timestamp);
        
        // ESP_LOGI(TAG, "Current NTP time: %04d-%02d-%02d %02d:%02d:%02d UTC (timestamp: %ld)",
        //          current_tm.tm_year + 1900, current_tm.tm_mon + 1, current_tm.tm_mday,
        //          current_tm.tm_hour, current_tm.tm_min, current_tm.tm_sec, current_timestamp);
        
        // ESP_LOGI(TAG, "Time comparison: Current=%ld, Expiry=%ld, Difference=%ld seconds",
        //          current_timestamp, kill_timestamp, kill_timestamp - current_timestamp);
        
        if (current_timestamp >= kill_timestamp) {
            // Firmware has expired
            // printf("BNKILL: FIRMWARE EXPIRED - Current date (%ld) >= Kill date (%ld)\n", 
            //          current_timestamp, kill_timestamp);
            // printf("BNKILL: This firmware expired on %s\n", BNKILL_EXPIRY_DATE_STR);
            // printf("BNKILL: Please contact vendor for updated firmware\n");
            
            // ESP_LOGE(TAG, "FIRMWARE EXPIRED - Current date (%ld) >= Kill date (%ld)", 
            //          current_timestamp, kill_timestamp);
            // ESP_LOGE(TAG, "This firmware expired on %s", BNKILL_EXPIRY_DATE_STR);
            // ESP_LOGE(TAG, "Please contact vendor for updated firmware");
            
            s_kill_state = BNKILL_STATE_EXPIRED;
            return false;
        } else {
            // Firmware is still active
            long days_remaining = (kill_timestamp - current_timestamp) / (24 * 60 * 60);
            // printf("BNKILL: Firmware is ACTIVE - %ld days remaining until %s\n", 
            //          days_remaining, BNKILL_EXPIRY_DATE_STR);
            
            // ESP_LOGI(TAG, "Firmware is ACTIVE - %ld days remaining until %s", 
            //          days_remaining, BNKILL_EXPIRY_DATE_STR);
            
            s_kill_state = BNKILL_STATE_ACTIVE;
            return true;
        }
    } else {
        // Could not get NTP time - fail open (allow operation)
        // printf("BNKILL: Could not synchronize with NTP server\n");
        // printf("BNKILL: Allowing operation (fail-open policy)\n");
        
        // ESP_LOGW(TAG, "Could not synchronize with NTP server");
        // ESP_LOGW(TAG, "Allowing operation (fail-open policy)");
        
        s_kill_state = BNKILL_STATE_CHECK_FAILED;
        return true;
    }
}

bnkill_state_t bnkill_get_state(void)
{
    return s_kill_state;
}

const char* bnkill_get_status_string(void)
{
    switch (s_kill_state) {
        case BNKILL_STATE_UNCHECKED:
            return "UNCHECKED";
        case BNKILL_STATE_ACTIVE:
            return "ACTIVE";
        case BNKILL_STATE_EXPIRED:
            return "EXPIRED";
        case BNKILL_STATE_CHECK_FAILED:
            return "CHECK_FAILED";
        default:
            return "UNKNOWN";
    }
}

void bnkill_reset_state(void)
{
    // ESP_LOGW(TAG, "Resetting kill switch state to UNCHECKED");
    s_kill_state = BNKILL_STATE_UNCHECKED;
}

void bnkill_deinit(void)
{
    if (!s_kill_initialized) {
        return;
    }
    
    ESP_LOGI(TAG, "Deinitializing kill switch subsystem");
    
    // Only stop SNTP if we initialized it
    if (s_ntp_initialized && esp_sntp_enabled()) {
        ESP_LOGI(TAG, "Stopping SNTP client");
        esp_sntp_stop();
    }
    
    s_ntp_initialized = false;
    s_kill_initialized = false;
    s_kill_state = BNKILL_STATE_UNCHECKED;
}
