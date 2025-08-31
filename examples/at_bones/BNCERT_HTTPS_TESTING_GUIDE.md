# BNCERT HTTPS Integration Testing Guide

This guide covers testing the integration between certificate flashing (`AT+BNFLASH_CERT`) and HTTPS requests (`AT+BNCURL`) to verify that certificates stored in the partition are automatically detected and used for TLS connections.

## Overview

The certificate manager provides a bridge between flashed certificates and the HTTPS/TLS stack:

1. **Certificate Storage**: Certificates are stored in a dedicated 1MB partition (0x340000-0x440000)
2. **Automatic Registration**: Flashed certificates are automatically registered with the certificate manager
3. **Type Detection**: Certificate types (CA cert, client cert, private key) are automatically detected by content
4. **TLS Integration**: HTTPS requests automatically use partition certificates when available
5. **Fallback Strategy**: Falls back to hardcoded certificates if partition certificates are not available

## Simplified Command Syntax

The certificate flashing command now only requires 2 parameters:

```
AT+BNFLASH_CERT=<flash_address>,<data_source>
```

- **flash_address**: Hex address in certificate partition (0x340000-0x440000)
- **data_source**: Either `@/path/to/file` or byte count for UART input

**Certificate types and names are automatically detected** - no need to specify them!

## Prerequisites

1. **Partition Table**: Ensure you're using the updated partition table with certificate partition:
   ```bash
   # Copy the new partition table
   cp partitions_with_certs.csv ../../../partitions.csv
   ```

2. **SD Card**: Required for loading certificates from files
3. **WiFi Connection**: Required for HTTPS testing

## Test Scenarios

### Scenario 1: Basic CA Certificate Testing

#### Step 1: Flash a CA Certificate
```
AT+BNFLASH_CERT=0x340000,@/certs/ca_cert.pem
```

Expected response:
```
+BNFLASH_CERT:OK,0x00340000,<size>
OK
```

#### Step 2: Verify Certificate Registration
```
AT+BNCERT_LIST?
```

Expected response:
```
+BNCERT_LIST:1,16
+BNCERT_ENTRY:0x00340000,<size>,"CERTIFICATE"
OK
```

#### Step 3: Test HTTPS Request with Partition Certificate
```
AT+BNCURL="GET","https://httpbin.org/get"
```

Expected behavior:
- Certificate manager initializes and finds 1 certificate
- Certificate type automatically detected as CERTIFICATE
- HTTPS request uses partition certificate for TLS verification
- Success response with data

### Scenario 2: Client Certificate Authentication

#### Step 1: Flash Client Certificate and Key
```
AT+BNFLASH_CERT=0x350000,@/certs/client_cert.pem
AT+BNFLASH_CERT=0x360000,@/certs/client_key.pem
```

#### Step 2: Verify Both Certificates
```
AT+BNCERT_LIST?
```

Expected response:
```
+BNCERT_LIST:2,16
+BNCERT_ENTRY:0x00350000,<size1>,"CERTIFICATE"
+BNCERT_ENTRY:0x00360000,<size2>,"PRIVATE_KEY"
OK
```

#### Step 3: Test Client Certificate HTTPS Request
```
AT+BNCURL="GET","https://client-cert-required-site.com/secure"
```

Expected behavior:
- Both certificate and key automatically detected and classified
- Client authentication configured automatically
- Secure data retrieved

### Scenario 3: Multiple Certificates with Auto-Detection

#### Step 1: Flash Multiple Certificates
```
AT+BNFLASH_CERT=0x340000,@/certs/ca_cert.pem
AT+BNFLASH_CERT=0x350000,@/certs/client_cert.pem  
AT+BNFLASH_CERT=0x360000,@/certs/client_key.pem
```

#### Step 2: Verify Auto-Detection
```
AT+BNCERT_LIST?
```

Expected response:
```
+BNCERT_LIST:3,16
+BNCERT_ENTRY:0x00340000,<size1>,"CERTIFICATE"
+BNCERT_ENTRY:0x00350000,<size2>,"CERTIFICATE" 
+BNCERT_ENTRY:0x00360000,<size3>,"PRIVATE_KEY"
OK
```

#### Step 3: Test Complete TLS Setup
```
AT+BNCURL="GET","https://mutual-tls-site.com/secure"
```

