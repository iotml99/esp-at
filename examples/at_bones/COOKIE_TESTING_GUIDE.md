# BNCURL Cookie Testing Guide

## Cookie Implementation Summary

The BNCURL library now supports cookie handling with the following features:

### Cookie Parameters
- `-c <file>` : Save received cookies to file AND stream to UART
- `-b <file>` : Load cookies from file and send with request

### Cookie Features
1. **Cookie Saving (-c option)**:
   - Saves cookies to specified file on SD card
   - Also streams cookies to UART immediately for host access
   - Creates directories if they don't exist
   - Overwrites existing cookie files

2. **Cookie Loading (-b option)**:
   - Loads cookies from file on SD card
   - Sends cookies with HTTP request
   - Validates file exists before request

3. **Cookie Streaming**:
   - All received cookies are streamed to UART with `+COOKIE:` prefix
   - Format: `+COOKIE:name=value; Domain=domain; Path=path; Secure; HttpOnly`
   - Immediate feedback to host for session management

## Test Examples

### Test 1: Set a Cookie (Save)
```
AT+BNSD_MOUNT
AT+BNCURL="POST","http://httpbin.org/cookies/set/test_cookie/hello_world","-c","@cookies.txt"
```
Expected Output:
- `+LEN:` marker with content length
- `+POST:` chunks with response data
- `+COOKIE:test_cookie=hello_world` streamed to UART
- Cookie saved to `/sdcard/cookies.txt`

### Test 2: Use Saved Cookies (Load)
```
AT+BNCURL="GET","http://httpbin.org/cookies","-b","@cookies.txt"
```
Expected Output:
- Cookies loaded from file and sent with request
- Response shows the cookies were sent

### Test 3: Combined Cookie Operations
```
AT+BNCURL="GET","http://httpbin.org/cookies/set/session_id/abc123","-c","@session.txt","-b","@cookies.txt"
```
Expected Output:
- Sends existing cookies from cookies.txt
- Receives new session_id cookie
- Saves all cookies to session.txt
- Streams new cookie to UART

## Cookie File Format

Cookies are saved in Netscape cookie file format:
```
# Netscape HTTP Cookie File
# This is a generated file! Do not edit.

domain	domain_specified	path	secure	expires	name	value
httpbin.org	TRUE	/	FALSE	0	test_cookie	hello_world
```

## Implementation Details

### Files Added:
- `bncurl_cookies.h` - Cookie handling interface
- `bncurl_cookies.c` - Cookie implementation

### Key Functions:
- `bncurl_cookies_load_from_file()` - Load cookies for sending
- `bncurl_cookies_configure_saving()` - Configure cookie capturing
- `bncurl_cookies_parse_and_add()` - Parse Set-Cookie headers
- `bncurl_cookies_stream_to_uart()` - Stream cookies to host
- `bncurl_cookies_save_to_file()` - Save cookies to file

### Integration:
- Integrated into `bncurl_common_execute_request()`
- Uses combined header callback for content-length and cookies
- Automatic cleanup after request completion

## Error Handling

### Cookie File Errors:
- SD card not mounted when using @ paths
- Cookie file doesn't exist for loading (-b)
- Cannot create directories for saving (-c)
- File permission errors

### Cookie Parsing:
- Invalid Set-Cookie header format
- Maximum cookie limit reached (16 cookies per request)
- Cookie name/value too long

## Testing Notes

1. **Use HTTP for initial testing** to avoid SSL certificate issues
2. **Mount SD card first** with `AT+BNSD_MOUNT`
3. **Check available space** with `AT+BNSD_SPACE?`
4. **Use httpbin.org** for reliable cookie testing endpoints

## Common Test Endpoints

```
# Set single cookie
http://httpbin.org/cookies/set/name/value

# Set multiple cookies
http://httpbin.org/cookies/set?cookie1=value1&cookie2=value2

# View current cookies
http://httpbin.org/cookies

# Cookie deletion (by setting expired cookie)
http://httpbin.org/cookies/delete?cookie1
```

## Troubleshooting

### SSL Certificate Issues:
For HTTPS endpoints that fail with certificate errors:
- Use HTTP equivalents when available
- Check system time with `AT+CIPSNTPTIME?`
- Some APIs (like openweathermap.org) may not work due to CA bundle limitations

### Cookie File Issues:
- Ensure SD card is mounted: `AT+BNSD_MOUNT`
- Check SD card space: `AT+BNSD_SPACE?`
- Use @ prefix for SD card paths: `@cookies.txt` → `/sdcard/cookies.txt`

### Verbose Debugging:
Add `-v` flag to see detailed HTTP communication:
```
AT+BNCURL="GET","http://httpbin.org/cookies","-b","@cookies.txt","-v"
```
