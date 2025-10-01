# BNCERT Certificate Bundle Commands

## Overview

The BNCERT commands provide a simplified certificate bundle management system for ESP-AT firmware. This system replaces the older individual certificate management with a streamlined approach using a single 256KB partition for certificate bundle storage.

## Key Features

- **Single Partition Design**: Uses one 256KB partition (subtype 0x40) for certificate storage
- **Memory Efficient**: Direct flash access with zero RAM usage during TLS operations
- **PEM Format Support**: Handles standard PEM certificate bundles with automatic validation
- **Streaming Operations**: Supports large bundles with 1KB chunk processing
- **Automatic Fallback**: Falls back to hardcoded certificates when flash bundle unavailable
- **Memory-Aware**: Automatically handles ESP32 memory constraints for large bundles

## Commands

### AT+BNCERT_FLASH - Flash Certificate Bundle

Flash a certificate bundle to the dedicated partition.

**Syntax:**
```
AT+BNCERT_FLASH=<source>,<parameter>
```

**Parameters:**
- `source`: Data source type
  - `0` = SD card file
  - `1` = UART input
- `parameter`: 
  - For SD card (`source=0`): File path (supports @ prefix for mount point)
  - For UART (`source=1`): Bundle size in bytes

**Examples:**
```bash
# Flash from SD card (with @ prefix auto-conversion)
AT+BNCERT_FLASH=0,"@/certs/ca_bundle.pem"

# Flash from SD card (absolute path)
AT+BNCERT_FLASH=0,"/sdcard/certs/ca_bundle.pem"

# Flash from UART (50KB bundle)
AT+BNCERT_FLASH=1,51200
```

**Response:**
- `OK` - Bundle flashed successfully
- `ERROR` - Flash operation failed

**Process:**
1. **Validation Pass**: Reads and validates PEM format, calculates CRC32
2. **Erase Partition**: Clears the certificate partition
3. **Write Header**: Stores 8-byte header (length + CRC32)
4. **Write Bundle**: Streams bundle data in 1KB chunks
5. **Verify**: Re-validates written data against original CRC32

### AT+BNCERT_CLEAR - Clear Certificate Bundle

Clears the certificate bundle partition, forcing fallback to hardcoded certificates.

**Syntax:**
```
AT+BNCERT_CLEAR
```

**Parameters:** None

**Examples:**
```bash
AT+BNCERT_CLEAR
```

**Response:**
- `OK` - Partition cleared successfully
- `ERROR` - Clear operation failed

### AT+BNCERT? - Query Certificate Bundle Status

Returns information about the current certificate bundle.

**Syntax:**
```
AT+BNCERT?
```

**Parameters:** None

**Response:**
```
+BNCERT:<status>,<size>,<crc32>
OK
```

**Status Values:**
- `0` = No bundle stored
- `1` = Valid bundle available  
- `2` = Bundle corrupted

**Example Response:**
```
+BNCERT:1,232574,0x46B1365A
OK
```

## Partition Layout

The certificate bundle partition uses a simple 8-byte header format:

```
Offset 0x00: Bundle Length (4 bytes) - Size of certificate data in bytes
Offset 0x04: Bundle CRC32 (4 bytes)  - CRC32 checksum of certificate data
Offset 0x08: Bundle Data (up to 255KB) - PEM certificate bundle
```

**Key Specifications:**
- **Partition Type**: `ESP_PARTITION_TYPE_DATA`
- **Partition Subtype**: `0x40` (custom certificate subtype)
- **Total Size**: 256KB
- **Header Size**: 8 bytes
- **Max Bundle Size**: 255KB (256KB - 8 bytes)

## Memory Management

The system is designed for ESP32's memory constraints:

### Large Bundle Handling
When a large certificate bundle (>150KB) is detected:
1. **Memory Check**: Verifies available heap before processing
2. **Streaming Mode**: Uses 1KB chunks to avoid large memory allocations
3. **Direct Flash Access**: TLS operations use memory-mapped flash pointers
4. **Automatic Fallback**: Falls back to smaller hardcoded bundle if memory insufficient

