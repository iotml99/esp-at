# AT Bones - User Guide

**Version:** 2.1  
**Date:** September 1, 2025  
**Quick Start Guide for ESP32 AT Bones Framework**

## 🚀 Quick Start

AT Bones extends ESP32 AT commands with HTTP/HTTPS client, SD card management, WiFi setup, certificate handling, and web radio streaming capabilities.

### Supported Commands

| Command | Purpose | Example |
|---------|---------|---------|
| `AT+BNCURL` | HTTP/HTTPS requests | `AT+BNCURL="GET","https://httpbin.org/get"` |
| `AT+BNURLCFG` | Configure URL for BNCURL | `AT+BNURLCFG="https://very-long-url.com/path"` |
| `AT+BNSD_MOUNT` | Mount SD card | `AT+BNSD_MOUNT="/sdcard"` |
| `AT+BNWPS` | WiFi WPS setup | `AT+BNWPS=120` |
| `AT+BNFLASH_CERT` | Flash certificates | `AT+BNFLASH_CERT=0x380000,1024` |
| `AT+BNWEB_RADIO` | Stream web radio | `AT+BNWEB_RADIO=1,"http://stream.mp3"` |

---

## 📡 HTTP/HTTPS Client (BNCURL)

### URL Configuration

**Configure URL for Long Commands:**
```
AT+BNURLCFG="https://very-long-domain-name.example.com/api/v2/endpoints/data/download"
OK

# Now use "." to reference the configured URL
AT+BNCURL="GET","."
OK
```

**Query Configured URL:**
```
AT+BNURLCFG?
+BNURLCFG:"https://very-long-domain-name.example.com/api/v2/endpoints/data/download"
OK
```

**Clear Configured URL:**
```
AT+BNURLCFG=""
OK

AT+BNURLCFG?
+BNURLCFG:<not set>
OK
```

### Basic HTTP Requests

**Simple GET Request:**
```
AT+BNCURL="GET","https://httpbin.org/get"
OK
```

**GET with Custom Headers:**
```
AT+BNCURL="GET","https://httpbin.org/headers","-H","User-Agent: MyApp/1.0","-H","X-API-Key: 12345"
OK
```

**POST with UART Data:**
```
AT+BNCURL="POST","https://httpbin.org/post","-du","25"
OK
>{"message":"hello world"}
```

**POST from SD Card File:**
```
AT+BNCURL="POST","https://httpbin.org/post","-du","@/data.json"
OK
```

**Using Configured URL:**
```
AT+BNURLCFG="https://api.example.com/v2/upload/endpoint"
OK

AT+BNCURL="POST",".","-du","25"
OK
>{"message":"hello world"}
```

### File Downloads

**Download to SD Card:**
```
AT+BNCURL="GET","https://example.com/file.pdf","-dd","@/downloads/file.pdf"
OK
```

**Range Download (partial content):**
```
AT+BNCURL="GET","https://example.com/large-file.zip","-r","0-1048575","-dd","@/downloads/part1.zip"
OK
```

**Range Download with Configured URL:**
```
AT+BNURLCFG="https://cdn.example.com/releases/firmware-v2.1.0.bin"
OK

AT+BNCURL="GET",".","-r","0-1048575","-dd","@/downloads/firmware_part1.bin"
OK
```

**Stream to UART (no file):**
```
AT+BNCURL="GET","https://example.com/data.txt","-r","0-1023"
OK
# Data streams directly to UART
```

### Monitoring and Control

**Check Status:**
```
AT+BNCURL?
+BNCURL:IDLE
OK
```

**Monitor Progress:**
```
AT+BNCURL_PROG?
+BNCURL_PROG:524288/1048576
OK
```

**Stop Operation:**
```
AT+BNCURL_STOP?
+BNCURL_STOP:1
OK
```

**Set Timeout:**
```
AT+BNCURL_TIMEOUT=60
OK

AT+BNCURL_TIMEOUT?
+BNCURL_TIMEOUT:60
OK
```

### Options Reference

| Option | Description | Example |
|--------|-------------|---------|
| `"."` | Use URL from AT+BNURLCFG | `AT+BNCURL="GET","."` |
| `-H "Header: Value"` | Custom HTTP header | `-H "Authorization: Bearer token123"` |
| `-du <bytes>` | Upload data via UART | `-du 100` (prompts for 100 bytes) |
| `-du @file` | Upload from SD card file | `-du @/uploads/data.json` |
| `-dd @file` | Download to SD card file | `-dd @/downloads/file.pdf` |
| `-r "start-end"` | Range request | `-r "0-1023"` (first 1KB) |
| `-v` | Verbose output | `-v` |

---

## 💾 SD Card Management (BNSD)

### Mount and Unmount

