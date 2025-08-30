# BNCURL File System Integration Example

## Overview
This document demonstrates the enhanced BNCURL parameter parsing with comprehensive file system validation.

## Example Commands and Expected Behavior

### 1. Download to SD Card with Directory Creation

**Command:**
```
AT+BNCURL="GET","https://httpbin.org/json","-dd","@Downloads/test.json"
```

**Expected Behavior:**
1. Path `@Downloads/test.json` is normalized to `/sdcard/Downloads/test.json`
2. SD card mount status is validated (must be mounted)
3. `/sdcard/Downloads/` directory is created recursively if it doesn't exist
4. Disk space is validated for the download
5. If file exists, it will be overwritten (with info message)
6. HTTP download proceeds to the specified file
7. `SEND OK` is returned on successful completion

**Error Scenarios:**
- SD card not mounted: `ERROR: SD card must be mounted to use @ file paths`
- Cannot create directory: `ERROR: Failed to create directory for file: /sdcard/Downloads/test.json`
- Insufficient disk space: `ERROR: Cannot create file /sdcard/Downloads/test.json: insufficient disk space or permission denied`

### 2. POST with File Upload from SD Card

**Command:**
```
AT+BNCURL="POST","https://httpbin.org/post","-du","@uploads/data.txt","-H","Content-Type: text/plain"
```

**Expected Behavior:**
1. Path `@uploads/data.txt` is normalized to `/sdcard/uploads/data.txt`
2. SD card mount status is validated
3. File `/sdcard/uploads/data.txt` must exist and be readable
4. HTTP POST proceeds with file content as body
5. `SEND OK` is returned on successful completion

**Error Scenarios:**
- SD card not mounted: `ERROR: SD card must be mounted to use @ file paths`
- File doesn't exist: `ERROR: File does not exist: /sdcard/uploads/data.txt`
- File not readable: `ERROR: Cannot open file for reading: /sdcard/uploads/data.txt`

### 3. Cookie Operations with SD Card Files

**Command:**
```
AT+BNCURL="GET","https://httpbin.org/cookies","-b","@cookies/session.txt","-c","@cookies/new_session.txt"
```

**Expected Behavior:**
1. Cookie send file: `/sdcard/cookies/session.txt` must exist and be readable
2. Cookie save file: `/sdcard/cookies/` directory created if needed
3. Both operations validated against SD card mount status

### 4. Error Case - SD Card Not Mounted

**Command:**
```
AT+BNCURL="GET","https://httpbin.org/json","-dd","@test.json"
```

**When SD card is not mounted:**
```
Parsing BNCURL command with 4 parameters
INFO: Normalized path with mount point: /sdcard/test.json
ERROR: SD card is not mounted but file paths are specified
ERROR: SD card must be mounted to use @ file paths
```

**Returns:** `ESP_AT_RESULT_CODE_ERROR`

## Implementation Details

### File System Validation Features

1. **SD Card Mount Validation**
   - Automatically detects when @ paths are used
   - Validates SD card is mounted before proceeding
   - Returns clear error messages if not mounted

2. **Recursive Directory Creation**
   - Creates all parent directories automatically
   - Uses POSIX `mkdir()` with proper error handling
   - Logs directory creation for debugging

3. **File Existence Validation**
   - Upload files must exist and be readable
   - Cookie send files must exist and be readable
   - Download/cookie save files create directories as needed

4. **Disk Space Validation**
   - Tests file creation before HTTP operation
   - Provides early error detection
   - Prevents partial downloads on full disk

5. **File Overwrite Handling**
   - Existing download files are overwritten
   - Informational message provided
   - No confirmation required (consistent with curl behavior)

### Error Reporting

All errors include:
- ESP-IDF log messages for debugging
- User-friendly printf messages for AT interface
- Specific error codes for different failure types
- File path information in error messages

### Performance Considerations

- Validation is performed during parameter parsing
- File system checks are minimal and fast
- Directory creation is only done when needed
- Test file creation is lightweight (creates and deletes small temp file)

## Integration with BNCURL Execution

The parameter parsing now provides:
- Validated and normalized file paths
- Guaranteed directory structure
- Early error detection before HTTP operations
- Consistent error handling across all file operations

This ensures that when the BNCURL executor receives the parameters, all file system prerequisites are met, leading to more reliable downloads and uploads.