### Memory Usage Patterns
- **Flashing Operation**: ~2KB RAM (dual 1KB buffers for streaming)
- **TLS Usage**: 0 bytes additional RAM (direct flash access)
- **Validation**: ~1KB RAM for CRC calculation chunks

## Integration with HTTPS/TLS

### Automatic Certificate Selection
The system automatically selects the best certificate source:

1. **Primary**: Flash-stored certificate bundle (if valid and memory sufficient)
2. **Fallback**: Hardcoded essential CA bundle (memory-optimized)  
3. **Permissive**: No certificate verification (if all else fails)

### Memory-Aware HTTPS
For HTTPS requests, the system:
- Checks available memory before using large bundles
- Automatically falls back to smaller certificate sets
- Logs memory usage for debugging
- Prevents out-of-memory errors during TLS handshake

## Error Handling

### Common Error Scenarios

**File Not Found:**
```
E (12345) CERT_BUNDLE: Failed to open file: @/certs/missing.pem
E (12345) CERT_BUNDLE_AT: Certificate bundle flash failed: Invalid parameter
```

**Memory Insufficient:**
```
W (12345) BNCURL_COMMON: Insufficient memory for large bundle (168604 < 282586)
I (12345) BNCURL_COMMON: Certificate bundle not configured, trying hardcoded CA bundle fallback
```

**CRC Validation Failed:**
```
E (12345) CERT_BUNDLE: Bundle validation failed after write
E (12345) CERT_BUNDLE_AT: Certificate bundle flash failed: CRC validation failed
```

**Invalid PEM Format:**
```
E (12345) CERT_BUNDLE: PEM validation failed: Invalid certificate format
E (12345) CERT_BUNDLE_AT: Certificate bundle flash failed: Invalid parameter
```

## Troubleshooting

### Bundle Not Loading
1. Check file exists on SD card: `AT+BNSD_MOUNT?`
2. Verify file path syntax (use @ prefix or full path)
3. Check bundle format is valid PEM
4. Ensure sufficient free memory

### Memory Issues
1. Monitor free memory: Logs show available heap
2. Use smaller certificate bundles (<100KB recommended)
3. Clear unused certificates to free memory
4. Consider using hardcoded bundle fallback

### HTTPS Connection Issues
1. Check certificate bundle status: `AT+BNCERT?`
2. Verify system time is synchronized (required for certificate validation)
3. Check if target server's CA is in bundle
4. Monitor memory usage during connection

## Best Practices

### Bundle Size Optimization
- **Recommended Size**: <100KB for optimal ESP32 performance
- **Certificate Count**: Limit to 20-50 essential root CAs
- **Content**: Include only CAs needed for your specific use case

### File Management
- Store bundles on SD card in `/certs/` directory
- Use descriptive filenames: `ca_bundle_essential.pem`
- Keep backup of working bundles
- Test bundles in development environment first

### Memory Management
- Monitor heap usage during development
- Test with realistic certificate bundle sizes
- Use query command to verify bundle status
- Clear unused bundles to free space

### Security Considerations
- Verify certificate bundle integrity before deployment
- Use proper certificate validation in production
- Keep certificate bundles updated with latest root CAs
- Monitor for certificate expiration

## Migration from Legacy System

### Key Changes from Old BNCERT System
- **Single Partition**: Replaces dual-partition approach
- **Simplified Commands**: Streamlined parameter structure
- **Memory Efficiency**: Direct flash access instead of RAM loading  
- **Automatic Fallback**: Built-in hardcoded certificate support
- **Streaming Support**: Handles large bundles without memory issues

### Updated Command Syntax
**Old:** `AT+BNCERT_FLASH=0x380000,"@/certs/bundle.pem"`
**New:** `AT+BNCERT_FLASH=0,"@/certs/bundle.pem"`

The flash address is no longer needed as the system automatically uses the dedicated certificate partition.