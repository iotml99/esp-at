/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bncurl_executor.h"
#include "bncurl.h"
#include "bncurl_config.h"
#include "bncurl_common.h"
#include "bncurl_get.h"
#include "bncurl_post.h"
#include "bncurl_head.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <curl/curl.h>
#include <stdlib.h>
#include <stdio.h>
#include "esp_at.h"
#include "esp_log.h"
#include "bnkill.h"

static const char *TAG = "BNCURL_EXECUTOR";

// Global executor context
static bncurl_executor_t *g_executor = NULL;
static SemaphoreHandle_t executor_mutex = NULL;

// Task function for executing HTTP requests
static void bncurl_executor_task(void *pvParameters)
{
    bncurl_executor_t *executor = (bncurl_executor_t *)pvParameters;
    
    while (executor->task_running) {
        // Wait for a request to be queued
        if (xSemaphoreTake(executor->request_semaphore, portMAX_DELAY) == pdTRUE) {
            if (!executor->task_running) {
                break; // Exit if shutdown requested
            }
            
            // Take mutex to access shared data
            if (xSemaphoreTake(executor_mutex, portMAX_DELAY) == pdTRUE) {
                if (executor->pending_request) {
                    bncurl_context_t *ctx = executor->pending_request;
                    executor->pending_request = NULL;
                    executor->current_request = ctx;
                    
                    xSemaphoreGive(executor_mutex);
                    
                    // Execute the request based on method
                    ESP_LOGI(TAG, "Executing %s request for URL: %s", ctx->params.method, ctx->params.url);
                    
                    bool success = false;
                    if (strcmp(ctx->params.method, "GET") == 0) {
                        success = bncurl_execute_get_request(ctx);
                    } else if (strcmp(ctx->params.method, "POST") == 0) {
                        success = bncurl_execute_post_request(ctx);
                    } else if (strcmp(ctx->params.method, "HEAD") == 0) {
                        success = bncurl_execute_head_request(ctx);
                    }
                    
                    // Take mutex again to update status
                    if (xSemaphoreTake(executor_mutex, portMAX_DELAY) == pdTRUE) {
                        executor->current_request = NULL;
                        executor->last_result = success;
                        
                        // Completion status (SEND OK/SEND ERROR) is handled by bncurl_stream_finalize()
                        // in the respective GET/POST/HEAD implementations, so we don't send it here
                        // to avoid duplicate messages
                        
                        xSemaphoreGive(executor_mutex);
                    }
                } else {
                    xSemaphoreGive(executor_mutex);
                }
            }
        }
    }
    
    ESP_LOGI(TAG, "BNCURL executor task exiting");
    vTaskDelete(NULL);
}

bool bncurl_executor_init(void)
{
    if (g_executor) {
        return true; // Already initialized
    }
    
    // Create mutex
    executor_mutex = xSemaphoreCreateMutex();
    if (!executor_mutex) {
        ESP_LOGE(TAG, "Failed to create executor mutex");
        return false;
    }
    
    // Allocate executor context
    g_executor = calloc(1, sizeof(bncurl_executor_t));
    if (!g_executor) {
        ESP_LOGE(TAG, "Failed to allocate executor context");
        vSemaphoreDelete(executor_mutex);
        executor_mutex = NULL;
        return false;
    }
    
    // Create semaphore for request signaling
    g_executor->request_semaphore = xSemaphoreCreateBinary();
    if (!g_executor->request_semaphore) {
        ESP_LOGE(TAG, "Failed to create request semaphore");
        free(g_executor);
        g_executor = NULL;
        vSemaphoreDelete(executor_mutex);
        executor_mutex = NULL;
        return false;
    }
    
    // Initialize executor state
    g_executor->task_running = true;
    g_executor->pending_request = NULL;
    g_executor->current_request = NULL;
    g_executor->last_result = false;
    
    // Create executor task
    BaseType_t task_result = xTaskCreate(
        bncurl_executor_task,
        "bncurl_executor",
        BNCURL_EXECUTOR_STACK_SIZE,
        g_executor,
        BNCURL_EXECUTOR_PRIORITY,
        &g_executor->task_handle
    );
    
    if (task_result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create executor task");
        vSemaphoreDelete(g_executor->request_semaphore);
        free(g_executor);
        g_executor = NULL;
        vSemaphoreDelete(executor_mutex);
        executor_mutex = NULL;
        return false;
    }
    
    // Initialize curl globally
    curl_global_init(CURL_GLOBAL_ALL);
    
    // Initialize kill switch system
    bnkill_init();
    
    ESP_LOGI(TAG, "BNCURL executor initialized successfully");
    return true;
}

