# BNCERT Certificate Flashing Testing Guide

## Certificate Flashing Implementation Summary

The BNCERT library implements certificate flashing functionality for the ESP32 AT command framework with the following features:

### Certificate Command Overview
- `AT+BNFLASH_CERT=<flash_address>,<data_source>` : Flash certificate data to specified flash address
- `<flash_address>` : Absolute flash memory address in hex format (e.g., 0x2A000)
- `<data_source>` : Either file path (@/path/file) or byte count for UART input

### Certificate Features
1. **Dual Data Sources**: 
   - File mode: Read certificate from SD card file (path starts with @)
   - UART mode: Collect certificate data via UART (numeric byte count)
2. **Flash Safety**: Address validation to prevent bootloader/app corruption
3. **Data Verification**: Flash write verification with read-back comparison
4. **Progress Feedback**: Detailed status responses and error reporting
5. **File System Integration**: Uses SD card mount point normalization

## Implementation Details

### Core Certificate Flow:
1. **Parameter Parsing**: Validate flash address and data source type
2. **Address Validation**: Ensure flash address is in certificate partition (0x380000-0x3C0000)
3. **Data Collection**:
   - File mode: Read certificate from SD card file
   - UART mode: Prompt with '>' and collect specified bytes
4. **Flash Operations**: Erase sector → Write data → Verify integrity
5. **Response**: Success confirmation with address and size

### Flash Safety Zones:
- **Certificate Partition**: 0x380000 - 0x3C0000 (256KB dedicated for certificates)
- **Avoided Areas**: Bootloader, partition table, application code (0x40000-0x380000), NVS
- **Alignment**: 4-byte address alignment required
- **Maximum Size**: 65536 bytes per certificate (limited by implementation)

## Test Examples

### Test 1: Certificate from File
```bash
# Mount SD card first
AT+BNSD_MOUNT

# Flash certificate from file
AT+BNFLASH_CERT=0x380000,@/certs/server_key.bin

# Expected response:
# +BNFLASH_CERT:OK,0x00380000,1432
# OK
```

### Test 2: Certificate from UART
```bash
# Flash certificate via UART (1400 bytes)
AT+BNFLASH_CERT=0x390000,1400

# System responds with prompt:
# >
# (Send 1400 bytes of certificate data now)

# Expected response after data:
# +BNFLASH_CERT:OK,0x00390000,1400
# OK
```

### Test 3: Multiple Certificates
```bash
# Flash server certificate
AT+BNFLASH_CERT=0x380000,@/certs/server.crt

# Flash server private key
AT+BNFLASH_CERT=0x390000,@/certs/server.key

# Flash CA certificate
AT+BNFLASH_CERT=0x3A0000,@/certs/ca.crt
```

### Test 4: Error Conditions
```bash
# Invalid flash address (outside certificate partition)
AT+BNFLASH_CERT=0x1000,@/certs/test.crt
# Expected: ERROR: Invalid flash address: 0x00001000

# Non-existent file
AT+BNFLASH_CERT=0x380000,@/missing/file.crt
# Expected: ERROR: Certificate flashing failed: File operation error

# Invalid data size
AT+BNFLASH_CERT=0x380000,100000
# Expected: ERROR: Invalid data size: 100000 (must be 0-65536)

# Non-aligned address
AT+BNFLASH_CERT=0x380001,@/certs/test.crt
# Expected: ERROR: Flash address 0x00380001 not 4-byte aligned
```

### Test 5: Help and Information
```bash
# Get command help
AT+BNFLASH_CERT=?

# Expected comprehensive help output showing:
# - Command syntax
# - Parameter descriptions  
# - Usage examples
# - Safe address ranges
# - Data size limits
```

## Expected Response Formats

### Successful Certificate Flashing:
```
AT+BNFLASH_CERT=0x380000,@/certs/server.crt
+BNFLASH_CERT:OK,0x00380000,1432
OK
```

### UART Data Collection:
```
AT+BNFLASH_CERT=0x390000,1400
>
(Certificate data transmitted here - 1400 bytes)
+BNFLASH_CERT:OK,0x00390000,1400
OK
```

### Error Responses:
```
AT+BNFLASH_CERT=0x1000,@/test.crt
ERROR: Invalid flash address: 0x00001000

AT+BNFLASH_CERT=0x380000,@/missing.crt
ERROR: Certificate flashing failed: File operation error

AT+BNFLASH_CERT=0x380000,100000
ERROR: Invalid data size: 100000 (must be 0-65536)
```

## Response Field Meanings

### Success Response Fields:
1. **Status**: Always "OK" for successful operations
2. **Address**: Actual flash address used (hex format with leading zeros)
3. **Size**: Number of bytes successfully written to flash

