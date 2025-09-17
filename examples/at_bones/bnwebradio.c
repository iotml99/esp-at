#include "bnwebradio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "curl/curl.h"
#include "esp_at.h"
#include "bncert_manager.h"
#include "bnsd.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "BNWEBRADIO";

// Global webradio context
static bnwebradio_context_t g_webradio_ctx = {0};
static SemaphoreHandle_t g_webradio_mutex = NULL;
static TaskHandle_t g_webradio_task = NULL;

// Forward declarations
static void webradio_task(void *pvParameters);
static void webradio_stream_task(void *pvParameters);
static size_t webradio_write_callback(void *contents, size_t size, size_t nmemb, void *userp);
static int webradio_progress_callback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, 
                                     curl_off_t ultotal, curl_off_t ulnow);
static bool webradio_init_shared_buffers(webradio_shared_buffers_t *shared, webradio_buffer_t *buffers);
static void webradio_cleanup_shared_buffers(webradio_shared_buffers_t *shared);
static bool webradio_add_data_to_buffer(const uint8_t *data, size_t size, webradio_shared_buffers_t *shared);
static bool webradio_switch_buffers(webradio_shared_buffers_t *shared);

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
    g_webradio_ctx.save_to_file = false;
    g_webradio_ctx.file_handle = NULL;
    g_webradio_ctx.shared_buffers = NULL;

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

bool bnwebradio_start(const char *url, const char *save_file_path)
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
    g_webradio_ctx.file_handle = NULL;
    g_webradio_ctx.write_count = 0;
    g_webradio_ctx.shared_buffers = NULL;

    // Handle file saving configuration
    if (save_file_path && strlen(save_file_path) > 0) {
        // Check if SD card is mounted
        if (!bnsd_is_mounted()) {
            ESP_LOGE(TAG, "SD card not mounted, cannot save to file");
            xSemaphoreGive(g_webradio_mutex);
            return false;
        }
        
        strncpy(g_webradio_ctx.save_file_path, save_file_path, sizeof(g_webradio_ctx.save_file_path) - 1);
        g_webradio_ctx.save_file_path[sizeof(g_webradio_ctx.save_file_path) - 1] = '\0';
        g_webradio_ctx.save_to_file = true;
        ESP_LOGI(TAG, "Will save stream to file: %s", save_file_path);
    } else {
        g_webradio_ctx.save_to_file = false;
        ESP_LOGI(TAG, "Streaming only mode (no file saving)");
    }

    // Create streaming (output) task first
    BaseType_t ret = xTaskCreate(webradio_stream_task, "webradio_stream", 4096, NULL, 6, &g_webradio_ctx.stream_task);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create webradio stream task");
        g_webradio_ctx.is_active = false;
        g_webradio_ctx.state = WEBRADIO_STATE_ERROR;
        g_webradio_ctx.save_to_file = false;
        xSemaphoreGive(g_webradio_mutex);
        return false;
    }

    // Create fetch (input) task
    ret = xTaskCreate(webradio_task, "webradio_fetch", 8192, NULL, 5, &g_webradio_task);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create webradio fetch task");
        
        // Clean up stream task
        if (g_webradio_ctx.stream_task) {
            vTaskDelete(g_webradio_ctx.stream_task);
            g_webradio_ctx.stream_task = NULL;
        }
        
        g_webradio_ctx.is_active = false;
        g_webradio_ctx.state = WEBRADIO_STATE_ERROR;
        g_webradio_ctx.save_to_file = false;
        xSemaphoreGive(g_webradio_mutex);
        return false;
    }

    xSemaphoreGive(g_webradio_mutex);
    
    if (g_webradio_ctx.save_to_file) {
        ESP_LOGI(TAG, "Web radio streaming started for URL: %s, saving to: %s", url, save_file_path);
    } else {
        ESP_LOGI(TAG, "Web radio streaming started for URL: %s", url);
    }
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

    // Signal stream task to wake up and exit
    if (g_webradio_ctx.shared_buffers && g_webradio_ctx.shared_buffers->data_ready_sem) {
        xSemaphoreGive(g_webradio_ctx.shared_buffers->data_ready_sem);
    }

    // Wait for fetch task to finish
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
            ESP_LOGW(TAG, "Force terminating webradio fetch task");
            vTaskDelete(g_webradio_task);
            g_webradio_task = NULL;
        }
    }

    // Wait for stream task to finish
    if (g_webradio_ctx.stream_task != NULL) {
        // Wait up to 3 seconds for graceful shutdown
        for (int i = 0; i < 30; i++) {
            if (g_webradio_ctx.stream_task == NULL) {
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(100));
        }

        // Force delete if still running
        if (g_webradio_ctx.stream_task != NULL) {
            ESP_LOGW(TAG, "Force terminating webradio stream task");
            vTaskDelete(g_webradio_ctx.stream_task);
            g_webradio_ctx.stream_task = NULL;
        }
    }

    xSemaphoreTake(g_webradio_mutex, portMAX_DELAY);
    
    // Close file if it was open
    if (g_webradio_ctx.file_handle) {
        // Final flush to ensure all data is written
        fflush((FILE*)g_webradio_ctx.file_handle);
        fclose((FILE*)g_webradio_ctx.file_handle);
        g_webradio_ctx.file_handle = NULL;
        if (g_webradio_ctx.save_to_file) {
            ESP_LOGI(TAG, "Closed file: %s", g_webradio_ctx.save_file_path);
        }
    }
    
    g_webradio_ctx.is_active = false;
    g_webradio_ctx.state = WEBRADIO_STATE_IDLE;
    g_webradio_ctx.save_to_file = false;
    
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

