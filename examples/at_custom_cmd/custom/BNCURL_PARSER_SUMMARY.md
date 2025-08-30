# BNCURL Parameter Parser Implementation Summary

## Overview

I have implemented a comprehensive AT+BNCURL parameter parser that handles all the specified parameters:

```
AT+BNCURL="method","url"[,"-H","header"][,"-du","<n or file>"][,"-dd","file"][,"-c","cookies"][,"-b","cookies"][,"-r","range"][,"-v"]
```

## Implemented Features

### 1. Core Structure (`bncurl_params_t`)

The parser fills a structure containing:

```c
typedef struct {
    /* Basic parameters */
    bncurl_method_t method;                    // GET, POST, HEAD
    char url[BNCURL_URL_MAX_LEN];             // Request URL
    
    /* File save options (-dd) */
    bool save_to_file;                         // Whether to save to file
    char save_path[BNCURL_FILEPATH_BUFFER_SIZE]; // Save file path
    
    /* Upload options (-du) */
    bool has_upload;                           // Whether upload data present
    bool upload_from_file;                     // Upload from file vs UART
    char upload_path[BNCURL_UPLOAD_PARAM_BUFFER_SIZE]; // Upload source
    size_t upload_size;                        // UART upload size
    
    /* Custom headers (-H) */
    char headers_list[BNCURL_MAX_HEADERS][BNCURL_HEADER_BUFFER_SIZE];
    int header_count;                          // Number of headers
    
    /* Cookies */
    bool save_cookies;                         // Save cookies (-c)
    bool load_cookies;                         // Load cookies (-b)
    char cookie_save_paths[BNCURL_MAX_COOKIES][BNCURL_COOKIE_BUFFER_SIZE];
    int cookie_save_count;
    char cookie_load_paths[BNCURL_MAX_COOKIES][BNCURL_COOKIE_BUFFER_SIZE];
    int cookie_load_count;
    
    /* Range requests (-r) */
    char range[BNCURL_RANGE_BUFFER_SIZE];     // Range string
    bool has_range;                           // Whether range specified
    uint64_t range_start;                     // Parsed range start
    uint64_t range_end;                       // Parsed range end
    
    /* Verbose mode (-v) */
    bool verbose;                             // Verbose output
} bncurl_params_t;
```

### 2. Supported Parameters

#### `-H "header"` (Multiple allowed)
- Validates header format (must contain ':')
- Supports up to `BNCURL_MAX_HEADERS` (10) custom headers
- Example: `-H "Content-Type: application/json" -H "Authorization: Bearer token"`

#### `-du "<n or file>"` (POST only)
- Numeric value: Upload N bytes from UART after `>` prompt
- File path: Upload from SD card file (supports `@` prefix)
- Examples: 
  - `-du "100"` (upload 100 bytes from UART)
  - `-du "@/upload/data.bin"` (upload from /sdcard/upload/data.bin)

#### `-dd "file"` (Save response)
- Save HTTP response to specified file path
- Supports `@` prefix for SD card mount point
- Example: `-dd "@/download/response.json"` → `/sdcard/download/response.json`

#### `-c "cookies"` (Save cookies)
- Save received cookies to specified file
- Supports multiple cookie files
- Example: `-c "@/cookies/session.txt"`

#### `-b "cookies"` (Load cookies)
- Load cookies from specified file for request
- Supports multiple cookie files
- Example: `-b "@/cookies/auth.txt"`

#### `-r "start-end"` (Range requests, GET only)
- HTTP Range header for partial content
- Must be used with `-dd` (file output required)
- Validates range format and logic
- Example: `-r "0-1023"` (download first 1024 bytes)

#### `-v` (Verbose mode)
- Enable verbose output (like curl -v)
- No additional parameters

### 3. Validation Rules

#### Method Validation
- **GET**: Supports all parameters except `-du`
- **POST**: Requires `-du` parameter
- **HEAD**: Supports all parameters except `-du` and `-r`

#### Parameter Constraints
- No duplicate parameters (e.g., two `-dd` flags = ERROR)
- `-r` requires `-dd` (range downloads must save to file)
- `-du` only valid with POST method
- `-r` only valid with GET method

#### File Path Processing
- `@` prefix expands to SD card mount point (`/sdcard`)
- `@/file.txt` → `/sdcard/file.txt`
- `@file.txt` → `/sdcard/file.txt`
- Path length validation (max 120 chars before expansion)

### 4. Error Handling

The parser provides detailed error messages for:
- Invalid methods
- Missing required parameters
- Duplicate parameters
- Invalid parameter combinations
- Path length violations
- Invalid range formats
- Header format errors

### 5. Two-Pass Parsing

1. **First Pass**: Validates all parameters and checks for errors
2. **Second Pass**: Extracts and processes parameter values

This ensures complete validation before any processing occurs.

## Usage Example

```c
// In AT command handler
static uint8_t at_bncurl_cmd(uint8_t para_num)
{
    bncurl_params_t params;
    
    // Parse all parameters
    uint8_t result = bncurl_params_parse(para_num, &params);
    if (result != ESP_AT_RESULT_CODE_OK) {
        return result; // Error already reported
    }
    
    // Use parsed parameters
    if (params.method == BNCURL_POST && params.has_upload) {
        if (params.upload_from_file) {
            // Upload from params.upload_path
        } else {
            // Expect params.upload_size bytes from UART
        }
    }
    
    // Process headers, cookies, range, etc.
    return ESP_AT_RESULT_CODE_OK;
}
```

## Example Commands

```bash
# Basic GET
AT+BNCURL="GET","https://httpbin.org/get"

# GET with file download
AT+BNCURL="GET","https://httpbin.org/json","-dd","@/response.json"

# POST with UART upload and headers
AT+BNCURL="POST","https://httpbin.org/post","-du","100","-H","Content-Type: text/plain"

# POST with file upload
AT+BNCURL="POST","https://httpbin.org/post","-du","@/upload/data.bin","-dd","@/response.json"

# GET with range download
AT+BNCURL="GET","https://httpbin.org/bytes/1024","-r","0-511","-dd","@/partial.bin"

# Complex example with cookies and verbose
AT+BNCURL="POST","https://api.example.com/upload","-du","@/data.json","-dd","@/response.json","-H","Authorization: Bearer token","-c","@/session.cookies","-b","@/auth.cookies","-v"
```

## Files Modified/Created

1. **bncurl_params.h** - Updated structure and function declarations
2. **bncurl_params.c** - Complete implementation of parameter parsing
3. **atbn_config.h** - Added missing configuration constants
4. **bncurl_params_example.c** - Usage examples and documentation

The implementation follows the test cases from `test.md` and provides robust parameter parsing with comprehensive error handling for the ESP32 AT+BNCURL command.