### Error Types:
- **Invalid parameters**: Address out of range, bad alignment, size limits
- **File operation error**: File not found, read failures, permission issues
- **Flash operation error**: Erase/write/verify failures
- **Memory allocation error**: Insufficient RAM for operations
- **UART data collection error**: Timeout or incomplete data reception

## Flash Address Planning

### Recommended Address Layout:
```
0x380000 - 0x390000  : Server certificates (64KB)
0x390000 - 0x3A0000  : Client certificates (64KB)  
0x3A0000 - 0x3B0000  : Private keys (64KB)
0x3B0000 - 0x3C0000  : CA certificates (64KB)
```

### Address Calculation:
```
Base address: 0x380000 (Certificate partition start)
Certificate 1: 0x380000
Certificate 2: 0x380000 + previous_size (rounded to 4KB boundary)
Certificate 3: 0x380000 + cumulative_size (rounded to 4KB boundary)
Maximum address: 0x3C0000 (Certificate partition end)
```

## Hardware Requirements

### Flash Requirements:
- ESP32 with external flash (typically 4MB+)
- Writable flash regions outside app/bootloader areas
- Sufficient free space for certificate storage

### SD Card Requirements (File Mode):
- SD card properly mounted via AT+BNSD_MOUNT
- Certificate files accessible in filesystem
- Sufficient space for certificate files

### Memory Requirements:
- RAM for certificate buffering (up to 64KB)
- Flash sector buffer for erase/write operations

## File Format Support

### Supported Certificate Formats:
- **DER encoded certificates** (.der, .crt)
- **PEM encoded certificates** (.pem, .crt)
- **Private keys** (.key, .pem)
- **Binary certificate data** (.bin)

### File Preparation:
```bash
# Convert PEM to DER for smaller size
openssl x509 -in certificate.pem -outform DER -out certificate.der

# Extract private key to DER
openssl rsa -in private.pem -outform DER -out private.der

# Verify certificate content
openssl x509 -in certificate.der -inform DER -text -noout
```

## Troubleshooting

### Common Issues:

#### Certificate Too Large:
```
ERROR: Invalid data size: 70000 (must be 0-65536)

Solutions:
- Use DER format instead of PEM (smaller)
- Split certificate chains into separate files
- Remove unnecessary intermediate certificates
```

#### Flash Address Conflicts:
```
ERROR: Invalid flash address: 0x00010000

Solutions:
- Use addresses within certificate partition (0x380000-0x3C0000)
- Check partition table for safe regions
- Ensure 4-byte address alignment
```

#### File Access Errors:
```
ERROR: Certificate flashing failed: File operation error

Solutions:
- Verify SD card is mounted (AT+BNSD_MOUNT)
- Check file path spelling and case sensitivity
- Ensure file exists and is readable
- Verify sufficient SD card space
```

#### UART Data Collection Issues:
```
ERROR: Certificate flashing failed: UART data collection error

Solutions:
- Send data immediately after '>' prompt
- Ensure exact byte count matches parameter
- Use binary transfer mode in terminal
- Check for UART timeout (30 seconds)
```

### Debug Steps:
1. **Verify Flash**: Check available flash space and layout
2. **Test SD Card**: Ensure file system is accessible
3. **Address Validation**: Use safe address ranges
4. **File Format**: Verify certificate format and size
5. **Memory Check**: Ensure sufficient RAM available

## Performance Considerations

### Timing:
- **File Reading**: ~1-5 seconds depending on size and SD speed
- **UART Collection**: Depends on baud rate and data size
- **Flash Erase**: ~100-500ms per 4KB sector
- **Flash Write**: ~1-10ms per page (typically 256 bytes)
- **Verification**: ~1-5ms per page read-back

### Memory Usage:
- **Certificate Buffer**: Equal to certificate size (up to 64KB)
- **Verification Buffer**: Additional copy for read-back verification
- **Flash Sector Buffer**: 4KB for erase operations
- **Total RAM needed**: ~128KB for largest certificates (64KB cert + 64KB verify buffer)

### Flash Wear:
- **Erase Cycles**: Modern flash supports 10,000+ erase cycles
- **Sector Management**: Automatic bad block handling
- **Wear Leveling**: Use different addresses for frequent updates

## Integration Notes

### Flash Layout Coordination:
- Coordinate with partition table design (certificates at 0x380000-0x3C0000)
- Reserve dedicated certificate flash regions (256KB total)
- Avoid conflicts with factory app at 0x40000-0x380000

### Certificate Management:
- Plan certificate update procedures within 256KB limit
- Implement certificate validation after flashing
- Consider certificate expiration handling

### Security Considerations:
- Protect private keys with appropriate access controls
- Use secure certificate storage if available
- Implement certificate chain validation

---

*Last Updated: September 1, 2025*
*Certificate Flashing Implementation Version: 1.1*
*Compatible with ESP-AT Framework*
*Certificate Partition: 0x380000-0x3C0000 (256KB)*
