# BNCURL Range Download Testing Guide

## Range Download Implementation Summary

The BNCURL library now supports HTTP Range requests for partial file downloads with the following features:

### Range Download Parameters
- `-r <start-end>` : Download a specific byte range (GET requests only)
- Optional `-dd <@file>` : Save to file (appends to existing), or stream to UART if omitted
- Data handling:
  - **With -dd**: Data is **APPENDED** to the target file (host responsibility for sequencing)
  - **Without -dd**: Data is streamed to UART with +POST: chunks

### Range Download Features
1. **HTTP Range Header**: Automatically adds `Range: bytes=start-end` header
2. **Dual Output Mode**: 
   - File mode: Opens target file in append mode for sequential downloads
   - UART mode: Streams data chunks to UART with range metadata
3. **Sequential Downloads**: Supports downloading large files in chunks
4. **No Position Checking**: Host is responsible for correct range sequencing
5. **Progress Tracking**: Shows bytes downloaded for each range

## Implementation Details

### Core Changes:
1. **Range Header Addition**: Automatically adds `Range: bytes=X-Y` to GET requests
2. **Dual Output Mode**: 
   - File mode: Files opened with `"ab"` mode for range downloads
   - UART mode: Data streamed with +POST: chunks and range metadata
3. **Stream Initialization**: New `bncurl_stream_init_with_range()` function
4. **Validation**: Range requests work with both file output (-dd) and UART streaming

### File Handling:
- **Regular Download**: `fopen(file, "wb")` - overwrites existing file
- **Range Download**: `fopen(file, "ab")` - appends to existing file
- **UART Streaming**: Data sent as +POST: chunks with range completion info
- **Current Size Logging**: Shows existing file size before appending range data

## Test Examples

### Test 1: Download Large File in Ranges (Basic)
```bash
# Mount SD card first
AT+BNSD_MOUNT

# Download first 2MB (0-2097151 bytes)
AT+BNCURL="GET","http://httpbin.org/range/8388608","-dd","@large_file.bin","-r","0-2097151"

# Download second 2MB (2097152-4194303 bytes) 
AT+BNCURL="GET","http://httpbin.org/range/8388608","-dd","@large_file.bin","-r","2097152-4194303"

# Download third 2MB (4194304-6291455 bytes)
AT+BNCURL="GET","http://httpbin.org/range/8388608","-dd","@large_file.bin","-r","4194304-6291455"

# Download final 2MB (6291456-8388607 bytes)
AT+BNCURL="GET","http://httpbin.org/range/8388608","-dd","@large_file.bin","-r","6291456-8388607"
```

### Test 2: Download MP3 File in Parts (Real-world Example)
```bash
# Based on user's example (adjusted for testing)
AT+BNCURL="GET","http://commondatastorage.googleapis.com/gtv-videos-bucket/sample/BigBuckBunny.mp4","-dd","@video.mp4","-r","0-2097151"
AT+BNCURL="GET","http://commondatastorage.googleapis.com/gtv-videos-bucket/sample/BigBuckBunny.mp4","-dd","@video.mp4","-r","2097152-4194303"
AT+BNCURL="GET","http://commondatastorage.googleapis.com/gtv-videos-bucket/sample/BigBuckBunny.mp4","-dd","@video.mp4","-r","4194304-6291455"
```

### Test 3: Range Download to UART (Streaming Mode)
```bash
# Download specific ranges directly to UART for processing
# First 1MB directly to UART
AT+BNCURL="GET","http://httpbin.org/range/8388608","-r","0-1048575"

# Second 1MB to UART  
AT+BNCURL="GET","http://httpbin.org/range/8388608","-r","1048576-2097151"

# Specific range to UART with verbose output
AT+BNCURL="GET","http://httpbin.org/range/8388608","-r","2097152-4194303","-v"
```

### Test 4: Range Download with Other Parameters
```bash
# Range download with verbose output and custom headers
AT+BNCURL="GET","http://httpbin.org/range/4194304","-dd","@test_range.bin","-r","1048576-2097151","-v","-H","User-Agent: ESP32-Range-Test"

# Range download with cookies
AT+BNCURL="GET","http://httpbin.org/range/4194304","-dd","@test_range.bin","-r","0-1048575","-c","@range_cookies.txt","-b","@session.txt"
```

## Expected Behavior

### First Range Request:
```
AT+BNCURL="GET","http://httpbin.org/range/8388608","-dd","@file.bin","-r","0-2097151"

Expected Output:
- +LEN: marker with range content length (2097152 bytes)
- File opened in WRITE mode (new file)
- Downloads 2MB of data
- SEND OK
```

