# ESP32 AT Bones - Comprehensive Developer Guide

**Version:** 2.0  
**Date:** August 31, 2025  
**Framework:** ESP-AT with Custom Extensions  

## 📋 Table of Contents

1. [Overview](#overview)
2. [Architecture](#architecture)
3. [HTTP/HTTPS Commands (BNCURL)](#httphttps-commands-bncurl)
4. [SD Card Management (BNSD)](#sd-card-management-bnsd)
5. [WiFi Protected Setup (BNWPS)](#wifi-protected-setup-bnwps)
6. [Certificate Management (BNCERT)](#certificate-management-bncert)
7. [Web Radio Streaming (BNWEB_RADIO)](#web-radio-streaming-bnweb_radio)
8. [Configuration & Limits](#configuration--limits)
9. [Error Handling](#error-handling)
10. [Performance Optimization](#performance-optimization)
11. [Integration Guide](#integration-guide)
12. [Troubleshooting](#troubleshooting)

---

## Overview

The AT Bones framework is a comprehensive ESP32 AT command extension that provides advanced HTTP/HTTPS capabilities, SD card management, WiFi setup automation, certificate handling, and web radio streaming. Built on top of ESP-AT, it extends the standard AT command set with powerful networking and storage features.

### Key Features

- **Advanced HTTP/HTTPS Client** - Full-featured HTTP client with range downloads, cookies, custom headers
- **Certificate Management** - Dynamic certificate flashing and automatic type detection  
- **SD Card Operations** - Mount, format, space management with FAT32 support
- **WiFi Protected Setup** - Automated WPS connection with timeout controls
- **Web Radio Streaming** - Continuous audio streaming with real-time UART output
- **Range Downloads** - Efficient partial content downloads with resume capability
- **Cookie Support** - Persistent cookie storage and management
- **HTTPS Integration** - Seamless certificate integration with fallback strategies

### Prerequisites

- **Hardware:** ESP32 with minimum 4MB flash
- **Framework:** ESP-IDF 5.0+
- **Components:** libcurl-esp32, fatfs, sdmmc driver
- **Memory:** Minimum 512KB RAM for simultaneous operations
- **Storage:** SD card (optional, for SD operations)

---

## Architecture

### Component Structure

```
at_bones/
├── Core Framework
│   ├── at_bones.c           # Main AT command registration
│   ├── util.c/h             # Utility functions
│   └── CMakeLists.txt       # Build configuration
├── HTTP/HTTPS (BNCURL)
│   ├── bncurl.c/h           # Main BNCURL interface
│   ├── bncurl_common.c/h    # Common HTTP functionality
│   ├── bncurl_executor.c/h  # Command execution engine
│   ├── bncurl_params.c/h    # Parameter handling
│   ├── bncurl_get.c/h       # GET method implementation
│   ├── bncurl_post.c/h      # POST method implementation
│   ├── bncurl_head.c/h      # HEAD method implementation
│   ├── bncurl_stream.c      # Streaming functionality
│   ├── bncurl_cookies.c/h   # Cookie management
│   └── bncurl_config.h      # Configuration constants
├── Storage Management
│   └── at_sd.c/h            # SD card operations
├── WiFi Setup
│   └── bnwps.c/h            # WPS functionality
├── Certificate Management
│   ├── bncert.c/h           # Certificate flashing
│   └── bncert_manager.c/h   # Certificate management
├── Audio Streaming
│   └── bnwebradio.c/h       # Web radio streaming
└── Documentation
    ├── COMPREHENSIVE_AT_BONES_README.md
    ├── BNCERT_HTTPS_TESTING_GUIDE.md
    ├── COOKIE_TESTING_GUIDE.md
    ├── RANGE_DOWNLOAD_GUIDE.md
    └── WEBRADIO_README.md
```

### Memory Layout

| Component | RAM Usage | Flash Usage | Notes |
|-----------|-----------|-------------|-------|
| BNCURL Core | 16KB | 45KB | Basic HTTP functionality |
| Certificate Manager | 8KB | 12KB | Certificate storage and validation |
| SD Card Driver | 4KB | 8KB | FAT32 filesystem support |
| WPS Module | 2KB | 6KB | WiFi setup automation |
| Web Radio | 12KB | 15KB | Audio streaming (during operation) |
| **Total** | **42KB** | **86KB** | **Approximate values** |

---

## HTTP/HTTPS Commands (BNCURL)

### AT+BNCURL - HTTP Request Execution

The main HTTP/HTTPS client command with support for all major HTTP methods.

#### Command Syntax

```
AT+BNCURL=<method>,<url>[,<headers>][,<redirect>][,<timeout>][,<range>][,<data_flag>][,<upload_size>]
```

#### Parameters

| Parameter | Type | Range | Description |
|-----------|------|-------|-------------|
| `method` | String | GET/POST/PUT/DELETE/HEAD/PATCH | HTTP method |
| `url` | String | 1-2048 chars | Target URL (HTTP/HTTPS) |
| `headers` | String | 0-4096 chars | Custom headers (optional) |
| `redirect` | Integer | 0-10 | Max redirects (default: 5) |
| `timeout` | Integer | 1-300 | Timeout in seconds (default: 30) |
| `range` | String | Format: "start-end" | Byte range (optional) |
| `data_flag` | String | -dd/-du | Data direction flag |
| `upload_size` | Integer | 1-1048576 | Upload data size for POST/PUT |

#### Response Format

**Success Response:**
```
OK
+LEN:<content_length>
+SEND:<data_chunk>
...
SEND OK
```

**Error Response:**
```
+HTTP_CODE:<status_code>
+ERROR:<error_message>
ERROR
```

#### Usage Examples

**1. Simple GET Request**
```
AT+BNCURL=GET,"https://httpbin.org/get"
OK
+LEN:326
+SEND:{"args":{},"headers":{"Accept":"*/*","Host":"httpbin.org"},"origin":"1.2.3.4","url":"https://httpbin.org/get"}
SEND OK
```

**2. POST with JSON Data**
```
AT+BNCURL=POST,"https://httpbin.org/post","Content-Type: application/json",5,30,,-du,25
OK
> {"key":"value","test":123}
+LEN:489
+SEND:{"args":{},"data":"{\"key\":\"value\",\"test\":123}","headers":{"Content-Type":"application/json"},"json":{"key":"value","test":123},"origin":"1.2.3.4","url":"https://httpbin.org/post"}
SEND OK
```

**3. Range Download**
```
AT+BNCURL=GET,"https://httpbin.org/bytes/1000","",5,30,"0-499",-dd
OK
+LEN:500
+SEND:<500 bytes of data>
SEND OK
```

**4. Custom Headers**
```
AT+BNCURL=GET,"https://httpbin.org/headers","User-Agent: MyApp/1.0\r\nX-Custom-Header: test"
OK
+LEN:234
+SEND:{"headers":{"Accept":"*/*","Host":"httpbin.org","User-Agent":"MyApp/1.0","X-Custom-Header":"test"}}
SEND OK
```

### AT+BNCURL_TIMEOUT - Server Response Timeout Configuration

Controls the server response timeout for HTTP operations. This is **NOT** a total download timeout, but a server inactivity timeout. If no data is received from the server for the specified duration, the connection will be closed and an error returned.

#### Command Syntax

```
AT+BNCURL_TIMEOUT=<timeout>  # Set server response timeout
AT+BNCURL_TIMEOUT?           # Query current timeout
AT+BNCURL_TIMEOUT=?          # Test command
```

#### Parameters

| Parameter | Type | Range | Default | Description |
|-----------|------|-------|---------|-------------|
| `timeout` | Integer | 1-120 | 30 | Server response timeout in seconds |

#### Behavior

- **Server Response Timeout**: If no data is received from server for `timeout` seconds, abort
- **Total Operation Timeout**: Set to `timeout × 10` as safety net for very large downloads
- **Connection Timeout**: Separate 30-second timeout for initial connection establishment

#### Examples

```
AT+BNCURL_TIMEOUT=60
OK

AT+BNCURL_TIMEOUT?
+BNCURL_TIMEOUT:60
OK
```

### AT+BNCURL_STOP - Stop Active Request

Cancels any ongoing HTTP operation.

#### Command Syntax

```
AT+BNCURL_STOP?              # Stop current operation
```

#### Example

```
# During a long download
AT+BNCURL_STOP?
+BNCURL_STOP:1
OK
```

### AT+BNCURL_PROG - Progress Monitoring

Queries the progress of ongoing HTTP operations.

#### Command Syntax

```
AT+BNCURL_PROG?              # Query progress
```

#### Response Format

```
+BNCURL_PROG:<bytes_downloaded>,<total_bytes>,<percentage>
```

#### Example

```
AT+BNCURL_PROG?
+BNCURL_PROG:524288,1048576,50
OK
```

### HTTPS Configuration

HTTPS requests automatically integrate with the certificate management system:

1. **Partition Certificates** - Custom certificates override defaults
2. **Hardcoded CA Bundle** - Embedded certificate bundle for common sites
3. **Permissive Mode** - Fallback for maximum compatibility

### File Path Requirements

**IMPORTANT:** All file paths in AT Bones commands **MUST** start with the `@` prefix to indicate SD card storage. File paths not starting with `@` will be rejected with an immediate error.

#### Valid File Path Examples
```
@/documents/file.txt         # Valid - starts with @
@/logs/access.log           # Valid - starts with @
@/certificates/ca.crt       # Valid - starts with @
```

#### Invalid File Path Examples  
```
/documents/file.txt         # INVALID - missing @ prefix
C:\files\data.txt          # INVALID - missing @ prefix  
documents/file.txt         # INVALID - missing @ prefix
```

#### Supported File Operations
- **`-dd @/path/file.ext`** - Download to SD card file
- **`-du @/path/file.ext`** - Upload from SD card file  
- **`-c @/path/cookies.txt`** - Save cookies to SD card file
- **`-b @/path/cookies.txt`** - Load cookies from SD card file
- **`AT+BNCERT=0x123456,@/path/cert.pem`** - Flash certificate from SD card file

### Cookie Support

Cookies are automatically handled when headers contain `Set-Cookie` responses:

```
# Cookie will be stored and sent with subsequent requests to same domain
AT+BNCURL=GET,"https://httpbin.org/cookies/set/sessionid/abc123"
AT+BNCURL=GET,"https://httpbin.org/cookies"
# Will include: Cookie: sessionid=abc123
```

### Range Download Features

#### Automatic Streaming (-dd flag)
When `-dd` flag is present, range data streams directly to UART without file creation:

```
AT+BNCURL=GET,"https://example.com/large-file.zip","",5,30,"1000000-1999999",-dd
# Streams 1MB chunk directly to UART
```

#### File Download (no -dd flag)
Without `-dd`, downloads are saved to specified file:

```
AT+BNCURL=GET,"https://example.com/file.pdf","",5,30,"0-"
# Downloads entire file and saves to configured path
```

---

## SD Card Management (BNSD)

Comprehensive SD card management with FAT32 filesystem support.

### AT+BNSD_MOUNT - Mount SD Card

Mounts SD card with specified filesystem.

#### Command Syntax

```
AT+BNSD_MOUNT=<mount_path>   # Mount at path
AT+BNSD_MOUNT?               # Query mount status
AT+BNSD_MOUNT=?              # Test command
```

#### Parameters

| Parameter | Type | Range | Description |
|-----------|------|-------|-------------|
| `mount_path` | String | 1-64 chars | Mount point path (e.g., "/sdcard") |

#### Examples

```
AT+BNSD_MOUNT="/sdcard"
OK

AT+BNSD_MOUNT?
+BNSD_MOUNT:"/sdcard",1
OK
```

### AT+BNSD_UNMOUNT - Unmount SD Card

Safely unmounts the SD card filesystem.

#### Command Syntax

```
AT+BNSD_UNMOUNT              # Unmount current SD card
AT+BNSD_UNMOUNT?             # Query unmount status
```

#### Example

```
AT+BNSD_UNMOUNT
OK

AT+BNSD_UNMOUNT?
+BNSD_UNMOUNT:0
OK
```

### AT+BNSD_SPACE - Disk Space Information

Queries available disk space on mounted SD card.

#### Command Syntax

```
AT+BNSD_SPACE?               # Query disk space
```

#### Response Format

```
+BNSD_SPACE:<total_bytes>,<free_bytes>,<used_bytes>
```

#### Example

```
AT+BNSD_SPACE?
+BNSD_SPACE:8056832000,7234567890,822264110
OK
```

### AT+BNSD_FORMAT - Format SD Card

Formats SD card with FAT32 filesystem.

#### Command Syntax

```
AT+BNSD_FORMAT               # Format with default settings
AT+BNSD_FORMAT?              # Query format capability
```

#### Example

```
AT+BNSD_FORMAT
+BNSD_FORMAT:FORMATTING...
OK
```

### SD Card Configuration

#### Supported Filesystems
- **FAT32** - Primary filesystem (recommended)
- **FAT16** - Legacy support for smaller cards
- **exFAT** - For cards >32GB (if enabled in menuconfig)

#### Hardware Requirements
- **SPI Interface** - 4-wire SPI connection to ESP32
- **Card Types** - SD, SDHC, SDXC (up to 2TB)
- **Speed Classes** - Class 4+ recommended for streaming
- **Voltage** - 3.3V operation

#### Pin Configuration (Default SPI)
| SD Pin | ESP32 Pin | Function |
|--------|-----------|----------|
| CLK | GPIO18 | SPI Clock |
| CMD | GPIO19 | SPI MOSI |
| DAT0 | GPIO23 | SPI MISO |
| DAT3 | GPIO5 | SPI CS |

---

## WiFi Protected Setup (BNWPS)

Automated WiFi connection using WPS (WiFi Protected Setup).

### AT+BNWPS - WPS Control

Initiates or queries WPS connection process.

#### Command Syntax

```
AT+BNWPS=<duration>          # Start WPS for duration seconds
AT+BNWPS=0                   # Query WPS status
AT+BNWPS?                    # Query current WPS state
AT+BNWPS=?                   # Test command
```

#### Parameters

| Parameter | Type | Range | Description |
|-----------|------|-------|-------------|
| `duration` | Integer | 0, 30-300 | WPS timeout (0=query, 30-300=activate) |

#### Response Format

**Status Query (duration=0):**
```
+BNWPS:<state>,<elapsed_time>
```

**WPS States:**
- `0` - WPS_IDLE
- `1` - WPS_SCANNING  
- `2` - WPS_ACTIVE
- `3` - WPS_SUCCESS
- `4` - WPS_FAILED
- `5` - WPS_TIMEOUT

#### Examples

**1. Query WPS Status**
```
AT+BNWPS=0
+BNWPS:0,0
OK
```

**2. Start WPS for 120 seconds**
```
AT+BNWPS=120
OK
# User presses WPS button on router
+CWJAP:"MyNetwork","aa:bb:cc:dd:ee:ff",6,-45
# Automatically connects when WPS succeeds
```

**3. Check WPS Progress**
```
AT+BNWPS?
+BNWPS:2,45
OK
# State 2 (WPS_ACTIVE), running for 45 seconds
```

### WPS Configuration

#### Supported WPS Methods
- **Push Button** - Physical button on router
- **PIN Method** - 8-digit PIN entry (if supported by router)

#### Security Notes
- WPS is disabled by default on many modern routers
- Enable WPS temporarily only during connection
- Disable WPS after successful connection for security

#### Timeout Behavior
- **Manual cancellation** - Stop with `AT+BNWPS=0`
- **Automatic timeout** - Returns to idle after specified duration
- **Success connection** - Automatically stops and connects

---

## Certificate Management (BNCERT)

Dynamic certificate management system for HTTPS operations.

### AT+BNFLASH_CERT - Certificate Flashing

Flashes certificates to dedicated partition storage.

#### Command Syntax

```
AT+BNFLASH_CERT=<address>,<data_source>
```

#### Parameters

| Parameter | Type | Range | Description |
|-----------|------|-------|-------------|
| `address` | Hex | 0x380000-0x3BFFFF | Flash address in certificate partition |
| `data_source` | String | uart/file | Data source (uart for direct input) |

#### Certificate Partition Layout

| Address Range | Size | Purpose |
|---------------|------|---------|
| 0x380000-0x383FFF | 16KB | Certificate 1 |
| 0x344000-0x347FFF | 16KB | Certificate 2 |
| 0x348000-0x34BFFF | 16KB | Certificate 3 |
| ... | ... | ... |
| 0x43C000-0x43FFFF | 16KB | Certificate 63 (last) |

#### Supported Certificate Formats

**PEM Format (Recommended):**
```
-----BEGIN CERTIFICATE-----
MIIDdzCCAl+gAwIBAgIEAgAAuTANBgkqhkiG9w0BAQUFADBaMQswCQYDVQQGEwJJ...
-----END CERTIFICATE-----
```

**DER Format (Binary):**
- Binary ASN.1 encoded certificates
- Automatically detected by content analysis

#### Usage Examples

**1. Flash CA Certificate via UART**
```
AT+BNFLASH_CERT=0x380000,uart
OK
>
-----BEGIN CERTIFICATE-----
MIIDdzCCAl+gAwIBAgIEAgAAuTANBgkqhkiG9w0BAQUFADBaMQswCQYDVQQGEwJJ
RVMxEjAQBgNVBAoTCUJhbHRpbW9yZTEgMB4GA1UECxMXVGVzdCBDQSAoZXhhbXBs
ZSBvbmx5KTEVMBMGA1UEAxMMVGVzdCBDQSBSb290MB4XDTA0MDEwMTAwMDAwMFoX
DTI0MTIzMTIzNTk1OVowWjELMAkGA1UEBhMCSUUxEjAQBgNVBAoTCUJhbHRpbW9y
ZTEgMB4GA1UECxMXVGVzdCBDQSAoZXhhbXBsZSBvbmx5KTEVMBMGA1UEAxMMVGVz
dCBDQSBSb290MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA2jiCXGNV
...
-----END CERTIFICATE-----
OK
```

**2. Flash Client Certificate**
```
AT+BNFLASH_CERT=0x344000,uart
OK
>
-----BEGIN CERTIFICATE-----
MIICpjCCAY4CAQAwDQYJKoZIhvcNAQEFBQAwWjELMAkGA1UEBhMCSUUxEjAQBgNV
...
-----END CERTIFICATE-----
OK
```

### AT+BNCERT_LIST - Certificate Listing

Lists all certificates stored in the partition.

#### Command Syntax

```
AT+BNCERT_LIST?              # List all certificates
AT+BNCERT_LIST=?             # Test command
```

#### Response Format

```
+BNCERT_LIST:<index>,<address>,<size>,<type>
```

**Certificate Types:**
- `1` - CA Certificate (for server verification)
- `2` - Client Certificate (for client authentication)  
- `3` - Private Key (for client authentication)

#### Example

```
AT+BNCERT_LIST?
+BNCERT_LIST:0,0x380000,1584,1
+BNCERT_LIST:1,0x384000,1124,2
+BNCERT_LIST:2,0x388000,1679,3
OK
```

### Certificate Auto-Detection

The system automatically detects certificate types based on content:

**Detection Logic:**
1. **PEM Headers** - Analyzes BEGIN/END certificate markers
2. **ASN.1 Structure** - Parses DER binary structure  
3. **Key Usage** - Determines CA vs client certificates
4. **Content Analysis** - Validates certificate integrity

**Automatic Configuration:**
- **HTTPS Requests** - Automatically uses appropriate certificates
- **Priority Order** - Partition certificates override hardcoded bundle
- **Fallback Strategy** - Uses embedded CA bundle if no partition CA found

### Certificate Integration with HTTPS

When making HTTPS requests, certificates are automatically applied:

```
# With partition certificates configured:
AT+BNCURL=GET,"https://client-cert-required.example.com"
# Automatically uses:
# - Partition CA cert for server verification
# - Partition client cert for client authentication
# - Partition private key for client authentication
```

### Hardcoded Certificate Bundle

The system includes a hardcoded CA bundle for common websites:

**Included Root CAs:**
- DigiCert Global Root CA
- Let's Encrypt Root CA
- GlobalSign Root CA
- VeriSign Root CA
- And 20+ other major root certificates

**Fallback Strategy:**
1. Use partition CA certificates (highest priority)
2. Fall back to hardcoded bundle (if no partition CA)
3. Use permissive mode (if hardcoded fails)

---

## Web Radio Streaming (BNWEB_RADIO)

Continuous audio streaming with real-time UART output.

### AT+BNWEB_RADIO - Web Radio Control

Streams web radio or podcast audio directly to UART.

#### Command Syntax

```
AT+BNWEB_RADIO=<enable>,<url>  # Start/stop streaming
AT+BNWEB_RADIO?                # Query streaming status
AT+BNWEB_RADIO=?               # Test command
```

#### Parameters

| Parameter | Type | Range | Description |
|-----------|------|-------|-------------|
| `enable` | Integer | 0-1 | 0=stop, 1=start streaming |
| `url` | String | 1-512 chars | Radio stream URL (HTTP/HTTPS) |

#### Response Format

**Status Query:**
```
+BNWEB_RADIO:<active>,<bytes_streamed>,<duration_ms>
```

#### Usage Examples

**1. Start BBC Radio Stream**
```
AT+BNWEB_RADIO=1,"http://stream.live.vc.bbcmedia.co.uk/bbc_radio_one"
OK
# Continuous MP3 audio data flows to UART
```

**2. Check Streaming Status**
```
AT+BNWEB_RADIO?
+BNWEB_RADIO:1,1048576,30000
OK
# Active, 1MB streamed, 30 seconds duration
```

**3. Stop Streaming**
```
AT+BNWEB_RADIO=0
OK
```

**4. Secure HTTPS Stream**
```
AT+BNWEB_RADIO=1,"https://edge-bauerall-01-gos2.sharp-stream.com/net2national.mp3"
OK
# Uses certificate manager for HTTPS validation
```

### Audio Format Support

**Supported Formats:**
- **MP3** - Most common web radio format
- **AAC** - High-quality audio streams
- **OGG** - Open source audio format
- **M4A** - Apple audio format
- **Any Binary Format** - Raw pass-through streaming

**Stream Characteristics:**
- **Real-time output** - Audio data flows directly to UART as received
- **No buffering** - Minimal latency for live audio
- **No framing** - Pure binary audio data (no AT command markers)
- **Continuous flow** - Streams indefinitely until stopped

### HTTPS Integration

Web radio automatically integrates with certificate management:

**Certificate Usage:**
1. **Partition certificates** - Uses custom CA if available
2. **Hardcoded bundle** - Falls back to embedded certificates
3. **Permissive mode** - Final fallback for streaming compatibility

### Performance Characteristics

| Metric | Value | Notes |
|--------|-------|-------|
| **Buffer Size** | 8KB | Network receive buffer |
| **Task Stack** | 8KB | FreeRTOS task stack size |
| **Task Priority** | 5 | Medium priority for smooth streaming |
| **Memory Usage** | 12KB | During active streaming |
| **Max URL Length** | 512 chars | Stream URL limit |

### Streaming Protocol Support

**HTTP Features:**
- **Redirects** - Follows up to 5 redirects automatically
- **ICY Protocol** - Internet radio metadata (filtered out)
- **User Agent** - Identifies as "ESP32-WebRadio/1.0"
- **Keep-Alive** - Maintains persistent connections

**Error Recovery:**
- **Network errors** - Automatic retry with exponential backoff
- **Stream interruption** - Graceful reconnection attempts
- **Memory errors** - Automatic cleanup and resource recovery

---

## Configuration & Limits

### Global Configuration Constants

#### HTTP/HTTPS Configuration

```c
// bncurl_config.h
#define BNCURL_MAX_URL_LENGTH           2048    // Maximum URL length
#define BNCURL_MAX_HEADER_LENGTH        4096    // Maximum custom headers
#define BNCURL_MAX_POST_DATA_SIZE       1048576 // 1MB max POST data
#define BNCURL_MAX_REDIRECTS            10      // Maximum redirects
#define BNCURL_DEFAULT_TIMEOUT          30      // Default timeout (seconds)
#define BNCURL_MAX_TIMEOUT              300     // Maximum timeout (seconds)
#define BNCURL_BUFFER_SIZE              8192    // Network buffer size
#define BNCURL_MAX_VERBOSE_LINE_LENGTH  512     // Debug output line length
```

#### Certificate Management Configuration

```c
// bncert_manager.h
#define BNCERT_PARTITION_OFFSET         0x380000  // Certificate partition start
#define BNCERT_PARTITION_SIZE           0x40000   // 256KB certificate partition
#define BNCERT_MAX_CERTIFICATES         64        // Maximum stored certificates
#define BNCERT_MAX_CERT_SIZE            16384     // 16KB per certificate slot
#define BNCERT_MAGIC_HEADER             0xCERT    // Certificate validation magic
```

#### SD Card Configuration

```c
// at_sd.h
#define SD_MOUNT_POINT_MAX_LENGTH       64      // Maximum mount path length
#define SD_MAX_FILE_SIZE                UINT32_MAX  // Maximum file size
#define SD_DEFAULT_ALLOCATION_UNIT      16384   // Default cluster size
#define SD_FORMAT_TIMEOUT_MS            60000   // Format timeout (60 seconds)
```

#### WPS Configuration

```c
// bnwps.h
#define WPS_MIN_TIMEOUT                 30      // Minimum WPS timeout (seconds)
#define WPS_MAX_TIMEOUT                 300     // Maximum WPS timeout (seconds)
#define WPS_DEFAULT_TIMEOUT             120     // Default WPS timeout
#define WPS_STATUS_UPDATE_INTERVAL_MS   1000    // Status update interval
```

#### Web Radio Configuration

```c
// bnwebradio.h
#define WEBRADIO_MAX_URL_LENGTH         512     // Maximum stream URL length
#define WEBRADIO_TASK_STACK_SIZE        8192    // Task stack size
#define WEBRADIO_TASK_PRIORITY          5       // Task priority
#define WEBRADIO_BUFFER_SIZE            8192    // Streaming buffer size
#define WEBRADIO_MAX_REDIRECTS          5       // Maximum redirects
#define WEBRADIO_CONNECT_TIMEOUT        30      // Connection timeout
```

### Memory Requirements

#### Minimum System Requirements

| Component | RAM (KB) | Flash (KB) | Notes |
|-----------|----------|------------|-------|
| ESP-IDF Base | 200 | 1000 | Core system |
| AT Framework | 50 | 200 | Basic AT commands |
| BNCURL Core | 16 | 45 | HTTP functionality |
| Certificate Manager | 8 | 12 | Certificate handling |
| SD Card Support | 4 | 8 | FAT32 filesystem |
| WPS Module | 2 | 6 | WiFi setup |
| Web Radio | 12 | 15 | Audio streaming |
| **Total Minimum** | **292 KB** | **1286 KB** | **Basic functionality** |

#### Recommended System Configuration

| Component | RAM (KB) | Flash (KB) | Notes |
|-----------|----------|------------|-------|
| **Recommended** | **512 KB** | **2048 KB** | **Comfortable operation** |
| **Optimal** | **1024 KB** | **4096 KB** | **Full features + headroom** |

### Partition Table Configuration

#### Standard Partition Layout

```csv
# partitions_with_certs.csv
# Name,       Type, SubType, Offset,  Size,     Flags
nvs,          data, nvs,     0x9000,  0x6000,
phy_init,     data, phy,     0xf000,  0x1000,
factory,      app,  factory, 0x40000, 0x340000,
certificates, data, 0x99,    0x380000, 0x40000,   # 256KB for certificates
storage,      data, fat,     0x3C0000, 0x100000,  # 1MB for file storage
```

#### Certificate Partition Details

- **Type:** `data` (0x01)
- **SubType:** `0x99` (custom type for certificates)
- **Size:** 256KB (0x40000 bytes)
- **Address:** 0x380000-0x3BFFFF
- **Slots:** 64 certificate slots of 16KB each

### Network Configuration

#### WiFi Requirements

| Parameter | Requirement | Notes |
|-----------|-------------|-------|
| **WiFi Standards** | 802.11 b/g/n | 2.4GHz only |
| **Security** | WPA/WPA2/WPA3 | WEP not recommended |
| **Bandwidth** | 1 Mbps minimum | For web radio streaming |
| **Latency** | <100ms | For real-time streaming |

#### HTTPS/TLS Configuration

| Parameter | Value | Notes |
|-----------|-------|-------|
| **TLS Version** | 1.2, 1.3 | Automatic negotiation |
| **Cipher Suites** | Modern suites | ESP-IDF default selection |
| **Certificate Validation** | Full chain | With CRL checking |
| **SNI Support** | Yes | Server Name Indication |

---

## Error Handling

### Error Response Format

All commands follow consistent error reporting:

```
+ERROR:<error_code>,"<error_message>"
ERROR
```

### Common Error Codes

#### HTTP/HTTPS Errors (BNCURL)

| Code | Error | Description | Solution |
|------|-------|-------------|----------|
| `1001` | CURL_INIT_FAILED | CURL initialization failed | Restart system, check memory |
| `1002` | URL_TOO_LONG | URL exceeds maximum length | Use shorter URL (<2048 chars) |
| `1003` | INVALID_METHOD | Unsupported HTTP method | Use GET/POST/PUT/DELETE/HEAD/PATCH |
| `1004` | NETWORK_ERROR | Network connectivity issue | Check WiFi connection |
| `1005` | TIMEOUT_ERROR | Request timeout | Increase timeout or check network |
| `1006` | SSL_ERROR | HTTPS/TLS error | Check certificates, try HTTP |
| `1007` | AUTH_ERROR | Authentication failed | Check credentials |
| `1008` | REDIRECT_LIMIT | Too many redirects | Check redirect loop |
| `1009` | MEMORY_ERROR | Insufficient memory | Free memory, restart |
| `1010` | PARSE_ERROR | Response parsing failed | Check server response format |

#### SD Card Errors (BNSD)

| Code | Error | Description | Solution |
|------|-------|-------------|----------|
| `2001` | SD_NOT_PRESENT | SD card not detected | Insert SD card properly |
| `2002` | SD_MOUNT_FAILED | Mount operation failed | Check card format (FAT32) |
| `2003` | SD_READ_ERROR | Read operation failed | Check card integrity |
| `2004` | SD_WRITE_ERROR | Write operation failed | Check card write protection |
| `2005` | SD_FORMAT_ERROR | Format operation failed | Try different card |
| `2006` | SD_FULL | Insufficient space | Free space on card |
| `2007` | SD_CORRUPTED | Filesystem corrupted | Reformat card |

#### WPS Errors (BNWPS)

| Code | Error | Description | Solution |
|------|-------|-------------|----------|
| `3001` | WPS_NOT_SUPPORTED | WPS not supported by router | Use manual WiFi connection |
| `3002` | WPS_TIMEOUT | WPS connection timeout | Retry, check router WPS status |
| `3003` | WPS_FAILED | WPS authentication failed | Check router WPS configuration |
| `3004` | WPS_ALREADY_ACTIVE | WPS already in progress | Wait for completion or stop |

#### Certificate Errors (BNCERT)

| Code | Error | Description | Solution |
|------|-------|-------------|----------|
| `4001` | CERT_INVALID_FORMAT | Invalid certificate format | Use PEM or DER format |
| `4002` | CERT_TOO_LARGE | Certificate exceeds size limit | Use smaller certificate |
| `4003` | CERT_FLASH_ERROR | Flash write failed | Check flash integrity |
| `4004` | CERT_PARTITION_FULL | Certificate partition full | Remove old certificates |
| `4005` | CERT_VALIDATION_FAILED | Certificate validation failed | Check certificate integrity |

#### Web Radio Errors (BNWEB_RADIO)

| Code | Error | Description | Solution |
|------|-------|-------------|----------|
| `5001` | STREAM_URL_INVALID | Invalid stream URL | Check URL format |
| `5002` | STREAM_NOT_FOUND | Stream not available | Check stream availability |
| `5003` | STREAM_FORMAT_ERROR | Unsupported audio format | Try different stream |
| `5004` | STREAM_NETWORK_ERROR | Network error during streaming | Check network stability |
| `5005` | STREAM_MEMORY_ERROR | Insufficient memory for streaming | Free memory, restart |

### Error Recovery Strategies

#### Automatic Recovery

**Network Errors:**
- Automatic retry with exponential backoff
- Maximum 3 retry attempts
- Fallback to different DNS servers

**Memory Errors:**
- Automatic garbage collection
- Buffer size reduction
- Resource cleanup

**Certificate Errors:**
- Fallback to hardcoded certificates
- Permissive SSL mode as last resort
- Error logging for debugging

#### Manual Recovery

**System Reset:**
```
AT+RST
```

**Component Restart:**
```
# Restart specific subsystem by re-initialization
AT+BNCURL_STOP?  # Stop ongoing operations
# Then retry command
```

**Memory Cleanup:**
```
# Stop all active operations
AT+BNCURL_STOP?
AT+BNWEB_RADIO=0
# Wait for cleanup, then retry
```

---

## Performance Optimization

### Network Performance

#### HTTP/HTTPS Optimization

**Connection Reuse:**
- Keep-alive connections enabled by default
- Connection pooling for multiple requests
- DNS caching (5-minute TTL)

**Buffer Optimization:**
```c
// Adjust buffer sizes for different use cases
#define BNCURL_BUFFER_SIZE_SMALL    2048    // Small requests
#define BNCURL_BUFFER_SIZE_NORMAL   8192    // Normal requests  
#define BNCURL_BUFFER_SIZE_LARGE    16384   // Large downloads
```

**Timeout Tuning:**
```c
// Optimize timeouts for different scenarios
#define FAST_TIMEOUT      5     // Quick API calls
#define NORMAL_TIMEOUT    30    // Standard requests
#define DOWNLOAD_TIMEOUT  300   // Large downloads
#define STREAM_TIMEOUT    0     // Infinite for streaming
```

#### Web Radio Streaming Optimization

**Buffer Size Tuning:**
```c
// Adjust for different audio qualities
#define WEBRADIO_BUFFER_SIZE_LOW    4096    // Low bitrate streams
#define WEBRADIO_BUFFER_SIZE_HIGH   16384   // High bitrate streams
```

**Task Priority Optimization:**
```c
// Adjust task priorities for smooth streaming
#define WEBRADIO_TASK_PRIORITY_LOW     3    // Background streaming
#define WEBRADIO_TASK_PRIORITY_HIGH    7    // Real-time streaming
```

### Memory Optimization

#### Static vs Dynamic Allocation

**Static Allocation (Recommended for production):**
```c
// Pre-allocate buffers to avoid fragmentation
static char http_buffer[BNCURL_BUFFER_SIZE];
static char cert_buffer[BNCERT_MAX_CERT_SIZE];
```

**Dynamic Allocation (Development):**
```c
// Flexible allocation for testing
char *http_buffer = malloc(buffer_size);
// Remember to free() after use
```

#### Memory Pool Management

**Buffer Reuse:**
- Reuse buffers across requests
- Implement buffer pools for common sizes
- Avoid frequent malloc/free cycles

**Stack Optimization:**
```c
// Optimize task stack sizes
#define MINIMAL_STACK     2048   // Basic operations
#define NORMAL_STACK      4096   // Standard operations
#define LARGE_STACK       8192   // Complex operations
```

### Flash Optimization

#### Certificate Storage Optimization

**Compression:**
- Use compressed certificate storage
- Remove unnecessary certificate chains
- Store only required certificates

**Wear Leveling:**
- Rotate certificate storage locations
- Use ESP-IDF wear leveling features
- Monitor flash health

#### Code Optimization

**Conditional Compilation:**
```c
// Disable unused features to save flash
#define ENABLE_WEBRADIO      1    // Enable web radio
#define ENABLE_SD_CARD       1    // Enable SD card support
#define ENABLE_WPS           0    // Disable WPS if not needed
#define ENABLE_HTTPS         1    // Enable HTTPS support
```

---

## Integration Guide

### Adding AT Bones to Existing Project

#### 1. Component Integration

**Add to main CMakeLists.txt:**
```cmake
set(EXTRA_COMPONENT_DIRS "path/to/at_bones")
```

**Required components in main CMakeLists.txt:**
```cmake
set(COMPONENTS 
    main
    at
    at_bones
    libcurl-esp32
    fatfs
    sdmmc
    esp_wifi
)
```

#### 2. Partition Table Setup

**Copy partition table:**
```bash
cp at_bones/partitions_with_certs.csv partitions.csv
```

**Update sdkconfig:**
```
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"
CONFIG_PARTITION_TABLE_FILENAME="partitions.csv"
```

#### 3. Menuconfig Configuration

**Required settings:**
```
Component config → ESP-AT → 
  [*] AT core command support
  [*] AT user command support
  [*] AT ethernet command support
  [*] AT WiFi command support

Component config → HTTP(S) client → 
  [*] Enable HTTP client
  [*] Enable HTTPS support

Component config → FAT Filesystem support →
  [*] FAT filesystem support
  [*] Long filename support
```

#### 4. Main Application Integration

**main.c example:**
```c
#include "esp_at.h"
#include "at_bones.h"

void app_main(void)
{
    // Initialize AT framework
    esp_at_init();
    
    // Register custom commands
    esp_at_custom_cmd_register();
    
    // Start AT processing
    esp_at_start();
}
```

### Custom Command Development

#### Adding New Commands

**1. Define command structure:**
```c
static const esp_at_cmd_struct custom_commands[] = {
    {"+MYCMD", my_test_func, my_query_func, my_setup_func, my_exe_func},
};
```

**2. Implement command handlers:**
```c
static uint8_t my_test_func(uint8_t *cmd_name)
{
    esp_at_port_write_data((uint8_t *)"+MYCMD:(0,1)\r\n", 15);
    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t my_query_func(uint8_t *cmd_name)
{
    esp_at_port_write_data((uint8_t *)"+MYCMD:1\r\n", 10);
    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t my_setup_func(uint8_t *cmd_name)
{
    // Parse parameters and execute command
    return ESP_AT_RESULT_CODE_OK;
}
```

**3. Register commands:**
```c
esp_at_custom_cmd_array_regist(custom_commands, 
    sizeof(custom_commands) / sizeof(esp_at_cmd_struct));
```

#### Parameter Parsing

**Integer parameters:**
```c
int param_value;
if (!esp_at_get_para_as_digit(data_ptr, &param_value)) {
    return ESP_AT_RESULT_CODE_ERROR;
}
```

**String parameters:**
```c
char string_param[256];
int param_len = esp_at_get_para_as_str(data_ptr, string_param, sizeof(string_param));
if (param_len <= 0) {
    return ESP_AT_RESULT_CODE_ERROR;
}
```

### Advanced Integration Features

#### Custom HTTP Methods

**Add new HTTP method:**
```c
// In bncurl_methods.h
typedef enum {
    BNCURL_METHOD_GET,
    BNCURL_METHOD_POST,
    BNCURL_METHOD_PUT,
    BNCURL_METHOD_DELETE,
    BNCURL_METHOD_HEAD,
    BNCURL_METHOD_PATCH,
    BNCURL_METHOD_OPTIONS,    // New method
    BNCURL_METHOD_TRACE,      // New method
} bncurl_method_t;
```

#### Custom Certificate Validation

**Override certificate validation:**
```c
// In bncert_manager.c
bool custom_cert_validator(const uint8_t *cert_data, size_t cert_size)
{
    // Custom validation logic
    return validate_custom_certificate(cert_data, cert_size);
}
```

#### Custom Audio Processing

**Add audio processing to web radio:**
```c
// In bnwebradio.c - modify webradio_write_callback
static size_t webradio_write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t total_size = size * nmemb;
    
    // Add custom audio processing here
    process_audio_data(contents, total_size);
    
    // Stream processed data to UART
    esp_at_port_write_data((uint8_t *)contents, total_size);
    
    return total_size;
}
```

---

## Troubleshooting

### Common Issues and Solutions

#### Build Issues

**Issue: "component 'at_bones' not found"**
```bash
# Solution: Check component path in CMakeLists.txt
set(EXTRA_COMPONENT_DIRS "components/at_bones")
```

**Issue: "undefined reference to curl functions"**
```bash
# Solution: Add libcurl-esp32 to dependencies
set(COMPONENTS ... libcurl-esp32)
```

**Issue: "partition table too large"**
```bash
# Solution: Increase partition table size in sdkconfig
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"
```

#### Runtime Issues

**Issue: AT commands not responding**
```bash
# Debug: Check UART configuration
CONFIG_AT_UART_PORT=1
CONFIG_AT_UART_BAUD_RATE=115200

# Test basic AT command
AT
OK
```

**Issue: HTTP requests failing**
```bash
# Debug: Check WiFi connection
AT+CWJAP?
+CWJAP:"network_name","aa:bb:cc:dd:ee:ff",6,-45
OK

# Test simple HTTP request
AT+BNCURL=GET,"http://httpbin.org/get"
```

**Issue: HTTPS certificate errors**
```bash
# Debug: List certificates
AT+BNCERT_LIST?
+BNCERT_LIST:0,0x380000,1584,1
OK

# Test with HTTP first
AT+BNCURL=GET,"http://httpbin.org/get"
```

**Issue: SD card not mounting**
```bash
# Debug: Check SD card detection
AT+BNSD_MOUNT="/sdcard"
+ERROR:2001,"SD card not detected"

# Solution: Check hardware connections and card format
```

**Issue: Web radio not streaming**
```bash
# Debug: Test with simple HTTP stream
AT+BNWEB_RADIO=1,"http://stream.example.com/test.mp3"

# Check stream status
AT+BNWEB_RADIO?
+BNWEB_RADIO:1,1024,5000
OK
```

### Debug Mode Configuration

#### Enable Debug Logging

**In sdkconfig:**
```
CONFIG_LOG_DEFAULT_LEVEL_DEBUG=y
CONFIG_LOG_MAXIMUM_LEVEL=4
```

**Component-specific logging:**
```c
// Set log levels for specific components
esp_log_level_set("BNCURL", ESP_LOG_DEBUG);
esp_log_level_set("BNCERT", ESP_LOG_DEBUG);
esp_log_level_set("BNWPS", ESP_LOG_DEBUG);
esp_log_level_set("BNWEBRADIO", ESP_LOG_DEBUG);
```

#### Monitor Output

**Serial monitor:**
```bash
# Use ESP-IDF monitor for debugging
idf.py monitor

# Filter for specific component
idf.py monitor | grep "BNCURL"
```

**Log analysis:**
```bash
# Save logs to file for analysis
idf.py monitor > debug.log 2>&1

# Search for errors
grep "ERROR" debug.log
grep "WARN" debug.log
```

### Performance Monitoring

#### Memory Usage Monitoring

**Add to main application:**
```c
void print_memory_stats(void)
{
    ESP_LOGI("MEMORY", "Free heap: %d bytes", esp_get_free_heap_size());
    ESP_LOGI("MEMORY", "Minimum free heap: %d bytes", esp_get_minimum_free_heap_size());
    
    // Print task stack usage
    TaskStatus_t task_status;
    vTaskGetInfo(NULL, &task_status, pdTRUE, eInvalid);
    ESP_LOGI("MEMORY", "Task stack high water mark: %d", task_status.usStackHighWaterMark);
}
```

#### Network Performance Monitoring

**Add to HTTP operations:**
```c
void monitor_http_performance(void)
{
    uint32_t start_time = esp_timer_get_time();
    
    // Perform HTTP operation
    bool result = bncurl_execute_request(...);
    
    uint32_t end_time = esp_timer_get_time();
    uint32_t duration_ms = (end_time - start_time) / 1000;
    
    ESP_LOGI("PERF", "HTTP request completed in %d ms", duration_ms);
}
```

### Factory Reset and Recovery

#### Software Reset

**Reset to factory defaults:**
```bash
# Erase entire flash
esptool.py --port COM3 erase_flash

# Flash factory firmware
idf.py flash
```

#### Selective Reset

**Reset specific components:**
```bash
# Clear certificate partition
esptool.py --port COM3 erase_region 0x380000 0x40000

# Clear NVS (WiFi settings)
esptool.py --port COM3 erase_region 0x9000 0x6000
```

#### Recovery Mode

**Enter recovery mode:**
```c
// Add to main application for emergency recovery
void enter_recovery_mode(void)
{
    ESP_LOGW("RECOVERY", "Entering recovery mode");
    
    // Reset all subsystems
    bnwebradio_stop();
    bncurl_stop_all_operations();
    at_sd_unmount();
    
    // Clear runtime state
    nvs_flash_erase();
    esp_restart();
}
```

---

## Conclusion

The AT Bones framework provides a comprehensive solution for ESP32 HTTP/HTTPS operations, certificate management, storage handling, and audio streaming. With its modular architecture and extensive configuration options, it can be adapted for a wide range of IoT applications.

### Key Benefits

- **Comprehensive HTTP/HTTPS support** with certificate management
- **Flexible certificate system** with automatic type detection
- **Robust error handling** and recovery mechanisms
- **Performance optimized** for ESP32 constraints
- **Extensive documentation** for easy integration and modification

### Future Development

The framework is designed for extensibility. Future enhancements may include:

- **WebSocket support** for real-time communication
- **MQTT integration** for IoT messaging
- **Advanced audio processing** for web radio streams
- **Database connectivity** for data persistence
- **Cloud service integrations** for popular IoT platforms

For support and contributions, refer to the individual component documentation and the project repository.

---

**Last Updated:** August 31, 2025  
**Version:** 2.0  
**Author:** AT Bones Development Team  
**License:** Apache 2.0
