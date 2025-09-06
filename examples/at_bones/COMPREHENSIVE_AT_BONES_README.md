# ESP32 AT Bones - Comprehensive Developer Guide

**Version:** 2.1  
**Date:** September 1, 2025  
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

The AT Bones framework is a comprehensive ESP32 AT command extension that provides HTTP/HTTPS capabilities, SD card management, WiFi setup automation, certificate handling, and web radio streaming. Built on top of ESP-AT, it extends the standard AT command set with networking and storage features.

### Key Features

- **HTTP/HTTPS Client** - Supports GET, POST, and HEAD methods with options-based syntax
- **Certificate Management** - Dynamic certificate flashing and automatic integration  
- **SD Card Operations** - Mount, format, space management with FAT32 support
- **WiFi Protected Setup** - Automated WPS connection with timeout controls
- **Web Radio Streaming** - Continuous audio streaming with real-time UART output
- **Range Downloads** - Efficient partial content downloads with UART streaming
- **Cookie Support** - Automatic cookie handling for session management
- **HTTPS Integration** - Seamless certificate integration with fallback strategies

### Prerequisites

- **Hardware:** ESP32 with minimum 4MB flash
- **Framework:** ESP-IDF 5.0+
- **Components:** libcurl-esp32, fatfs, sdmmc driver
- **Memory:** Minimum 512KB RAM for simultaneous operations
- **Storage:** SD card (optional, for SD operations)

---

## HTTP/HTTPS Commands (BNCURL)

### AT+BNCURL - HTTP Request Execution

The main HTTP/HTTPS client command with support for GET, POST, and HEAD methods using option-based syntax.

#### Command Syntax

```
AT+BNCURL=<method>,<url>[,<options>]
```

#### Parameters

| Parameter | Type | Range | Description |
|-----------|------|-------|-------------|
| `method` | String | GET/POST/HEAD | HTTP method (only these 3 supported) |
| `url` | String | 1-1024 chars | Target URL (HTTP/HTTPS) |
| `options` | String | Various flags | Optional parameters (see below) |

#### Supported Options

| Option | Syntax | Description |
|--------|--------|-------------|
| `-H` | `-H "Header: Value"` | Custom HTTP header |
| `-du` | `-du <bytes>` or `-du @file` | Upload data (POST only) |
| `-dd` | `-dd @file` | Download to SD card file |
| `-c` | `-c @file` | Save cookies to SD card file |
| `-b` | `-b @file` | Send cookies from SD card file |
| `-r` | `-r "start-end"` | Range request (GET only, optional with -dd) |
| `-v` | `-v` | Verbose debug output |

#### Response Format

**Success Response:**
```
OK
```

**Error Response:**
```
ERROR
```

**Note:** Actual HTTP response data and progress are handled asynchronously through the executor system. Use `AT+BNCURL_PROG?` to monitor progress and `AT+BNCURL?` to check status.

#### Usage Examples

**1. Simple GET Request**
```
AT+BNCURL="GET","https://httpbin.org/get"
OK
```

**2. POST with UART Data Input**
```
AT+BNCURL="POST","https://httpbin.org/post","-du","25"
OK
>{"key":"value","test":123}
```

**3. Range Download to SD Card**
```
AT+BNCURL="GET","https://example.com/file.mp3","-dd","@file.mp3","-r","0-2097151"
OK
```

**4. Range Download to UART (streaming)**
```
AT+BNCURL="GET","https://example.com/file.mp3","-r","0-2097151"
OK
```

**5. Custom Headers**
```
AT+BNCURL="GET","https://httpbin.org/headers","-H","User-Agent: MyApp/1.0","-H","X-Custom-Header: test"
OK
```

**6. Upload from SD Card File**
```
AT+BNCURL="POST","https://httpbin.org/post","-du","@data.json"
OK
```

### AT+BNCURL? - Query BNCURL Status

Queries the current status of the BNCURL executor.

#### Command Syntax

```
AT+BNCURL?                   # Query executor status
```

#### Response Format

```
+BNCURL:<status>
```

Where `<status>` can be:
- `IDLE` - No operations in progress
- `QUEUED` - Operation is queued for execution  
- `EXECUTING` - Operation is currently executing
- `UNKNOWN` - Unknown status

#### Example

```
AT+BNCURL?
+BNCURL:IDLE
OK
```

### AT+BNCURL_TIMEOUT - Server Response Timeout Configuration

Controls the server response timeout for HTTP operations.

#### Command Syntax

```
AT+BNCURL_TIMEOUT=<timeout>  # Set server response timeout
AT+BNCURL_TIMEOUT?           # Query current timeout
AT+BNCURL_TIMEOUT=?          # Test command (shows range)
```

#### Parameters

| Parameter | Type | Range | Default | Description |
|-----------|------|-------|---------|-------------|
| `timeout` | Integer | See config | 30 | Server response timeout in seconds |

#### Examples

```
AT+BNCURL_TIMEOUT=60
OK

AT+BNCURL_TIMEOUT?
+BNCURL_TIMEOUT:60
OK

AT+BNCURL_TIMEOUT=?
AT+BNCURL_TIMEOUT=<timeout>
Set timeout for server reaction in seconds.
Range: <min>-<max> seconds
OK
```

