/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BNCURL_EXECUTOR_H
#define BNCURL_EXECUTOR_H

#include <stdbool.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include "bncurl.h"

// Executor task configuration
#define BNCURL_EXECUTOR_STACK_SIZE    (16 * 1024)
#define BNCURL_EXECUTOR_PRIORITY      (2)

// Executor status enumeration
typedef enum {
    BNCURL_EXECUTOR_IDLE,       // No requests pending or executing
    BNCURL_EXECUTOR_QUEUED,     // Request queued but not yet executing
    BNCURL_EXECUTOR_EXECUTING,  // Request currently executing
} bncurl_executor_status_t;

// Executor context structure
typedef struct {
    TaskHandle_t task_handle;           // FreeRTOS task handle
    SemaphoreHandle_t request_semaphore; // Semaphore for request signaling
    bool task_running;                  // Flag to control task lifecycle
    
    bncurl_context_t *pending_request;  // Request waiting to be executed
    bncurl_context_t *current_request;  // Currently executing request
    bool last_result;                   // Result of last executed request
} bncurl_executor_t;

/**
 * @brief Initialize the BNCURL executor
 * 
 * Creates the executor task and initializes all necessary resources.
 * This should be called once during system initialization.
 * 
 * @return true on success, false on failure
 */
bool bncurl_executor_init(void);

/**
 * @brief Deinitialize the BNCURL executor
 * 
 * Stops the executor task and cleans up all resources.
 * This should be called during system shutdown.
 */
void bncurl_executor_deinit(void);

/**
 * @brief Submit a request for asynchronous execution
 * 
 * Queues the provided BNCURL context for execution by the executor task.
 * Only one request can be queued or executing at a time.
 * 
 * @param ctx BNCURL context containing request parameters
 * @return true if request was queued successfully, false otherwise
 */
bool bncurl_executor_submit_request(bncurl_context_t *ctx);

/**
 * @brief Check if the executor is currently busy
 * 
 * @return true if a request is queued or executing, false if idle
 */
bool bncurl_executor_is_busy(void);

/**
 * @brief Stop the currently executing request
 * 
 * Signals the current request (if any) to stop and clears any pending requests.
 * 
 * @return true if a request was stopped, false if no request was running
 */
bool bncurl_executor_stop_current(void);

/**
 * @brief Get the current status of the executor
 * 
 * @return Current executor status
 */
bncurl_executor_status_t bncurl_executor_get_status(void);

#endif /* BNCURL_EXECUTOR_H */
