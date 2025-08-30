/*
 * BNCURL Parameter Parsing and Validation
 * 
 * Handles parsing and validation of BNCURL command parameters including
 * method validation, option parsing (-dd, -du, -H, -v), and parameter validation.
 */

#ifndef BNCURL_PARAMS_H
#define BNCURL_PARAMS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "esp_at.h"
#include "atbn_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* HTTP method enumeration */
typedef enum {
    BNCURL_GET = 0,
    BNCURL_POST,
    BNCURL_HEAD,
    BNCURL_METHOD_MAX
} bncurl_method_t;

/* Parsed parameter structure */
typedef struct {
    /* Basic parameters */
    bncurl_method_t method;
    char url[BNCURL_URL_MAX_LEN];
    
    /* File save options (-dd) */
    bool save_to_file;
    char save_path[BNCURL_FILEPATH_BUFFER_SIZE];
    
    /* Upload options (-du) */
    bool has_upload;
    bool upload_from_file;
    char upload_path[BNCURL_UPLOAD_PARAM_BUFFER_SIZE];
    size_t upload_size;
    
    /* Custom headers (-H) */
    char headers_list[BNCURL_MAX_HEADERS][BNCURL_HEADER_BUFFER_SIZE];
    int header_count;

    /* Cookies */
    bool save_cookies;
    bool load_cookies;

    char cookie_save_paths[BNCURL_MAX_COOKIES][BNCURL_COOKIE_BUFFER_SIZE];
    int cookie_save_count;

    char cookie_load_paths[BNCURL_MAX_COOKIES][BNCURL_COOKIE_BUFFER_SIZE];
    int cookie_load_count;

    /* range */
    char range[BNCURL_RANGE_BUFFER_SIZE];
    bool has_range;
    uint64_t range_start;
    uint64_t range_end;

    /* Verbose mode (-v) */
    bool verbose;
    
} bncurl_params_t;

/**
 * @brief Parse and validate BNCURL command parameters
 * 
 * Parses AT command parameters and validates them according to BNCURL specification.
 * Performs two-pass parsing: first for validation, second for actual processing.
 * 
 * @param para_num Number of parameters passed to AT command
 * @param params Output structure to store parsed parameters
 * @return ESP_AT_RESULT_CODE_OK on success, ESP_AT_RESULT_CODE_ERROR on failure
 */
uint8_t bncurl_params_parse(uint8_t para_num, bncurl_params_t *params);

/**
 * @brief Validate HTTP method string
 * 
 * Validates that the method string is one of the supported HTTP methods.
 * 
 * @param method_str Method string to validate
 * @param method Output method enumeration value
 * @return true if valid method, false otherwise
 */
bool bncurl_params_validate_method(const char *method_str, bncurl_method_t *method);

/**
 * @brief Process file path parameter with @ expansion
 * 
 * Handles @ prefix expansion for SD card mount point and validates path length.
 * 
 * @param input_path Input path string (may contain @ prefix)
 * @param output_path Output buffer for processed path
 * @param output_size Size of output buffer
 * @return true if successful, false if path is invalid or too long
 */
bool bncurl_params_process_filepath(const char *input_path, char *output_path, size_t output_size);

/**
 * @brief Validate upload parameter (size or file path)
 * 
 * Determines if upload parameter is a numeric size or file path and validates accordingly.
 * 
 * @param param Upload parameter string
 * @param is_file_upload Output flag indicating if this is file upload
 * @param upload_size Output size for numeric parameters
 * @return true if valid parameter, false otherwise
 */
bool bncurl_params_validate_upload(const char *param, bool *is_file_upload, size_t *upload_size);

/**
 * @brief Validate HTTP header format
 * 
 * Validates that header contains ':' and is within length limits.
 * 
 * @param header Header string to validate
 * @return true if valid header format, false otherwise
 */
bool bncurl_params_validate_header(const char *header);

/**
 * @brief Validate range parameter format
 * 
 * Validates range format "start-end" and extracts start/end values.
 * 
 * @param range_str Range string to validate
 * @param start Output start value
 * @param end Output end value
 * @return true if valid range format, false otherwise
 */
bool bncurl_params_validate_range(const char *range_str, uint64_t *start, uint64_t *end);

#ifdef __cplusplus
}
#endif

#endif /* BNCURL_PARAMS_H */