### AT+BNCURL_STOP - Stop Active Request

Cancels any ongoing HTTP operation.

#### Command Syntax

```
AT+BNCURL_STOP?              # Stop current operation
```

#### Response Format

```
+BNCURL_STOP:<result>
```

Where `<result>` is:
- `1` - Operation stopped successfully
- `0` - No operation to stop or stop failed

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
+BNCURL_PROG:<bytes_transferred>/<total_bytes>
```

#### Example

```
AT+BNCURL_PROG?
+BNCURL_PROG:524288/1048576
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

Mounts SD card with specified mount point.

#### Command Syntax

```
AT+BNSD_MOUNT=<mount_point>  # Mount at specific path
AT+BNSD_MOUNT                # Mount at default path (/sdcard)
AT+BNSD_MOUNT?               # Query mount status  
AT+BNSD_MOUNT=?              # Test command
```

#### Parameters

| Parameter | Type | Range | Description |
|-----------|------|-------|-------------|
| `mount_point` | String | 1-64 chars | Mount point path (optional) |

#### Response Format

**Query Response:**
```
+BNSD_MOUNT:<status>[,"<mount_point>"]
```

Where:
- `<status>`: `1` if mounted, `0` if not mounted
- `<mount_point>`: Mount path (when mounted)

#### Examples

```
AT+BNSD_MOUNT="/sdcard"
OK

AT+BNSD_MOUNT
OK

AT+BNSD_MOUNT?
+BNSD_MOUNT:1,"/sdcard"
OK
```

### AT+BNSD_UNMOUNT - Unmount SD Card

Safely unmounts the SD card filesystem.

#### Command Syntax

```
AT+BNSD_UNMOUNT              # Unmount current SD card
AT+BNSD_UNMOUNT?             # Query unmount status  
AT+BNSD_UNMOUNT=?            # Test command
```

#### Response Format

**Query Response:**
```
+BNSD_UNMOUNT:<status>
```

Where `<status>` represents the SD card status.

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
AT+BNSD_SPACE=?              # Test command
```

#### Response Format

```
+BNSD_SPACE:<total_bytes>/<used_bytes>
```

Or on error:
```
+BNSD_SPACE:ERROR
```

#### Example

```
AT+BNSD_SPACE?
+BNSD_SPACE:8056832000/822264110
OK
```

### AT+BNSD_FORMAT - Format SD Card

Formats SD card with FAT32 filesystem.

#### Command Syntax

```
AT+BNSD_FORMAT               # Format with default settings
AT+BNSD_FORMAT?              # Query format capability
AT+BNSD_FORMAT=?             # Test command
```

#### Response Format

**Query Response:**
```
+BNSD_FORMAT:<status>
```

Where `<status>` can be:
- `READY` - SD card is mounted and ready for formatting
- `NO_CARD` - No SD card detected

#### Example

```
AT+BNSD_FORMAT?
+BNSD_FORMAT:READY
OK