**Mount SD Card:**
```
AT+BNSD_MOUNT="/sdcard"
OK

# Query mount status
AT+BNSD_MOUNT?
+BNSD_MOUNT:1,"/sdcard"
OK
```

**Unmount SD Card:**
```
AT+BNSD_UNMOUNT
OK

AT+BNSD_UNMOUNT?
+BNSD_UNMOUNT:0
OK
```

### Disk Space

**Check Available Space:**
```
AT+BNSD_SPACE?
+BNSD_SPACE:8056832000/822264110
OK
# Format: total_bytes/used_bytes
```

### Format SD Card

**Format with FAT32:**
```
AT+BNSD_FORMAT?
+BNSD_FORMAT:READY
OK

AT+BNSD_FORMAT
OK
# WARNING: This erases all data!
```

---

## 📶 WiFi WPS Setup (BNWPS)

### Basic WPS Connection

**Start WPS for 2 minutes:**
```
AT+BNWPS=120
OK
# Press WPS button on router now
# On success:
+CWJAP:"MyNetwork","aa:bb:cc:dd:ee:ff",6,-45,1,0,0,0,0
OK
```

**Check WPS Status:**
```
AT+BNWPS?
+BNWPS:1
OK
# 1 = active, 0 = inactive
```

**Cancel WPS:**
```
AT+BNWPS=0
+BNWPS:0
OK
```

### Expected Responses

**Successful Connection:**
```
AT+BNWPS=60
OK
# After pressing router WPS button:
+CWJAP:"HomeWiFi","12:34:56:78:9a:bc",6,-42,1,0,0,0,0
OK
```

**Timeout:**
```
AT+BNWPS=30
OK
# After 30 seconds with no connection:
+CWJAP:1
ERROR
```

---

## 🔐 Certificate Management (BNCERT)

### Flash Certificates

**Flash Certificate via UART:**
```
AT+BNFLASH_CERT=0x380000,1024
OK
>-----BEGIN CERTIFICATE-----
MIIDdzCCAl+gAwIBAgIEAgAAuTANBgkqhkiG9w0BAQUFADBa...
...certificate content...
-----END CERTIFICATE-----
+BNFLASH_CERT:OK,0x380000,1024
OK
```

**Flash from SD Card File:**
```
AT+BNFLASH_CERT=0x384000,"@/certs/ca-bundle.pem"
+BNFLASH_CERT:OK,0x384000,2048
OK
```

### Certificate Management

**List All Certificates:**
```
AT+BNCERT_LIST?
+BNCERT_LIST:0,0x380000,1584,1
+BNCERT_LIST:1,0x384000,1124,2
+BNCERT_LIST:2,0x388000,1679,3
OK
# Format: index,address,size,type
# Types: 1=CA, 2=Client, 3=Private Key
```

**List Valid Addresses:**
```
AT+BNCERT_ADDR?
# Shows all available certificate storage addresses
```

**Clear Certificate:**
```
AT+BNCERT_CLEAR=0x380000
+BNCERT_CLEAR:OK,0x380000
OK
```

### Common Certificate Addresses

| Address | Purpose | Size |
|---------|---------|------|
| 0x380000 | CA Certificate 1 | 16KB |
| 0x384000 | CA Certificate 2 | 16KB |
| 0x388000 | Client Certificate | 16KB |
| 0x38C000 | Private Key | 16KB |

---

## 📻 Web Radio Streaming (BNWEB_RADIO)

### Basic Streaming

**Start Radio Stream:**
```
AT+BNWEB_RADIO=1,"http://stream.live.vc.bbcmedia.co.uk/bbc_radio_one"
OK
# Audio data now streams continuously to UART
```

**Check Stream Status:**
```
AT+BNWEB_RADIO?
+BNWEB_RADIO:1,1048576,30000
OK
# Format: active,bytes_streamed,duration_ms
```

**Stop Streaming:**
```
AT+BNWEB_RADIO=0
OK
```

### HTTPS Radio Streams

**Secure Stream with Certificates:**
```
AT+BNWEB_RADIO=1,"https://edge-bauerall-01-gos2.sharp-stream.com/net2national.mp3"
OK
# Uses flashed certificates automatically
```

### Popular Radio Stream Examples

```
# BBC Radio 1
AT+BNWEB_RADIO=1,"http://stream.live.vc.bbcmedia.co.uk/bbc_radio_one"

# Example MP3 Stream
AT+BNWEB_RADIO=1,"http://ice1.somafm.com/groovesalad-256-mp3"

# HTTPS Stream
AT+BNWEB_RADIO=1,"https://playerservices.streamtheworld.com/api/livestream"
```

---

## 🔧 Common Usage Patterns

### Long URL Workaround