bool bnwebradio_get_context_info(bool *save_to_file, char *save_file_path, size_t path_buffer_size)
{
    if (!save_to_file || !save_file_path || path_buffer_size == 0 || g_webradio_mutex == NULL) {
        return false;
    }

    xSemaphoreTake(g_webradio_mutex, portMAX_DELAY);
    
    if (!g_webradio_ctx.is_active) {
        xSemaphoreGive(g_webradio_mutex);
        return false;
    }

    *save_to_file = g_webradio_ctx.save_to_file;
    
    if (g_webradio_ctx.save_to_file) {
        strncpy(save_file_path, g_webradio_ctx.save_file_path, path_buffer_size - 1);
        save_file_path[path_buffer_size - 1] = '\0';
    } else {
        save_file_path[0] = '\0';
    }

    xSemaphoreGive(g_webradio_mutex);
    return true;
}

static void webradio_task(void *pvParameters)
{
    CURL *curl = NULL;
    CURLcode res;
    
    // Allocate buffers on stack - these will persist for the lifetime of this task
    webradio_buffer_t stack_buffers[2];
    webradio_shared_buffers_t shared_buffers;

    ESP_LOGI(TAG, "Web radio task started with stack-allocated buffers");

    // Initialize the shared buffer context
    if (!webradio_init_shared_buffers(&shared_buffers, stack_buffers)) {
        ESP_LOGE(TAG, "Failed to initialize shared buffers");
        goto task_cleanup;
    }

    // Set the shared buffer pointer in the global context
    xSemaphoreTake(g_webradio_mutex, portMAX_DELAY);
    g_webradio_ctx.shared_buffers = &shared_buffers;
    xSemaphoreGive(g_webradio_mutex);

    // Initialize CURL
    curl = curl_easy_init();
    if (!curl) {
        ESP_LOGE(TAG, "Failed to initialize CURL");
        goto task_cleanup;
    }

    // Store curl handle in context and open file if needed
    xSemaphoreTake(g_webradio_mutex, portMAX_DELAY);
    g_webradio_ctx.curl_handle = curl;
    
    // Open file for writing if file saving is enabled
    if (g_webradio_ctx.save_to_file) {
        g_webradio_ctx.file_handle = fopen(g_webradio_ctx.save_file_path, "wb");
        if (!g_webradio_ctx.file_handle) {
            ESP_LOGE(TAG, "Failed to open file for writing: %s", g_webradio_ctx.save_file_path);
            xSemaphoreGive(g_webradio_mutex);
            goto task_cleanup;
        }
        ESP_LOGI(TAG, "Opened file for writing: %s", g_webradio_ctx.save_file_path);
    }
    
    xSemaphoreGive(g_webradio_mutex);

    // Configure CURL for streaming
    curl_easy_setopt(curl, CURLOPT_URL, g_webradio_ctx.url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, webradio_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, NULL);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, webradio_progress_callback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, NULL);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    
    // Set user agent
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "ESP32-WebRadio/1.0");
    
    // Follow redirects
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    
    // Set timeouts
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0L); // No total timeout for streaming
    
    // Set server response timeout for web radio (if no data for 60 seconds, abort)
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 60L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1L); // 1 byte/sec minimum speed
    
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

    // Clean up shared buffers
    webradio_cleanup_shared_buffers(&shared_buffers);

    // Update context and close file
    xSemaphoreTake(g_webradio_mutex, portMAX_DELAY);
    g_webradio_ctx.curl_handle = NULL;
    g_webradio_ctx.shared_buffers = NULL;
    
    // Close file if it was opened
    if (g_webradio_ctx.file_handle) {
        // Final flush to ensure all data is written
        fflush((FILE*)g_webradio_ctx.file_handle);
        fclose((FILE*)g_webradio_ctx.file_handle);
        g_webradio_ctx.file_handle = NULL;
        ESP_LOGI(TAG, "Closed file: %s", g_webradio_ctx.save_file_path);
    }
    
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
    
    if (total_size == 0) {
        return 0;
    }
    
    // Check if stop was requested
    xSemaphoreTake(g_webradio_mutex, portMAX_DELAY);
    if (g_webradio_ctx.stop_requested) {
        ESP_LOGI(TAG, "Stop requested, terminating stream");
        xSemaphoreGive(g_webradio_mutex);
        return 0; // This will cause CURL to abort
    }
    
    // Update statistics
    g_webradio_ctx.bytes_streamed += total_size;
    xSemaphoreGive(g_webradio_mutex);
    
    // Add data to buffer system for smooth streaming
    xSemaphoreTake(g_webradio_mutex, portMAX_DELAY);
    webradio_shared_buffers_t *shared = g_webradio_ctx.shared_buffers;
    xSemaphoreGive(g_webradio_mutex);
    
    if (shared && !webradio_add_data_to_buffer((uint8_t *)contents, total_size, shared)) {
        ESP_LOGW(TAG, "Failed to add data to buffer, data loss possible");
        // Continue anyway to avoid breaking the stream
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

static bool webradio_init_shared_buffers(webradio_shared_buffers_t *shared, webradio_buffer_t *buffers)
{
    if (!shared || !buffers) {
        return false;
    }

    // Create buffer mutex
    shared->buffer_mutex = xSemaphoreCreateMutex();
    if (shared->buffer_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create buffer mutex");
        return false;
    }

    // Create data ready semaphore
    shared->data_ready_sem = xSemaphoreCreateBinary();
    if (shared->data_ready_sem == NULL) {
        ESP_LOGE(TAG, "Failed to create data ready semaphore");
        vSemaphoreDelete(shared->buffer_mutex);
        shared->buffer_mutex = NULL;
        return false;
    }

    // Set buffer pointer and initialize buffers
    shared->buffers = buffers;
    for (int i = 0; i < 2; i++) {
        shared->buffers[i].size = 0;
        shared->buffers[i].is_ready = false;
        shared->buffers[i].is_full = false;
    }

    shared->active_buffer = 0;
    shared->streaming_buffer = -1;

    ESP_LOGI(TAG, "Stack-based audio buffers initialized (2 x %d bytes)", WEBRADIO_BUFFER_SIZE);
    return true;
}

static void webradio_cleanup_shared_buffers(webradio_shared_buffers_t *shared)
{
    if (!shared) {
        return;
    }

    if (shared->buffer_mutex != NULL) {
        vSemaphoreDelete(shared->buffer_mutex);
        shared->buffer_mutex = NULL;
    }

    if (shared->data_ready_sem != NULL) {
        vSemaphoreDelete(shared->data_ready_sem);
        shared->data_ready_sem = NULL;
    }

    shared->buffers = NULL;
    ESP_LOGI(TAG, "Stack-based audio buffers cleaned up");
}

static bool webradio_add_data_to_buffer(const uint8_t *data, size_t size, webradio_shared_buffers_t *shared)
{
    if (!data || size == 0 || !shared || !shared->buffers) {
        return false;
    }

    xSemaphoreTake(shared->buffer_mutex, portMAX_DELAY);

    size_t remaining = size;
    const uint8_t *src = data;

    while (remaining > 0) {
        // Check if stop was requested
        xSemaphoreTake(g_webradio_mutex, portMAX_DELAY);
        bool stop_requested = g_webradio_ctx.stop_requested;
        xSemaphoreGive(g_webradio_mutex);
        
        if (stop_requested) {
            xSemaphoreGive(shared->buffer_mutex);
            return false;
        }

        webradio_buffer_t *buffer = &shared->buffers[shared->active_buffer];
        
        // Check if current buffer is full
        if (buffer->is_full) {
            // Try to switch buffers
            if (!webradio_switch_buffers(shared)) {
                // Couldn't switch, buffer overflow - drop oldest data
                ESP_LOGW(TAG, "Buffer overflow, dropping %zu bytes", remaining);
                xSemaphoreGive(shared->buffer_mutex);
                return false;
            }
            buffer = &shared->buffers[shared->active_buffer];
        }

        // Calculate how much we can write to current buffer
        size_t available = WEBRADIO_BUFFER_SIZE - buffer->size;
        size_t to_copy = (remaining < available) ? remaining : available;

        // Copy data to buffer
        memcpy(buffer->data + buffer->size, src, to_copy);
        buffer->size += to_copy;
        src += to_copy;
        remaining -= to_copy;

        // Check if buffer is now full
        if (buffer->size >= WEBRADIO_BUFFER_SIZE) {
            buffer->is_full = true;
            buffer->is_ready = true;
            
            // Signal streaming task that data is ready
            xSemaphoreGive(shared->data_ready_sem);
        }
    }

    xSemaphoreGive(shared->buffer_mutex);
    return true;
}

static bool webradio_switch_buffers(webradio_shared_buffers_t *shared)
{
    if (!shared || !shared->buffers) {
        return false;
    }

    // Find the other buffer
    int other_buffer = (shared->active_buffer == 0) ? 1 : 0;
    
    // Check if the other buffer is available
    if (shared->buffers[other_buffer].is_ready) {
        // Other buffer is still being streamed, can't switch
        return false;
    }

    // Switch to the other buffer
    shared->active_buffer = other_buffer;
    shared->buffers[other_buffer].size = 0;
    shared->buffers[other_buffer].is_full = false;
    shared->buffers[other_buffer].is_ready = false;

    return true;
}

static void webradio_stream_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Web radio stream task started");

    while (true) {
        // Get shared buffer context
        xSemaphoreTake(g_webradio_mutex, portMAX_DELAY);
        webradio_shared_buffers_t *shared = g_webradio_ctx.shared_buffers;
        bool stop_requested = g_webradio_ctx.stop_requested;
        xSemaphoreGive(g_webradio_mutex);

        // Exit if no shared buffer context or stop requested
        if (!shared || stop_requested) {
            break;
        }

        // Wait for data to be ready or stop signal
        if (xSemaphoreTake(shared->data_ready_sem, pdMS_TO_TICKS(1000)) == pdTRUE) {
            
            // Check if we should stop
            xSemaphoreTake(shared->buffer_mutex, portMAX_DELAY);
            
            // Re-check stop status and shared context
            xSemaphoreTake(g_webradio_mutex, portMAX_DELAY);
            stop_requested = g_webradio_ctx.stop_requested;
            shared = g_webradio_ctx.shared_buffers;
            xSemaphoreGive(g_webradio_mutex);
            
            if (stop_requested || !shared || !shared->buffers) {
                xSemaphoreGive(shared->buffer_mutex);
                break;
            }

            // Find a ready buffer to stream
            int buffer_to_stream = -1;
            for (int i = 0; i < 2; i++) {
                if (shared->buffers[i].is_ready && 
                    shared->streaming_buffer != i) {
                    buffer_to_stream = i;
                    break;
                }
            }

            if (buffer_to_stream >= 0) {
                shared->streaming_buffer = buffer_to_stream;
                webradio_buffer_t *buffer = &shared->buffers[buffer_to_stream];
                
                // Copy buffer data for processing outside mutex
                uint8_t temp_buffer[WEBRADIO_BUFFER_SIZE];
                size_t buffer_size = buffer->size;
                memcpy(temp_buffer, buffer->data, buffer_size);
                
                xSemaphoreGive(shared->buffer_mutex);

                // Stream to UART outside of mutex to prevent blocking
                esp_at_port_write_data(temp_buffer, buffer_size);

                // Handle file saving if enabled
                xSemaphoreTake(g_webradio_mutex, portMAX_DELAY);
                if (g_webradio_ctx.save_to_file && g_webradio_ctx.file_handle) {
                    size_t written = fwrite(temp_buffer, 1, buffer_size, (FILE*)g_webradio_ctx.file_handle);
                    if (written != buffer_size) {
                        ESP_LOGE(TAG, "File write error: expected %zu bytes, wrote %zu bytes", buffer_size, written);
                    } else {
                        // Increment write counter and flush periodically
                        g_webradio_ctx.write_count++;
                        if (g_webradio_ctx.write_count >= 50) {  // Flush less frequently with larger buffers
                            fflush((FILE*)g_webradio_ctx.file_handle);
                            g_webradio_ctx.write_count = 0;
                        }
                    }
                }
                xSemaphoreGive(g_webradio_mutex);

                // Mark buffer as consumed
                xSemaphoreTake(shared->buffer_mutex, portMAX_DELAY);
                buffer->size = 0;
                buffer->is_ready = false;
                buffer->is_full = false;
                shared->streaming_buffer = -1;
                xSemaphoreGive(shared->buffer_mutex);
                
            } else {
                xSemaphoreGive(shared->buffer_mutex);
            }
        } else {
            // Timeout - continue to check for stop condition in main loop
        }
    }

    // Clear task handle
    g_webradio_ctx.stream_task = NULL;
    ESP_LOGI(TAG, "Web radio stream task ended");
    vTaskDelete(NULL);
}
