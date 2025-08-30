/*
 * UART Utilities for AT Custom Commands
 * 
 * Provides thread-safe UART communication utilities for ESP32 AT commands
 */

#include "uart_utils.h"
#include "esp_at.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

/* Static variables for UART utilities */
static SemaphoreHandle_t at_uart_lock = NULL;
static SemaphoreHandle_t data_input_sema = NULL;

bool uart_utils_init(void)
{
    /* Create UART lock mutex if not already created */
    if (!at_uart_lock) {
        at_uart_lock = xSemaphoreCreateMutex();
        if (!at_uart_lock) {
            return false;
        }
    }
    
    /* Create data input semaphore if not already created */
    if (!data_input_sema) {
        data_input_sema = xSemaphoreCreateBinary();
        if (!data_input_sema) {
            return false;
        }
    }
    
    return true;
}

void at_uart_write_locked(const uint8_t *data, size_t len)
{
    if (at_uart_lock) {
        xSemaphoreTake(at_uart_lock, portMAX_DELAY);
    }
    esp_at_port_write_data((uint8_t*)data, len);
    if (at_uart_lock) {
        xSemaphoreGive(at_uart_lock);
    }
}

SemaphoreHandle_t uart_utils_get_lock(void)
{
    return at_uart_lock;
}

SemaphoreHandle_t uart_utils_get_data_sema(void)
{
    return data_input_sema;
}