Expected behavior:
- First certificate used as CA certificate
- Second certificate used as client certificate  
- Private key paired with client certificate
- Complete mutual TLS authentication
- Secure connection established

### Scenario 4: UART Certificate Input

#### Step 1: Flash Certificate via UART
```
AT+BNFLASH_CERT=0x340000,2048
```

Expected prompt:
```
>
```

Then paste certificate data and the system will collect exactly 2048 bytes.

#### Step 2: Verify UART Certificate
```
AT+BNCERT_LIST?
```

## Auto-Detection Logic

The certificate manager automatically detects certificate types by analyzing content:

### Certificate Detection
- **PEM Format**: `-----BEGIN CERTIFICATE-----` 
- **DER Format**: Starts with `0x30 0x82`
- **Classification**: Detected as "CERTIFICATE"

### Private Key Detection  
- **PEM Formats**: 
  - `-----BEGIN PRIVATE KEY-----`
  - `-----BEGIN RSA PRIVATE KEY-----` 
  - `-----BEGIN EC PRIVATE KEY-----`
- **DER Format**: Starts with `0x30 0x82`
- **Classification**: Detected as "PRIVATE_KEY"

### TLS Configuration Strategy
1. **First certificate found** → Used as CA certificate
2. **Second certificate found** → Used as client certificate
3. **First private key found** → Paired with client certificate

## Debugging and Troubleshooting

### Certificate Manager Debugging

1. **Check Certificate Manager Initialization**:
   ```
   # Look for log message:
   I (xxxxx) BNCERT_MGR: Certificate manager initialized with partition at 0x00340000 (1048576 bytes)
   ```

2. **Verify Certificate Loading and Detection**:
   ```
   # Look for log messages:
   I (xxxxx) BNCERT_MGR: Loaded certificate from 0x00340000 (xxxx bytes)
   D (xxxxx) BNCERT_MGR: Detected PEM certificate format
   I (xxxxx) BNCURL_COMMON: Using CA certificate from partition (xxxx bytes)
   ```

3. **Check Auto-Detection**:
   ```
   # Look for log messages during HTTPS requests:
   I (xxxxx) BNCURL_COMMON: Found 3 certificates in partition, attempting to configure TLS
   I (xxxxx) BNCURL_COMMON: Using CA certificate from partition (xxxx bytes)
   I (xxxxx) BNCURL_COMMON: Using client certificate from partition (xxxx bytes)
   I (xxxxx) BNCURL_COMMON: Using client key from partition (xxxx bytes)
   ```

### Common Issues

#### Issue 1: Certificate Type Not Detected
```
W (xxxxx) BNCERT_MGR: Certificate type detection failed - unrecognized format
```

**Solution**: Ensure certificate is in valid PEM or DER format.

#### Issue 2: No Certificates Found
```
I (xxxxx) BNCURL_COMMON: No certificates found in partition
I (xxxxx) BNCURL_COMMON: Using hardcoded CA bundle for SSL verification
```

**Solution**: Flash certificates and verify with `AT+BNCERT_LIST?`

#### Issue 3: Certificate Loading Failed
```
W (xxxxx) BNCERT_MGR: Failed to load certificate at 0x00340000
```

**Solution**: Check flash address is within partition bounds and certificate was flashed successfully.

## Security Considerations

1. **Automatic Detection**: Only validates certificate format, not authenticity
2. **Type Classification**: Based on content markers, not cryptographic validation
3. **Precedence Order**: First certificate becomes CA, second becomes client cert
4. **Secure Storage**: Certificates stored in dedicated flash partition
5. **Memory Safety**: Certificate data properly allocated and freed

## Test Commands Summary

```bash
# Flash certificates (simplified syntax)
AT+BNFLASH_CERT=0x340000,@/certs/ca_cert.pem
AT+BNFLASH_CERT=0x350000,@/certs/client_cert.pem
AT+BNFLASH_CERT=0x360000,@/certs/client_key.pem

# Flash via UART
AT+BNFLASH_CERT=0x370000,2048

# List certificates (shows auto-detected types)
AT+BNCERT_LIST?

# Test HTTPS with auto-configured certificates
AT+BNCURL="GET","https://httpbin.org/get"
AT+BNCURL="GET","https://httpbin.org/ip"
AT+BNCURL="GET","https://api.github.com/user"
```

This simplified approach removes the complexity of specifying certificate types and names while providing automatic detection and configuration for seamless HTTPS integration.
