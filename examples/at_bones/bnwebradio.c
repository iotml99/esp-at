#include "bnwebradio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "curl/curl.h"
#include "esp_at.h"
#include "bncert_manager.h"
#include <string.h>

static const char *TAG = "BNWEBRADIO";

// Global webradio context
static bnwebradio_context_t g_webradio_ctx = {0};
static SemaphoreHandle_t g_webradio_mutex = NULL;
static TaskHandle_t g_webradio_task = NULL;

// Forward declarations
static void webradio_task(void *pvParameters);
static size_t webradio_write_callback(void *contents, size_t size, size_t nmemb, void *userp);
static int webradio_progress_callback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, 
                                     curl_off_t ultotal, curl_off_t ulnow);

bool bnwebradio_init(void)
{
    if (g_webradio_mutex != NULL) {
        ESP_LOGW(TAG, "Web radio already initialized");
        return true;
    }

    // Create mutex for thread safety
    g_webradio_mutex = xSemaphoreCreateMutex();
    if (g_webradio_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create webradio mutex");
        return false;
    }

    // Initialize context
    memset(&g_webradio_ctx, 0, sizeof(bnwebradio_context_t));
    g_webradio_ctx.state = WEBRADIO_STATE_IDLE;

    ESP_LOGI(TAG, "Web radio module initialized");
    return true;
}

void bnwebradio_deinit(void)
{
    // Stop any active streaming
    bnwebradio_stop();

    // Clean up mutex
    if (g_webradio_mutex != NULL) {
        vSemaphoreDelete(g_webradio_mutex);
        g_webradio_mutex = NULL;
    }

    ESP_LOGI(TAG, "Web radio module deinitialized");
}

bool bnwebradio_start(const char *url)
{
    if (!url || strlen(url) == 0) {
        ESP_LOGE(TAG, "Invalid URL provided");
        return false;
    }

    if (g_webradio_mutex == NULL) {
        ESP_LOGE(TAG, "Web radio not initialized");
        return false;
    }

    xSemaphoreTake(g_webradio_mutex, portMAX_DELAY);

    // Check if already streaming
    if (g_webradio_ctx.is_active) {
        ESP_LOGW(TAG, "Web radio already streaming, stopping current stream");
        bnwebradio_stop();
    }

    // Initialize context for new stream
    strncpy(g_webradio_ctx.url, url, sizeof(g_webradio_ctx.url) - 1);
    g_webradio_ctx.url[sizeof(g_webradio_ctx.url) - 1] = '\0';
    g_webradio_ctx.is_active = true;
    g_webradio_ctx.state = WEBRADIO_STATE_CONNECTING;
    g_webradio_ctx.bytes_streamed = 0;
    g_webradio_ctx.start_time = esp_timer_get_time() / 1000; // Convert to milliseconds
    g_webradio_ctx.stop_requested = false;

    // Create streaming task
    BaseType_t ret = xTaskCreate(webradio_task, "webradio_task", 8192, NULL, 5, &g_webradio_task);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create webradio task");
        g_webradio_ctx.is_active = false;
        g_webradio_ctx.state = WEBRADIO_STATE_ERROR;
        xSemaphoreGive(g_webradio_mutex);
        return false;
    }

    xSemaphoreGive(g_webradio_mutex);
    ESP_LOGI(TAG, "Web radio streaming started for URL: %s", url);
    return true;
}

bool bnwebradio_stop(void)
{
    if (g_webradio_mutex == NULL) {
        ESP_LOGE(TAG, "Web radio not initialized");
        return false;
    }

    xSemaphoreTake(g_webradio_mutex, portMAX_DELAY);

    if (!g_webradio_ctx.is_active) {
        ESP_LOGW(TAG, "Web radio not active");
        xSemaphoreGive(g_webradio_mutex);
        return true;
    }

    // Signal stop request
    g_webradio_ctx.stop_requested = true;
    g_webradio_ctx.state = WEBRADIO_STATE_STOPPING;

    xSemaphoreGive(g_webradio_mutex);

    // Wait for task to finish
    if (g_webradio_task != NULL) {
        // Wait up to 5 seconds for graceful shutdown
        for (int i = 0; i < 50; i++) {
            if (g_webradio_task == NULL) {
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(100));
        }

        // Force delete if still running
        if (g_webradio_task != NULL) {
            ESP_LOGW(TAG, "Force terminating webradio task");
            vTaskDelete(g_webradio_task);
            g_webradio_task = NULL;
        }
    }

    xSemaphoreTake(g_webradio_mutex, portMAX_DELAY);
    g_webradio_ctx.is_active = false;
    g_webradio_ctx.state = WEBRADIO_STATE_IDLE;
    xSemaphoreGive(g_webradio_mutex);

    ESP_LOGI(TAG, "Web radio streaming stopped");
    return true;
}

webradio_state_t bnwebradio_get_state(void)
{
    if (g_webradio_mutex == NULL) {
        return WEBRADIO_STATE_ERROR;
    }

    xSemaphoreTake(g_webradio_mutex, portMAX_DELAY);
    webradio_state_t state = g_webradio_ctx.state;
    xSemaphoreGive(g_webradio_mutex);

    return state;
}

bool bnwebradio_get_stats(size_t *bytes_streamed, uint32_t *duration_ms)
{
    if (!bytes_streamed || !duration_ms || g_webradio_mutex == NULL) {
        return false;
    }

    xSemaphoreTake(g_webradio_mutex, portMAX_DELAY);

    if (!g_webradio_ctx.is_active) {
        xSemaphoreGive(g_webradio_mutex);
        return false;
    }

    *bytes_streamed = g_webradio_ctx.bytes_streamed;
    uint32_t current_time = esp_timer_get_time() / 1000;
    *duration_ms = current_time - g_webradio_ctx.start_time;

    xSemaphoreGive(g_webradio_mutex);
    return true;
}

