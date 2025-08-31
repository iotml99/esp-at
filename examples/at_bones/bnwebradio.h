#ifndef __BNWEBRADIO_H__
#define __BNWEBRADIO_H__

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

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
 * @brief Web Radio context structure
 */
typedef struct {
    char url[512];                    // Radio stream URL
    bool is_active;                   // Stream active flag
    webradio_state_t state;          // Current streaming state
    size_t bytes_streamed;           // Total bytes streamed
    uint32_t start_time;             // Stream start timestamp
    void *curl_handle;               // CURL handle for streaming
    bool stop_requested;             // Stop flag for graceful shutdown
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
 * @return true on success, false on failure
 */
bool bnwebradio_start(const char *url);

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

#ifdef __cplusplus
}
#endif

#endif /* __BNWEBRADIO_H__ */