AT+BNSD_FORMAT
OK
```

### SD Card Configuration

FAT32 formatted

#### Hardware Requirements
- **SPI Interface** - 4-wire SPI connection to ESP32
- **Card Types** - SD, SDHC, SDXC (up to 2TB)
- **Speed Classes** - Class 4+ recommended for streaming
- **Voltage** - 3.3V operation

#### Pin Configuration (Default SPI)
#define PIN_NUM_CS    18
#define PIN_NUM_MOSI  19
#define PIN_NUM_CLK   21
#define PIN_NUM_MISO  20
---

## WiFi Protected Setup (BNWPS)

Automated WiFi connection using WPS (WiFi Protected Setup).

### AT+BNWPS - WPS Control

Initiates or queries WPS connection process.

#### Command Syntax

```
AT+BNWPS=<timeout>           # Start WPS for timeout seconds (1-300) or cancel (0)
AT+BNWPS?                    # Query current WPS state
AT+BNWPS=?                   # Test command (show help)
```

#### Parameters

| Parameter | Type | Range | Description |
|-----------|------|-------|-------------|
| `timeout` | Integer | 0, 1-300 | WPS timeout in seconds (0=cancel) |

#### Response Format

**Status Query:**
```
+BNWPS:<state>
```

Where `<state>` is:
- `0` - WPS inactive
- `1` - WPS active

**On successful WPS connection, the system automatically executes:**
```
+CWJAP:"<ssid>","<bssid>",<channel>,<rssi>,<pci_en>,<reconn_interval>,<listen_interval>,<scan_mode>,<pmf>
OK
```

**On WPS error:**
```
+CWJAP:<error_code>
ERROR
```

#### Examples

**1. Query WPS Status**
```
AT+BNWPS?
+BNWPS:0
OK
```

**2. Start WPS for 120 seconds**
```
AT+BNWPS=120
OK
# User presses WPS button on router
# On success:
+CWJAP:"MyNetwork","aa:bb:cc:dd:ee:ff",6,-45,1,0,0,0,0
OK
```

**3. Cancel WPS Operation**
```
AT+BNWPS=0
+BNWPS:0
OK
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
AT+BNFLASH_CERT=?              # Test command (show help)
```

#### Parameters

| Parameter | Type | Range | Description |
|-----------|------|-------|-------------|
| `address` | Hex | Certificate partition range | Flash address in certificate partition |
| `data_source` | String/Integer | @file or byte_count | Data source: SD card file or UART byte count |

#### Usage Examples

**1. Flash CA Certificate via UART**
```
AT+BNFLASH_CERT=0x380000,1400
OK
>-----BEGIN CERTIFICATE-----
MIIDdzCCAl+gAwIBAgIEAgAAuTANBgkqhkiG9w0BAQUFADBaMQswCQYDVQQGEwJJ
...certificate data...
-----END CERTIFICATE-----
+BNFLASH_CERT:OK,0x380000,1400
OK
```

**2. Flash Certificate from SD Card File**
```
AT+BNFLASH_CERT=0x384000,"@/certs/server_key.bin"
+BNFLASH_CERT:OK,0x384000,<file_size>
OK
```

### AT+BNCERT_FLASH - Certificate Flash (Alias)

This is an alias for `AT+BNFLASH_CERT` with identical functionality.

#### Command Syntax

```
AT+BNCERT_FLASH=<address>,<data_source>
AT+BNCERT_FLASH=?              # Test command (show help)
```

### AT+BNCERT_LIST - Certificate Listing

Lists all certificates stored in the partition.

#### Command Syntax

```
AT+BNCERT_LIST?                # List all certificates
AT+BNCERT_LIST=?               # Test command
```

#### Response Format

```
+BNCERT_LIST:<index>,<address>,<size>,<type>
```

Where:
- `<index>`: Certificate index number
- `<address>`: Flash address (hex)
- `<size>`: Certificate size in bytes
- `<type>`: Certificate type (1=CA, 2=Client, 3=Private Key)

#### Example

```
AT+BNCERT_LIST?
+BNCERT_LIST:0,0x380000,1584,1
+BNCERT_LIST:1,0x384000,1124,2
+BNCERT_LIST:2,0x388000,1679,3
OK
```

### AT+BNCERT_ADDR - Certificate Addresses

Lists all valid certificate storage addresses.

#### Command Syntax

```
AT+BNCERT_ADDR?                # List valid addresses
AT+BNCERT_ADDR=?               # Test command
```

### AT+BNCERT_CLEAR - Certificate Clear

Clears (erases) certificate at specific address.

#### Command Syntax

```
AT+BNCERT_CLEAR=<address>      # Clear certificate at address
AT+BNCERT_CLEAR=?              # Test command
```

#### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `address` | Hex | 4KB-aligned address in certificate partition |

#### Response Format

```
+BNCERT_CLEAR:<result>,<address>
```

Where `<result>` is:
- `OK` - Certificate cleared successfully
- `ERROR` - Clear operation failed

#### Example

```
AT+BNCERT_CLEAR=0x380000
+BNCERT_CLEAR:OK,0x380000
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
AT+BNWEB_RADIO=<enable>,<url>  # Start (1) or stop (0) streaming
AT+BNWEB_RADIO?                # Query streaming status
AT+BNWEB_RADIO=?               # Test command
```

#### Parameters

| Parameter | Type | Range | Description |
|-----------|------|-------|-------------|
| `enable` | Integer | 0-1 | 0=stop, 1=start streaming |
| `url` | String | 1-512 chars | Radio stream URL (when enable=1) |

#### Response Format

**Test Command:**
```
+BNWEB_RADIO:(0,1)
```

**Status Query:**
```
+BNWEB_RADIO:<active>[,<bytes_streamed>,<duration_ms>]
```

Where:
- `<active>`: `1` if streaming, `0` if stopped
- `<bytes_streamed>`: Total bytes streamed (when active)
- `<duration_ms>`: Streaming duration in milliseconds (when active)

#### Usage Examples

**1. Start BBC Radio Stream**
```
AT+BNWEB_RADIO=1,"http://stream.live.vc.bbcmedia.co.uk/bbc_radio_one"
OK
# Continuous audio data flows to UART
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
// bncurl_config.h - Key configuration constants
#define BNCURL_MAX_URL_LENGTH           1024    // Maximum URL length
#define BNCURL_MAX_HEADERS_COUNT        16      // Maximum custom headers
#define BNCURL_MAX_HEADER_LENGTH        256     // Maximum single header length
#define BNCURL_MAX_UART_UPLOAD_SIZE     (512 * 1024)  // 512KB max UART upload
#define BNCURL_MAX_FILE_UPLOAD_SIZE     (16 * 1024 * 1024)  // 16MB max file upload
#define BNCURL_DEFAULT_TIMEOUT          30      // Default timeout (seconds)
#define BNCURL_MIN_TIMEOUT              1       // Minimum timeout
#define BNCURL_MAX_TIMEOUT              120     // Maximum timeout
#define BNCURL_SUPPORTED_METHODS_COUNT  3       // GET, POST, HEAD only
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
| `1003` | INVALID_METHOD | Unsupported HTTP method | Use GET/POST/HEAD only |
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