void bncurl_executor_deinit(void)
{
    if (!g_executor) {
        return;
    }
    
    ESP_LOGI(TAG, "Shutting down BNCURL executor");
    
    // Signal task to stop
    if (xSemaphoreTake(executor_mutex, portMAX_DELAY) == pdTRUE) {
        g_executor->task_running = false;
        xSemaphoreGive(executor_mutex);
    }
    
    // Signal task to wake up and exit
    xSemaphoreGive(g_executor->request_semaphore);
    
    // Wait for task to complete (with timeout)
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(5000));
    
    // Cleanup resources
    if (g_executor->request_semaphore) {
        vSemaphoreDelete(g_executor->request_semaphore);
    }
    
    free(g_executor);
    g_executor = NULL;
    
    if (executor_mutex) {
        vSemaphoreDelete(executor_mutex);
        executor_mutex = NULL;
    }
    
    // Cleanup curl
    curl_global_cleanup();
    
    ESP_LOGI(TAG, "BNCURL executor shutdown complete");
}

bool bncurl_executor_submit_request(bncurl_context_t *ctx)
{
    if (!g_executor || !ctx) {
        ESP_LOGE(TAG, "Executor not initialized or invalid context");
        return false;
    }
    
    // Perform kill switch check before executing any BNCURL command
    // Pass NULL for http_date_header since we don't have it yet
    if (!bnkill_check_expiry(NULL)) {
        ESP_LOGE(TAG, "FIRMWARE EXPIRED");
        return false;
    }
    
    // Support GET, POST, HEAD methods
    if (strcmp(ctx->params.method, "GET") != 0 && 
        strcmp(ctx->params.method, "POST") != 0 && 
        strcmp(ctx->params.method, "HEAD") != 0) {
        ESP_LOGW(TAG, "Method %s not supported", ctx->params.method);
        return false;
    }
    
    // Check if executor is busy (either executing or has a pending request)
    if (xSemaphoreTake(executor_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (g_executor->pending_request || g_executor->current_request) {
            xSemaphoreGive(executor_mutex);
            ESP_LOGW(TAG, "Executor is busy - rejecting new request");
            return false; // Return false to indicate ERROR
        }
        
        // Queue the request
        g_executor->pending_request = ctx;
        xSemaphoreGive(executor_mutex);
        
        // Signal the executor task
        xSemaphoreGive(g_executor->request_semaphore);
        
        ESP_LOGI(TAG, "Request queued for execution: %s %s", ctx->params.method, ctx->params.url);
        return true; // Return true to indicate OK
    }
    
    ESP_LOGE(TAG, "Failed to acquire executor mutex");
    return false;
}

bool bncurl_executor_is_busy(void)
{
    if (!g_executor) {
        return false;
    }
    
    bool busy = false;
    
    if (xSemaphoreTake(executor_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        busy = (g_executor->pending_request != NULL || g_executor->current_request != NULL);
        xSemaphoreGive(executor_mutex);
    }
    
    return busy;
}

bool bncurl_executor_stop_current(void)
{
    if (!g_executor) {
        return false;
    }
    
    bool stopped = false;
    
    if (xSemaphoreTake(executor_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (g_executor->current_request) {
            // Signal the current request to stop
            bncurl_stop(g_executor->current_request);
            stopped = true;
        }
        
        // Clear any pending request
        g_executor->pending_request = NULL;
        
        xSemaphoreGive(executor_mutex);
    }
    
    if (stopped) {
        ESP_LOGI(TAG, "Current request stop requested");
    }
    
    return stopped;
}

bncurl_executor_status_t bncurl_executor_get_status(void)
{
    if (!g_executor) {
        return BNCURL_EXECUTOR_IDLE;
    }
    
    bncurl_executor_status_t status = BNCURL_EXECUTOR_IDLE;
    
    if (xSemaphoreTake(executor_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        if (g_executor->current_request) {
            status = BNCURL_EXECUTOR_EXECUTING;
        } else if (g_executor->pending_request) {
            status = BNCURL_EXECUTOR_QUEUED;
        }
        
        xSemaphoreGive(executor_mutex);
    }
    
    return status;
}
