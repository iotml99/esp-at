/*
 * BNCURL Parameter Parsing and Validation
 * 
 * Handles parsing and validation of BNCURL command parameters including
 * method validation, option parsing (-dd, -du, -H, -v, -c, -b, -r), and parameter validation.
 */

#include "bncurl_params.h"
#include "uart_utils.h"
#include <string.h>
#include <stdlib.h>

/* HTTP method strings */
static const char *bncurl_method_str[BNCURL_METHOD_MAX] = {
    "GET", "POST", "HEAD"
};

bool bncurl_params_validate_method(const char *method_str, bncurl_method_t *method)
{
    if (!method_str || !method) {
        return false;
    }
    
    for (int i = 0; i < BNCURL_METHOD_MAX; i++) {
        if (strcasecmp(method_str, bncurl_method_str[i]) == 0) {
            *method = (bncurl_method_t)i;
            return true;
        }
    }
    
    return false;
}

bool bncurl_params_process_filepath(const char *input_path, char *output_path, size_t output_size)
{
    if (!input_path || !output_path || output_size == 0) {
        return false;
    }
    
    /* Validate input path length before processing */
    if (strlen(input_path) > BNCURL_FILEPATH_MAX_LEN) {
        return false;
    }
    
    /* Handle @ prefix as SD card mount point shorthand */
    if (input_path[0] == '@') {
        if (input_path[1] == '/' || input_path[1] == '\0') {
            /* @/ or @ represents the mount point */
            if (input_path[1] == '/') {
                snprintf(output_path, output_size, "%s%s", BNCURL_SDCARD_MOUNT_POINT, input_path + 1);
            } else {
                snprintf(output_path, output_size, "%s", BNCURL_SDCARD_MOUNT_POINT);
            }
        } else {
            /* @filename -> /sdcard/filename */
            snprintf(output_path, output_size, "%s/%s", BNCURL_SDCARD_MOUNT_POINT, input_path + 1);
        }
    } else {
        /* No @ prefix, use path as-is */
        strncpy(output_path, input_path, output_size - 1);
        output_path[output_size - 1] = '\0';
    }
    
    return true;
}

bool bncurl_params_validate_upload(const char *param, bool *is_file_upload, size_t *upload_size)
{
    if (!param || !is_file_upload || !upload_size) {
        return false;
    }
    
    /* Validate parameter length */
    if (strlen(param) > BNCURL_FILEPATH_MAX_LEN) {
        return false;
    }
    
    /* Check if it's a file path (starts with @ or contains /) */
    if (param[0] == '@' || param[0] == '/' || strchr(param, '/') != NULL) {
        *is_file_upload = true;
        *upload_size = 0;
        return true;
    }
    
    /* UART size parameter - validate it's a valid number */
    *is_file_upload = false;
    
    const char *p = param;
    bool is_valid_number = true;
    if (*p == '\0') {
        is_valid_number = false; /* Empty string */
    }
    
    while (*p && is_valid_number) {
        if (*p < '0' || *p > '9') {
            is_valid_number = false;
        }
        p++;
    }
    
    if (!is_valid_number) {
        return false;
    }
    
    *upload_size = (size_t)atoi(param);
    
    /* Validate reasonable size limits (max 1MB for UART input) */
    if (*upload_size > BNCURL_UART_UPLOAD_MAX_SIZE) {
        return false;
    }
    
    return true;
}

bool bncurl_params_validate_range(const char *range_str, uint64_t *start, uint64_t *end)
{
    if (!range_str || !start || !end) {
        return false;
    }
    
    /* Validate range length */
    if (strlen(range_str) >= BNCURL_RANGE_BUFFER_SIZE) {
        return false;
    }
    
    /* Parse range format: "start-end" */
    char *dash = strchr(range_str, '-');
    if (!dash) {
        return false;
    }
    
    /* Extract start */
    char start_str[16];
    size_t start_len = dash - range_str;
    if (start_len >= sizeof(start_str)) {
        return false;
    }
    strncpy(start_str, range_str, start_len);
    start_str[start_len] = '\0';
    
    /* Extract end */
    const char *end_str = dash + 1;
    
    /* Validate numeric values */
    char *endptr;
    *start = strtoull(start_str, &endptr, 10);
    if (*endptr != '\0') {
        return false;
    }
    
    *end = strtoull(end_str, &endptr, 10);
    if (*endptr != '\0') {
        return false;
    }
    
    /* Validate range logic */
    if (*start > *end) {
        return false;
    }
    
    return true;
}

