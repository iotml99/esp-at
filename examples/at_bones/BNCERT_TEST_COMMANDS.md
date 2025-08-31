# Quick Certificate Flashing Test Commands

## Basic Certificate Flashing Tests

### Test 1: Certificate from File
```bash
# Mount SD card
AT+BNSD_MOUNT

# Flash server certificate from file
AT+BNFLASH_CERT=0x340000,@/certs/server.crt

# Expected: +BNFLASH_CERT:OK,0x00340000,1432
#           OK
```

### Test 2: Certificate from UART
```bash
# Flash certificate via UART (1400 bytes)
AT+BNFLASH_CERT=0x350000,1400

# System prompts: >
# Send 1400 bytes of certificate data now

# Expected: +BNFLASH_CERT:OK,0x00350000,1400
#           OK
```

### Test 3: Multiple Certificates
```bash
# Flash server certificate
AT+BNFLASH_CERT=0x340000,@/certs/server.crt

# Flash private key  
AT+BNFLASH_CERT=0x350000,@/certs/server.key

# Flash CA certificate
AT+BNFLASH_CERT=0x360000,@/certs/ca.crt
```

## Error Testing

### Test 4: Invalid Addresses
```bash
# Too low address (before partition)
AT+BNFLASH_CERT=0x1000,@/test.crt
# Expected: ERROR: Flash address outside certificate partition

# Unaligned address  
AT+BNFLASH_CERT=0x340001,@/test.crt
# Expected: ERROR: Flash address not 4-byte aligned

# Too high address (after partition end)
AT+BNFLASH_CERT=0x500000,@/test.crt
# Expected: ERROR: Flash address outside certificate partition
```

### Test 5: File Errors
```bash
# Non-existent file
AT+BNFLASH_CERT=0x2A000,@/missing/file.crt
# Expected: ERROR: File operation error

# Invalid file path
AT+BNFLASH_CERT=0x2A000,@invalid_path
# Expected: ERROR: File operation error
```

### Test 6: Data Size Errors
```bash
# Too large size
AT+BNFLASH_CERT=0x2A000,100000
# Expected: ERROR: Invalid data size (must be 0-65536)

# Negative size (invalid format)
AT+BNFLASH_CERT=0x2A000,-1
# Expected: ERROR: Invalid data size

# Non-numeric size
AT+BNFLASH_CERT=0x2A000,abc
# Expected: ERROR: Invalid data size
```

## Help and Information

### Test 7: Command Help
```bash
# Get detailed help
AT+BNFLASH_CERT=?

# Expected: Comprehensive help showing:
# - Command syntax
# - Parameter descriptions
# - Usage examples
# - Address ranges
# - Size limits
```

## Expected Output Examples

### Successful File Flashing:
```
AT+BNFLASH_CERT=0x340000,@/certs/server.crt
+BNFLASH_CERT:OK,0x00340000,1432
OK
```

### Successful UART Flashing:
```
AT+BNFLASH_CERT=0x350000,1400
>
(1400 bytes of certificate data transmitted)
+BNFLASH_CERT:OK,0x00350000,1400
OK
```

### Error Examples:
```
AT+BNFLASH_CERT=0x1000,@/test.crt
ERROR: Flash address 0x00001000 outside certificate partition

AT+BNFLASH_CERT=0x340000,@/missing.crt  
ERROR: Certificate flashing failed: File operation error

AT+BNFLASH_CERT=0x340000,100000
ERROR: Invalid data size: 100000 (must be 0-65536)
```

## Recommended Flash Layout

### Certificate Address Planning:
```bash
# Certificate partition: 0x340000 - 0x440000 (1MB)

# Server certificates (0x340000 - 0x360000)
AT+BNFLASH_CERT=0x340000,@/certs/server1.crt
AT+BNFLASH_CERT=0x350000,@/certs/server2.crt

# Client certificates (0x360000 - 0x380000)  
AT+BNFLASH_CERT=0x360000,@/certs/client1.crt
AT+BNFLASH_CERT=0x370000,@/certs/client2.crt

# Private keys (0x380000 - 0x3A0000)
AT+BNFLASH_CERT=0x380000,@/certs/server1.key
AT+BNFLASH_CERT=0x390000,@/certs/client1.key

# CA certificates (0x3A0000 - 0x3C0000)
AT+BNFLASH_CERT=0x3A0000,@/certs/ca_root.crt
AT+BNFLASH_CERT=0x3B0000,@/certs/ca_intermediate.crt
```

## UART Data Transfer Tips

### Binary Data Transfer:
1. Set terminal to binary/raw mode
2. Send data immediately after '>' prompt
3. Ensure exact byte count matches parameter
4. No extra characters or line endings
5. Complete transfer within 30 seconds

### File Preparation:
```bash
# Check certificate size
ls -la certificate.der
# -rw-r--r-- 1 user user 1432 Aug 31 10:00 certificate.der

# Use exact size in command
AT+BNFLASH_CERT=0x340000,1432
```