### Subsequent Range Requests:
```
AT+BNCURL="GET","http://httpbin.org/range/8388608","-dd","@file.bin","-r","2097152-4194303"

Expected Output:
- +LEN: marker with range content length (2097152 bytes)  
- File opened in APPEND mode (existing file size logged)
- Downloads next 2MB of data and appends
- SEND OK
```

### Range Download to UART:
```
AT+BNCURL="GET","http://httpbin.org/range/8388608","-r","0-1048575"

Expected Output:
- +RANGE_INFO:uart_stream=true
- +LEN: marker with range content length (1048576 bytes)
- +POST:512,<data> (streaming chunks)
- +POST:512,<data> (continues until range complete)
- +RANGE_FINAL:uart_bytes=1048576
- SEND OK
```

### File Size Verification:
```bash
# Check file size after each range
AT+BNSD_SPACE?

# First range: file should be ~2MB
# Second range: file should be ~4MB
# Third range: file should be ~6MB
# Fourth range: file should be ~8MB
```

## Range Calculation Helper

### Common Range Sizes:
- **1MB chunks**: `0-1048575`, `1048576-2097151`, `2097152-3145727`, etc.
- **2MB chunks**: `0-2097151`, `2097152-4194303`, `4194304-6291455`, etc.
- **512KB chunks**: `0-524287`, `524288-1048575`, `1048576-1572863`, etc.

### Range Calculation Formula:
```
For chunk size N bytes:
Range 0: 0-(N-1)
Range 1: N-(2*N-1)  
Range 2: 2*N-(3*N-1)
Range i: i*N-((i+1)*N-1)
```

## HTTP Range Response Details

### Server Response Headers:
```
HTTP/1.1 206 Partial Content
Content-Range: bytes 0-2097151/8388608
Content-Length: 2097152
Accept-Ranges: bytes
```

### Response Processing:
- **206 Partial Content**: Indicates successful range request
- **Content-Range**: Shows actual range served and total file size
- **Content-Length**: Size of this specific range (not total file size)

## Error Handling

### Range Request Errors:
1. **416 Range Not Satisfiable**: Requested range exceeds file size
2. **Server doesn't support ranges**: Returns 200 with full content
3. **Invalid range format**: Parameter validation error

### Validation Errors:
```
ERROR: POST/HEAD methods cannot have range (-r)  
ERROR: Range parameter too long. Max length: 24
ERROR: Invalid range format: must be start-end with start <= end
```

### File Errors:
```
Failed to open file for appending: /sdcard/file.bin
SD card must be mounted to use @ file paths
```

## Testing Notes

### Host Responsibilities:
1. **Correct Sequencing**: Ensure ranges are downloaded in proper order
2. **No Gaps**: Verify ranges are contiguous (no missing bytes)
3. **No Overlaps**: Avoid downloading same bytes multiple times
4. **Error Recovery**: Handle failed ranges and retry if needed
5. **Final Verification**: Verify complete file integrity after all ranges

### Server Requirements:
- Server must support HTTP Range requests (Accept-Ranges: bytes)
- Server must return 206 Partial Content for valid ranges
- Server must include Content-Range header in response

### Testing Endpoints:
```bash
# HTTPBin range endpoint (8MB test file)
http://httpbin.org/range/8388608

# Large file for testing (requires real server)
http://commondatastorage.googleapis.com/gtv-videos-bucket/sample/BigBuckBunny.mp4

# Custom range test (returns requested range of zeros)
http://httpbin.org/range/1048576  # 1MB test file
```

## Troubleshooting

### Range Not Working:
1. Check server supports ranges: `curl -I -H "Range: bytes=0-1023" <url>`
2. Verify range format: `start-end` (both inclusive)
3. Ensure file download specified: `-dd "@file.ext"`
4. Check SD card mounted: `AT+BNSD_MOUNT`

### File Size Issues:
1. Use `AT+BNSD_SPACE?` to check available space
2. Monitor file growth with each range download
3. Verify ranges are contiguous and non-overlapping

### Performance Optimization:
- Use 1-2MB chunk sizes for optimal performance
- Monitor memory usage during large range downloads
- Consider network stability for large files

## Advanced Usage

### Resume Interrupted Downloads:
```bash
# Check current file size first
# Then calculate next range based on existing data
# Example: If file is 3MB, next range starts at byte 3145728
```

### Parallel Range Downloads:
Note: Current implementation processes one request at a time.
For parallel downloads, multiple ESP32 devices would be needed.

### Range Verification:
```bash
# Download known file with ranges
# Compare final file checksum with reference
# Verify no data corruption occurred
```