bool bncurl_params_validate_header(const char *header)
{
    if (!header) {
        return false;
    }
    
    /* Validate header length */
    if (strlen(header) > BNCURL_HEADER_MAX_LEN) {
        return false;
    }
    
    /* Basic header format validation (should contain :) */
    if (!strchr(header, ':')) {
        return false;
    }
    
    return true;
}

uint8_t bncurl_params_parse(uint8_t para_num, bncurl_params_t *params)
{
    if (!params || para_num < 2) {
        return ESP_AT_RESULT_CODE_ERROR;
    }
    
    /* Initialize parameters structure */
    memset(params, 0, sizeof(bncurl_params_t));
    params->method = BNCURL_GET;  /* Default method */
    
    /* Parse method and URL (required parameters) */
    uint8_t *method_str = NULL;
    uint8_t *url = NULL;
    
    if (esp_at_get_para_as_str(0, &method_str) != ESP_AT_PARA_PARSE_RESULT_OK) {
        return ESP_AT_RESULT_CODE_ERROR;
    }
    if (esp_at_get_para_as_str(1, &url) != ESP_AT_PARA_PARSE_RESULT_OK) {
        return ESP_AT_RESULT_CODE_ERROR;
    }
    
    /* Validate URL length */
    if (strlen((const char*)url) >= sizeof(params->url)) {
        const char *e = "+BNCURL: ERROR URL too long (max 255 characters)\r\n";
        at_uart_write_locked((const uint8_t*)e, strlen(e));
        return ESP_AT_RESULT_CODE_ERROR;
    }
    
    /* Validate and parse method */
    if (!bncurl_params_validate_method((const char*)method_str, &params->method)) {
        const char *e = "+BNCURL: ERROR unsupported method (GET, HEAD, and POST supported)\r\n";
        at_uart_write_locked((const uint8_t*)e, strlen(e));
        return ESP_AT_RESULT_CODE_ERROR;
    }
    
    /* Store URL */
    strncpy(params->url, (const char*)url, sizeof(params->url) - 1);
    params->url[sizeof(params->url) - 1] = '\0';
    
    /* Parameter validation flags to detect duplicates */
    bool dd_seen = false;
    bool du_seen = false;
    bool v_seen = false;
    bool c_seen = false;
    bool b_seen = false;
    bool r_seen = false;
    
    /* First pass: validate all parameters */
    for (int i = 2; i < para_num; i++) {
        uint8_t *opt = NULL;
        esp_at_para_parse_result_type result = esp_at_get_para_as_str(i, &opt);
        
        if (result != ESP_AT_PARA_PARSE_RESULT_OK || !opt) {
            const char *e = "+BNCURL: ERROR invalid parameter format\r\n";
            at_uart_write_locked((const uint8_t*)e, strlen(e));
            return ESP_AT_RESULT_CODE_ERROR;
        }
        
        /* Check for known parameters */
        if (strcasecmp((const char*)opt, "-dd") == 0) {
            if (dd_seen) {
                const char *e = "+BNCURL: ERROR duplicate -dd parameter\r\n";
                at_uart_write_locked((const uint8_t*)e, strlen(e));
                return ESP_AT_RESULT_CODE_ERROR;
            }
            dd_seen = true;
            i++; /* Skip next parameter (file path) */
            if (i >= para_num) {
                const char *e = "+BNCURL: ERROR -dd requires file path parameter\r\n";
                at_uart_write_locked((const uint8_t*)e, strlen(e));
                return ESP_AT_RESULT_CODE_ERROR;
            }
        } else if (strcasecmp((const char*)opt, "-du") == 0) {
            if (du_seen) {
                const char *e = "+BNCURL: ERROR duplicate -du parameter\r\n";
                at_uart_write_locked((const uint8_t*)e, strlen(e));
                return ESP_AT_RESULT_CODE_ERROR;
            }
            /* Validate that -du is only used with POST method */
            if (params->method != BNCURL_POST) {
                const char *e = "+BNCURL: ERROR -du parameter only valid with POST method\r\n";
                at_uart_write_locked((const uint8_t*)e, strlen(e));
                return ESP_AT_RESULT_CODE_ERROR;
            }
            du_seen = true;
            i++; /* Skip next parameter (upload param) */
            if (i >= para_num) {
                const char *e = "+BNCURL: ERROR -du requires parameter (size or file path)\r\n";
                at_uart_write_locked((const uint8_t*)e, strlen(e));
                return ESP_AT_RESULT_CODE_ERROR;
            }
        } else if (strcasecmp((const char*)opt, "-H") == 0) {
            i++; /* Skip next parameter (header) */
            if (i >= para_num) {
                const char *e = "+BNCURL: ERROR -H requires header parameter\r\n";
                at_uart_write_locked((const uint8_t*)e, strlen(e));
                return ESP_AT_RESULT_CODE_ERROR;
            }
        } else if (strcasecmp((const char*)opt, "-c") == 0) {
            if (c_seen) {
                const char *e = "+BNCURL: ERROR duplicate -c parameter\r\n";
                at_uart_write_locked((const uint8_t*)e, strlen(e));
                return ESP_AT_RESULT_CODE_ERROR;
            }
            c_seen = true;
            i++; /* Skip next parameter (cookie save file) */
            if (i >= para_num) {
                const char *e = "+BNCURL: ERROR -c requires cookie file path parameter\r\n";
                at_uart_write_locked((const uint8_t*)e, strlen(e));
                return ESP_AT_RESULT_CODE_ERROR;
            }
        } else if (strcasecmp((const char*)opt, "-b") == 0) {
            if (b_seen) {
                const char *e = "+BNCURL: ERROR duplicate -b parameter\r\n";
                at_uart_write_locked((const uint8_t*)e, strlen(e));
                return ESP_AT_RESULT_CODE_ERROR;
            }
            b_seen = true;
            i++; /* Skip next parameter (cookie load file) */
            if (i >= para_num) {
                const char *e = "+BNCURL: ERROR -b requires cookie file path parameter\r\n";
                at_uart_write_locked((const uint8_t*)e, strlen(e));
                return ESP_AT_RESULT_CODE_ERROR;
            }
        } else if (strcasecmp((const char*)opt, "-r") == 0) {
            if (r_seen) {
                const char *e = "+BNCURL: ERROR duplicate -r parameter\r\n";
                at_uart_write_locked((const uint8_t*)e, strlen(e));
                return ESP_AT_RESULT_CODE_ERROR;
            }
            /* Validate that -r is only used with GET method */
            if (params->method != BNCURL_GET) {
                const char *e = "+BNCURL: ERROR -r parameter only valid with GET method\r\n";
                at_uart_write_locked((const uint8_t*)e, strlen(e));
                return ESP_AT_RESULT_CODE_ERROR;
            }
            r_seen = true;
            i++; /* Skip next parameter (range) */
            if (i >= para_num) {
                const char *e = "+BNCURL: ERROR -r requires range parameter\r\n";
                at_uart_write_locked((const uint8_t*)e, strlen(e));
                return ESP_AT_RESULT_CODE_ERROR;
            }
        } else if (strcasecmp((const char*)opt, "-v") == 0) {
            if (v_seen) {
                const char *e = "+BNCURL: ERROR duplicate -v parameter\r\n";
                at_uart_write_locked((const uint8_t*)e, strlen(e));
                return ESP_AT_RESULT_CODE_ERROR;
            }
            v_seen = true;
        } else {
            /* Unknown parameter */
            char err_msg[128];
            int n = snprintf(err_msg, sizeof(err_msg), "+BNCURL: ERROR unknown parameter: %s\r\n", (const char*)opt);
            at_uart_write_locked((const uint8_t*)err_msg, n);
            return ESP_AT_RESULT_CODE_ERROR;
        }
    }
    
    
    /* Second pass: parse all parameters for actual processing */
    for (int i = 2; i < para_num; i++) {
        uint8_t *opt = NULL;
        esp_at_para_parse_result_type result = esp_at_get_para_as_str(i, &opt);
        
        if (result == ESP_AT_PARA_PARSE_RESULT_OK && opt) {
            if (strcasecmp((const char*)opt, "-dd") == 0) {
                /* Found -dd flag, get file path from next parameter */
                if (i + 1 < para_num) {
                    uint8_t *path = NULL;
                    result = esp_at_get_para_as_str(i + 1, &path);
                    if (result == ESP_AT_PARA_PARSE_RESULT_OK && path) {
                        if (!bncurl_params_process_filepath((const char*)path, params->save_path, sizeof(params->save_path))) {
                            const char *e = "+BNCURL: ERROR -dd file path too long (max 120 characters)\r\n";
                            at_uart_write_locked((const uint8_t*)e, strlen(e));
                            return ESP_AT_RESULT_CODE_ERROR;
                        }
                        
                        params->save_to_file = true;
                        i++; /* Skip next parameter as it's the path */
                    } else {
                        const char *e = "+BNCURL: ERROR reading -dd path parameter\r\n";
                        at_uart_write_locked((const uint8_t*)e, strlen(e));
                        return ESP_AT_RESULT_CODE_ERROR;
                    }
                }
            } else if (strcasecmp((const char*)opt, "-du") == 0) {
                /* Found -du flag, get upload parameter from next parameter */
                if (i + 1 < para_num) {
                    uint8_t *param = NULL;
                    result = esp_at_get_para_as_str(i + 1, &param);
                    if (result == ESP_AT_PARA_PARSE_RESULT_OK && param) {
                        if (!bncurl_params_validate_upload((const char*)param, &params->upload_from_file, &params->upload_size)) {
                            const char *e = "+BNCURL: ERROR -du parameter invalid (max 1MB for UART, valid file path for file upload)\r\n";
                            at_uart_write_locked((const uint8_t*)e, strlen(e));
                            return ESP_AT_RESULT_CODE_ERROR;
                        }
                        
                        params->has_upload = true;
                        i++; /* Skip next parameter as it's the upload param */
                        
                        if (params->upload_from_file) {
                            /* Process file path with @ expansion */
                            if (!bncurl_params_process_filepath((const char*)param, params->upload_path, sizeof(params->upload_path))) {
                                const char *e = "+BNCURL: ERROR -du file path too long (max 120 characters)\r\n";
                                at_uart_write_locked((const uint8_t*)e, strlen(e));
                                return ESP_AT_RESULT_CODE_ERROR;
                            }
                        } else {
                            /* Store size parameter as-is for UART input */
                            strncpy(params->upload_path, (const char*)param, sizeof(params->upload_path) - 1);
                            params->upload_path[sizeof(params->upload_path) - 1] = '\0';
                        }
                    } else {
                        const char *e = "+BNCURL: ERROR reading -du parameter\r\n";
                        at_uart_write_locked((const uint8_t*)e, strlen(e));
                        return ESP_AT_RESULT_CODE_ERROR;
                    }
                }
            } else if (strcasecmp((const char*)opt, "-H") == 0) {
                /* Found -H flag, get header from next parameter */
                if (i + 1 < para_num && params->header_count < BNCURL_MAX_HEADERS) {
                    uint8_t *header = NULL;
                    result = esp_at_get_para_as_str(i + 1, &header);
                    if (result == ESP_AT_PARA_PARSE_RESULT_OK && header) {
                        if (!bncurl_params_validate_header((const char*)header)) {
                            const char *e = "+BNCURL: ERROR -H header invalid (max 250 chars, must contain ':')\r\n";
                            at_uart_write_locked((const uint8_t*)e, strlen(e));
                            return ESP_AT_RESULT_CODE_ERROR;
                        }
                        
                        strncpy(params->headers_list[params->header_count], (const char*)header, 
                               sizeof(params->headers_list[params->header_count]) - 1);
                        params->headers_list[params->header_count][sizeof(params->headers_list[params->header_count]) - 1] = '\0';
                        params->header_count++;
                        i++; /* Skip next parameter as it's the header */
                    } else {
                        const char *e = "+BNCURL: ERROR reading -H parameter\r\n";
                        at_uart_write_locked((const uint8_t*)e, strlen(e));
                        return ESP_AT_RESULT_CODE_ERROR;
                    }
                } else {
                    const char *e = "+BNCURL: ERROR too many headers or missing -H parameter\r\n";
                    at_uart_write_locked((const uint8_t*)e, strlen(e));
                    return ESP_AT_RESULT_CODE_ERROR;
                }
            } else if (strcasecmp((const char*)opt, "-c") == 0) {
                /* Found -c flag, get cookie save file from next parameter */
                if (i + 1 < para_num && params->cookie_save_count < BNCURL_MAX_COOKIES) {
                    uint8_t *cookie_path = NULL;
                    result = esp_at_get_para_as_str(i + 1, &cookie_path);
                    if (result == ESP_AT_PARA_PARSE_RESULT_OK && cookie_path) {
                        if (!bncurl_params_process_filepath((const char*)cookie_path, 
                                                           params->cookie_save_paths[params->cookie_save_count], 
                                                           sizeof(params->cookie_save_paths[params->cookie_save_count]))) {
                            const char *e = "+BNCURL: ERROR -c cookie file path too long (max 120 characters)\r\n";
                            at_uart_write_locked((const uint8_t*)e, strlen(e));
                            return ESP_AT_RESULT_CODE_ERROR;
                        }
                        
                        params->save_cookies = true;
                        params->cookie_save_count++;
                        i++; /* Skip next parameter as it's the cookie path */
                    } else {
                        const char *e = "+BNCURL: ERROR reading -c parameter\r\n";
                        at_uart_write_locked((const uint8_t*)e, strlen(e));
                        return ESP_AT_RESULT_CODE_ERROR;
                    }
                } else {
                    const char *e = "+BNCURL: ERROR too many cookie files or missing -c parameter\r\n";
                    at_uart_write_locked((const uint8_t*)e, strlen(e));
                    return ESP_AT_RESULT_CODE_ERROR;
                }
            } else if (strcasecmp((const char*)opt, "-b") == 0) {
                /* Found -b flag, get cookie load file from next parameter */
                if (i + 1 < para_num && params->cookie_load_count < BNCURL_MAX_COOKIES) {
                    uint8_t *cookie_path = NULL;
                    result = esp_at_get_para_as_str(i + 1, &cookie_path);
                    if (result == ESP_AT_PARA_PARSE_RESULT_OK && cookie_path) {
                        if (!bncurl_params_process_filepath((const char*)cookie_path, 
                                                           params->cookie_load_paths[params->cookie_load_count], 
                                                           sizeof(params->cookie_load_paths[params->cookie_load_count]))) {
                            const char *e = "+BNCURL: ERROR -b cookie file path too long (max 120 characters)\r\n";
                            at_uart_write_locked((const uint8_t*)e, strlen(e));
                            return ESP_AT_RESULT_CODE_ERROR;
                        }
                        
                        params->load_cookies = true;
                        params->cookie_load_count++;
                        i++; /* Skip next parameter as it's the cookie path */
                    } else {
                        const char *e = "+BNCURL: ERROR reading -b parameter\r\n";
                        at_uart_write_locked((const uint8_t*)e, strlen(e));
                        return ESP_AT_RESULT_CODE_ERROR;
                    }
                } else {
                    const char *e = "+BNCURL: ERROR too many cookie files or missing -b parameter\r\n";
                    at_uart_write_locked((const uint8_t*)e, strlen(e));
                    return ESP_AT_RESULT_CODE_ERROR;
                }
            } else if (strcasecmp((const char*)opt, "-r") == 0) {
                /* Found -r flag, get range from next parameter */
                if (i + 1 < para_num) {
                    uint8_t *range = NULL;
                    result = esp_at_get_para_as_str(i + 1, &range);
                    if (result == ESP_AT_PARA_PARSE_RESULT_OK && range) {
                        if (!bncurl_params_validate_range((const char*)range, &params->range_start, &params->range_end)) {
                            const char *e = "+BNCURL: ERROR -r range invalid (format: start-end)\r\n";
                            at_uart_write_locked((const uint8_t*)e, strlen(e));
                            return ESP_AT_RESULT_CODE_ERROR;
                        }
                        
                        strncpy(params->range, (const char*)range, sizeof(params->range) - 1);
                        params->range[sizeof(params->range) - 1] = '\0';
                        params->has_range = true;
                        i++; /* Skip next parameter as it's the range */
                    } else {
                        const char *e = "+BNCURL: ERROR reading -r parameter\r\n";
                        at_uart_write_locked((const uint8_t*)e, strlen(e));
                        return ESP_AT_RESULT_CODE_ERROR;
                    }
                }
            } else if (strcasecmp((const char*)opt, "-v") == 0) {
                /* Found -v flag for verbose mode */
                params->verbose = true;
            }
        }
    }
    
    /* Additional validation rules */
    if (params->has_range && !params->save_to_file) {
        const char *e = "+BNCURL: ERROR -r range parameter requires -dd file output\r\n";
        at_uart_write_locked((const uint8_t*)e, strlen(e));
        return ESP_AT_RESULT_CODE_ERROR;
    }
    
    /* Validate POST method requirements */
    if (params->method == BNCURL_POST && !params->has_upload) {
        const char *e = "+BNCURL: ERROR POST method requires -du parameter\r\n";
        at_uart_write_locked((const uint8_t*)e, strlen(e));
        return ESP_AT_RESULT_CODE_ERROR;
    }
    
    return ESP_AT_RESULT_CODE_OK;
}