```bash
# 1. Configure very long URL
AT+BNURLCFG="https://very-long-domain-name.example.com/api/v2/endpoints/data/download/files/large-dataset.json"
OK

# 2. Use "." to reference configured URL
AT+BNCURL="GET",".","-dd","@/data/dataset.json"
OK

# 3. Reuse same URL with different options
AT+BNCURL="GET",".","-r","0-1048575","-dd","@/data/dataset_part1.json"
OK
```

### Setup WiFi with WPS

```bash
# 1. Start WPS
AT+BNWPS=120
OK

# 2. Press WPS button on router
# Wait for connection...

# 3. Verify connection
+CWJAP:"MyNetwork","aa:bb:cc:dd:ee:ff",6,-45,1,0,0,0,0
OK
```

### Setup SD Card and Download File

```bash
# 1. Mount SD card
AT+BNSD_MOUNT="/sdcard"
OK

# 2. Check space
AT+BNSD_SPACE?
+BNSD_SPACE:8056832000/822264110
OK

# 3. Download file
AT+BNCURL="GET","https://example.com/file.pdf","-dd","@/downloads/file.pdf"
OK
```

### Upload Data with Custom Headers

```bash
# 1. Configure API endpoint
AT+BNURLCFG="https://api.example.com/v1/sensors/temperature/readings"
OK

# 2. Prepare data via UART with headers
AT+BNCURL="POST",".","-H","Content-Type: application/json","-H","Authorization: Bearer xyz123","-du","45"
OK
>{"sensor":"temperature","value":23.5,"unit":"C"}

# Data is uploaded automatically
```

### Stream Large File in Chunks

```bash
# 1. Configure large file URL
AT+BNURLCFG="https://releases.example.com/firmware/esp32-firmware-v2.1.0.bin"
OK

# 2. Download first 2MB
AT+BNCURL="GET",".","-r","0-2097151","-dd","@/firmware/part1.bin"
OK

# 3. Download next 2MB  
AT+BNCURL="GET",".","-r","2097152-4194303","-dd","@/firmware/part2.bin"
OK
```

---

## ⚠️ Important Notes

### File Path Requirements

**Always use `@` prefix for SD card files:**
```bash
# ✅ Correct
AT+BNCURL="GET","https://example.com/file.pdf","-dd","@/downloads/file.pdf"

# ❌ Wrong
AT+BNCURL="GET","https://example.com/file.pdf","-dd","/downloads/file.pdf"
```

### Supported HTTP Methods

- ✅ **GET** - Retrieve data
- ✅ **POST** - Send data  
- ✅ **HEAD** - Get headers only
- ❌ PUT, DELETE, PATCH - Not supported

### URL Configuration

**Use AT+BNURLCFG for long URLs:**
```bash
# ✅ Correct - for very long URLs
AT+BNURLCFG="https://very-long-domain-name.example.com/api/v2/endpoints/data"
AT+BNCURL="GET","."

# ✅ Also correct - direct URL
AT+BNCURL="GET","https://short-url.com/api"

# ❌ Wrong - URL not configured when using "."
AT+BNCURL="GET","."  # Error if no URL configured
```

### UART Data Input

When using `-du <bytes>`, the system prompts with `>` and waits for exact byte count:

```bash
AT+BNCURL="POST","https://httpbin.org/post","-du","13"
OK
>Hello, World!
# Exactly 13 bytes required
```

### Certificate Integration

HTTPS requests automatically use flashed certificates:

1. **Partition certificates** (highest priority)
2. **Hardcoded CA bundle** (fallback)
3. **Permissive mode** (last resort)

---

## 📞 Quick Reference

### Command Summary

| Command | Quick Example |
|---------|---------------|
| GET request | `AT+BNCURL="GET","https://httpbin.org/get"` |
| Configure URL | `AT+BNURLCFG="https://very-long-url.example.com/path"` |
| Use configured URL | `AT+BNCURL="GET","."` |
| POST data | `AT+BNCURL="POST","https://httpbin.org/post","-du","20"` |
| Download file | `AT+BNCURL="GET","https://example.com/file.pdf","-dd","@/file.pdf"` |
| Mount SD | `AT+BNSD_MOUNT="/sdcard"` |
| WPS connect | `AT+BNWPS=120` |
| Flash cert | `AT+BNFLASH_CERT=0x380000,1024` |
| Stream radio | `AT+BNWEB_RADIO=1,"http://stream.mp3"` |
| Check status | `AT+BNCURL?` |
| Stop operation | `AT+BNCURL_STOP?` |

### Status Codes

| Component | Active | Inactive |
|-----------|--------|----------|
| BNCURL | `EXECUTING`/`QUEUED` | `IDLE` |
| SD Card | `1` (mounted) | `0` (unmounted) |
| WPS | `1` (active) | `0` (inactive) |
| Web Radio | `1` (streaming) | `0` (stopped) |

---

**License:** Apache 2.0  
**Support:** Check implementation files for advanced configuration
