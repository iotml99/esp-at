#ifndef __BNWEBRADIO_H__
#define __BNWEBRADIO_H__

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WEBRADIO_BUFFER_SIZE 2048  // 2KB per buffer

/**
 * @brief Web Radio streaming states
 */
typedef enum {
    WEBRADIO_STATE_IDLE = 0,
    WEBRADIO_STATE_CONNECTING,
    WEBRADIO_STATE_STREAMING,
    WEBRADIO_STATE_STOPPING,
    WEBRADIO_STATE_ERROR
} webradio_state_t;

/**
 * @brief Audio buffer structure for double buffering
 */
typedef struct {
    uint8_t data[WEBRADIO_BUFFER_SIZE];  // Buffer data
    size_t size;                         // Current data size in buffer
    bool is_ready;                       // Buffer ready for consumption
    bool is_full;                        // Buffer is full and ready to switch
} webradio_buffer_t;

/**
 * @brief Shared buffer context structure for task communication
 */
typedef struct {
    webradio_buffer_t *buffers;      // Pointer to buffer array (allocated on task stack)
    int active_buffer;               // Index of buffer currently being filled (0 or 1)
    int streaming_buffer;            // Index of buffer currently being streamed (-1 if none)
    SemaphoreHandle_t buffer_mutex;  // Mutex for buffer operations
    SemaphoreHandle_t data_ready_sem; // Semaphore to signal data ready for streaming
} webradio_shared_buffers_t;

/**
 * @brief Web Radio context structure
 */
typedef struct {
    char url[256];                    // Radio stream URL
    bool is_active;                   // Stream active flag
    webradio_state_t state;          // Current streaming state
    size_t bytes_streamed;           // Total bytes streamed
    uint32_t start_time;             // Stream start timestamp
    void *curl_handle;               // CURL handle for streaming
    bool stop_requested;             // Stop flag for graceful shutdown
    char save_file_path[256];        // SD card file path to save stream
    bool save_to_file;               // Flag to enable file saving
    void *file_handle;               // FILE* handle for saving
    uint32_t write_count;            // Counter for periodic file flushing
    
    // Double buffering - now managed on task stack
    webradio_shared_buffers_t *shared_buffers; // Pointer to shared buffer context
    TaskHandle_t stream_task;        // Handle for streaming task
} bnwebradio_context_t;

/**
 * @brief Initialize web radio module
 * @return true on success, false on failure
 */
bool bnwebradio_init(void);

/**
 * @brief Deinitialize web radio module
 */
void bnwebradio_deinit(void);

/**
 * @brief Start web radio streaming
 * @param url Radio stream URL
 * @param save_file_path Optional SD card file path to save stream (NULL for streaming only)
 * @return true on success, false on failure
 */
bool bnwebradio_start(const char *url, const char *save_file_path);

/**
 * @brief Stop web radio streaming
 * @return true on success, false on failure
 */
bool bnwebradio_stop(void);

/**
 * @brief Get current streaming state
 * @return Current webradio_state_t
 */
webradio_state_t bnwebradio_get_state(void);

/**
 * @brief Get streaming statistics
 * @param bytes_streamed Output: total bytes streamed
 * @param duration_ms Output: streaming duration in milliseconds
 * @return true if streaming is active, false otherwise
 */
bool bnwebradio_get_stats(size_t *bytes_streamed, uint32_t *duration_ms);

/**
 * @brief Check if web radio is currently active
 * @return true if streaming, false otherwise
 */
bool bnwebradio_is_active(void);

/**
 * @brief Get webradio context information (for AT command queries)
 * @param save_to_file Output: whether saving to file is enabled
 * @param save_file_path Output: file path being saved to (if save_to_file is true)
 * @param path_buffer_size Size of the save_file_path buffer
 * @return true if context retrieved successfully, false otherwise
 */
bool bnwebradio_get_context_info(bool *save_to_file, char *save_file_path, size_t path_buffer_size);

#ifdef __cplusplus
}
#endif

#endif /* __BNWEBRADIO_H__ */