bool bnwebradio_is_active(void)
{
    if (g_webradio_mutex == NULL) {
        return false;
    }

    xSemaphoreTake(g_webradio_mutex, portMAX_DELAY);
    bool active = g_webradio_ctx.is_active;
    xSemaphoreGive(g_webradio_mutex);

    return active;
}

static void webradio_task(void *pvParameters)
{
    CURL *curl = NULL;
    CURLcode res;

    ESP_LOGI(TAG, "Web radio task started");

    // Initialize CURL
    curl = curl_easy_init();
    if (!curl) {
        ESP_LOGE(TAG, "Failed to initialize CURL");
        goto task_cleanup;
    }

    // Store curl handle in context
    xSemaphoreTake(g_webradio_mutex, portMAX_DELAY);
    g_webradio_ctx.curl_handle = curl;
    xSemaphoreGive(g_webradio_mutex);

    // Configure CURL for streaming
    curl_easy_setopt(curl, CURLOPT_URL, g_webradio_ctx.url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, webradio_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, NULL);
    curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, webradio_progress_callback);
    curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, NULL);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    
    // Set user agent
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "ESP32-WebRadio/1.0");
    
    // Follow redirects
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    
    // Set timeouts
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0L); // No timeout for streaming
    
    // Configure for audio streaming
    curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, 8192L); // 8KB buffer for audio
    curl_easy_setopt(curl, CURLOPT_TCP_NODELAY, 1L);
    
    // Configure HTTPS/TLS if needed
    if (strncmp(g_webradio_ctx.url, "https://", 8) == 0) {
        ESP_LOGI(TAG, "HTTPS stream detected, configuring SSL");
        
        // Try certificate manager integration first
        bool ssl_configured = false;
        if (bncert_manager_init()) {
            // Use partition certificates if available
            size_t cert_count = bncert_manager_get_cert_count();
            if (cert_count > 0) {
                ESP_LOGI(TAG, "Using certificate manager for SSL");
                // Note: Certificate manager integration would be added here
                // For now, use permissive settings
            }
        }
        
        if (!ssl_configured) {
            // Use permissive SSL settings for audio streams
            ESP_LOGI(TAG, "Using permissive SSL for web radio");
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        }
    }

    // Update state to streaming
    xSemaphoreTake(g_webradio_mutex, portMAX_DELAY);
    g_webradio_ctx.state = WEBRADIO_STATE_STREAMING;
    xSemaphoreGive(g_webradio_mutex);

    ESP_LOGI(TAG, "Starting audio stream from: %s", g_webradio_ctx.url);

    // Start the streaming
    res = curl_easy_perform(curl);

    // Check if stopped gracefully or due to error
    xSemaphoreTake(g_webradio_mutex, portMAX_DELAY);
    if (g_webradio_ctx.stop_requested) {
        ESP_LOGI(TAG, "Web radio streaming stopped by user request");
        g_webradio_ctx.state = WEBRADIO_STATE_IDLE;
    } else {
        ESP_LOGE(TAG, "Web radio streaming ended with error: %s", curl_easy_strerror(res));
        g_webradio_ctx.state = WEBRADIO_STATE_ERROR;
    }
    xSemaphoreGive(g_webradio_mutex);

task_cleanup:
    // Cleanup CURL
    if (curl) {
        curl_easy_cleanup(curl);
    }

    // Update context
    xSemaphoreTake(g_webradio_mutex, portMAX_DELAY);
    g_webradio_ctx.curl_handle = NULL;
    g_webradio_ctx.is_active = false;
    if (g_webradio_ctx.state != WEBRADIO_STATE_ERROR) {
        g_webradio_ctx.state = WEBRADIO_STATE_IDLE;
    }
    xSemaphoreGive(g_webradio_mutex);

    // Clear task handle
    g_webradio_task = NULL;

    ESP_LOGI(TAG, "Web radio task ended, streamed %zu bytes", g_webradio_ctx.bytes_streamed);
    vTaskDelete(NULL);
}

static size_t webradio_write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t total_size = size * nmemb;
    
    // Check if stop was requested
    xSemaphoreTake(g_webradio_mutex, portMAX_DELAY);
    bool stop_requested = g_webradio_ctx.stop_requested;
    xSemaphoreGive(g_webradio_mutex);
    
    if (stop_requested) {
        ESP_LOGI(TAG, "Stop requested, terminating stream");
        return 0; // This will cause CURL to abort
    }

    // Stream raw audio data directly to UART
    if (total_size > 0) {
        esp_at_port_write_data((uint8_t *)contents, total_size);
        
        // Update statistics
        xSemaphoreTake(g_webradio_mutex, portMAX_DELAY);
        g_webradio_ctx.bytes_streamed += total_size;
        xSemaphoreGive(g_webradio_mutex);
    }

    return total_size;
}

static int webradio_progress_callback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, 
                                     curl_off_t ultotal, curl_off_t ulnow)
{
    // Check if stop was requested
    xSemaphoreTake(g_webradio_mutex, portMAX_DELAY);
    bool stop_requested = g_webradio_ctx.stop_requested;
    xSemaphoreGive(g_webradio_mutex);
    
    if (stop_requested) {
        ESP_LOGI(TAG, "Progress callback: stop requested");
        return 1; // Non-zero return will abort the transfer
    }

    return 0; // Continue transfer
}
