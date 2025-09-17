# CA Certificate Bundle Generation and Flashing Guide

This guide demonstrates how to generate CA certificate bundles using OpenSSL in Git Bash and flash them to the ESP32-AT device using the BNCERT commands for secure HTTPS connections.

## Overview

The ESP32-AT BNCERT system provides automatic certificate management for HTTPS requests. When certificates are flashed to the dedicated partition, they are automatically detected, classified, and used for TLS connections without manual configuration.

**Important:** Each individual certificate must be under 4KB (4096 bytes) as it occupies one 4KB flash sector. The certificate partition provides 64 sectors (256KB total) for storing up to 64 individual certificates.

## What Are Certificates and Why Do You Need Them?

### Understanding Digital Certificates

Think of digital certificates like ID cards for websites. Just as you might check someone's driver's license to verify their identity, your ESP32 device checks a website's certificate to make sure it's really talking to the legitimate website and not an imposter.

### Why Are Certificates Important?

When your ESP32 device connects to a website using HTTPS (secure HTTP), it needs to:

1. **Verify Identity**: Make sure the website is actually who it claims to be (not a fake site)
2. **Encrypt Data**: Scramble the information sent between your device and the website so eavesdroppers can't read it
3. **Prevent Tampering**: Ensure nobody can modify the data while it's traveling over the internet

### What Happens Without Certificates?

Without proper certificates, your device might:
- Connect to malicious websites pretending to be legitimate services
- Send sensitive data (like API keys) over unencrypted connections
- Receive modified or corrupted data from attackers

### Real-World Example

Imagine your ESP32 is requesting weather data from `api.openweathermap.org`:

**Without Certificate Verification:**
```
ESP32 → Internet → ??? → Weather API
```
Your device can't tell if it's really talking to the weather service or a malicious server.

**With Certificate Verification:**
```
ESP32 → Checks Certificate → Verified Weather API → Encrypted Data Exchange
```
Your device confirms it's talking to the real weather service and encrypts all communication.

### Types of Certificates

**CA (Certificate Authority) Certificates:**
- Like the government agency that issues driver's licenses
- These are the "trusted authorities" that vouch for website certificates
- Examples: DigiCert, Let's Encrypt, VeriSign

**Server Certificates:**
- Like the actual driver's license held by a website
- Issued by a CA to prove the website's identity
- Each website has its own unique certificate

**Certificate Chains:**
- Like a chain of trust from your local DMV up to the federal government
- Root CA → Intermediate CA → Server Certificate
- Your device needs to trust at least one link in this chain

### How the ESP32 Uses Certificates

1. **Storage**: Certificates are stored in a special 256KB flash partition
2. **Detection**: The system automatically detects what type each certificate is
3. **Selection**: When making HTTPS requests, the system picks the right certificate
4. **Verification**: The certificate is used to verify the website's identity
5. **Encryption**: Once verified, encrypted communication begins

### Common Certificate Scenarios

**Scenario 1: Single Website Access**
- You only connect to one HTTPS service (like weather API)
- Flash just that website's CA certificate
- Simple and efficient

**Scenario 2: Multiple Websites**
- Your device connects to several different HTTPS services
- Flash certificates for each service you'll use
- More storage used but covers all your needs

**Scenario 3: General Internet Access**
- Your device might connect to any HTTPS website
- Flash a collection of major CA certificates
- Uses more storage but provides maximum compatibility

### Certificate Lifecycle

**Installation**: Flash certificates to your device (this guide shows you how)
**Validation**: System checks certificate format and registers it
**Usage**: Automatic selection during HTTPS requests
**Expiration**: Certificates have expiration dates (usually 1-3 years)
**Renewal**: Update certificates before they expire

### Security Benefits

- **Data Protection**: All communication is encrypted
- **Identity Verification**: Confirms you're talking to the right server
- **Integrity Checking**: Detects if data has been tampered with
- **Trust Establishment**: Creates a secure foundation for communication

### When You Don't Need This Guide

You might not need custom certificates if:
- Your ESP32 firmware already includes a comprehensive CA bundle
- You only connect to websites with certificates from major CAs
- You're not concerned about the latest certificate updates

### When You DO Need This Guide

You should use this guide if:
- Your HTTPS connections are failing with certificate errors
- You want to minimize memory usage by including only needed certificates
- You need to connect to websites with newer certificates not in your firmware
- You want to ensure the latest security updates for certificate validation

This guide will walk you through the technical steps, but remember: you're essentially giving your ESP32 device a set of "trusted ID checkers" so it can safely communicate with websites on the internet.

## Quick Start with Python Script

**For a faster automated approach**, use the included Python script:

```bash
# First, find your CA bundle location
openssl version -d

# Generate important CA certificates automatically (specify your bundle path)
python generate_ca_certificates.py --bundle /usr/ssl/cert.pem

# Alternative bundle locations:
# python generate_ca_certificates.py --bundle /mingw64/ssl/cert.pem
# python generate_ca_certificates.py --bundle C:\Users\username\system_ca_bundle.pem

# Copy generated files to SD card  
cp sd_card_certs/*.pem /path/to/sd/card/certs/

# Flash using generated AT commands
# (See sd_card_certs/flash_commands.txt)
```

**Note:** You must specify the CA bundle path using the `--bundle` argument. The script no longer auto-detects bundle locations.

This script automates the certificate extraction and preparation process described in Method 3 below. See `CERTIFICATE_GENERATOR_README.md` for full script documentation and help finding your CA bundle path.

**Continue reading for manual methods and detailed understanding.**

## Prerequisites

### Software Requirements
- **Git Bash** (includes OpenSSL)
- **ESP32-AT device** with BNCERT support
- **SD Card** (optional, for file-based certificate loading)
- **Serial terminal** (PuTTY, Tera Term, etc.)

### Hardware Requirements
- ESP32 device with AT firmware
- WiFi connectivity for testing HTTPS requests
- SD card slot (if using file-based certificate loading)

## Test URL

We'll use the OpenWeatherMap API as our test case:
```
https://api.openweathermap.org/data/2.5/weather?lat=42.567001&lon=1.598100&appid=e5eb4def9773176630cc8f18e75be406&lang=tr&units=metric
```

This URL requires TLS/SSL verification and will demonstrate the certificate system working correctly.

## Method 1: Automatic CA Bundle from Target Server

### Step 1: Extract Server Certificate Chain

Use OpenSSL to connect to the target server and extract its certificate chain:

```bash
# Open Git Bash and run:
echo | openssl s_client -showcerts -servername api.openweathermap.org -connect api.openweathermap.org:443 2>/dev/null | openssl x509 -outform PEM > openweather_ca.pem
```

### Step 2: Verify Certificate Content

Check the extracted certificate:

```bash
# Verify certificate details
openssl x509 -in openweather_ca.pem -text -noout

# Check certificate validity
openssl x509 -in openweather_ca.pem -dates -noout
```

Expected output should show:
- **Subject**: Certificate authority information
- **Issuer**: Root CA information  
- **Validity**: Not Before/Not After dates
- **Public Key**: RSA or ECC key information

### Step 3: Flash Certificate to ESP32

#### Option A: Via SD Card (Recommended)

1. Copy certificate to SD card:
```bash
cp openweather_ca.pem /path/to/sd/card/certs/
```

2. Flash using AT command:
```
AT+BNFLASH_CERT=0x380000,@/certs/openweather_ca.pem
```

#### Option B: Via UART

1. Get certificate size:
```bash
wc -c openweather_ca.pem
```

2. Flash using AT command with byte count:
```
AT+BNFLASH_CERT=0x380000,1674
```

3. When prompted with `>`, paste the certificate content.

### Step 4: Verify Certificate Registration

Check that the certificate was properly detected and registered:

```
AT+BNCERT_LIST?
```

Expected response:
```
+BNCERT_LIST:1,16
+BNCERT_ENTRY:0x00380000,1674,"CERTIFICATE"
OK
```

### Step 5: Test HTTPS Connection

Test the secure connection using the flashed certificate:

```
AT+BNCURL="GET","https://api.openweathermap.org/data/2.5/weather?lat=42.567001&lon=1.598100&appid=e5eb4def9773176630cc8f18e75be406&lang=tr&units=metric"
```

Expected successful response:
```
{"coord":{"lon":1.5981,"lat":42.567},"weather":[{"id":804,"main":"Clouds","description":"kapalı","icon":"04d"}],"base":"stations","main":{"temp":12.59,"feels_like":11.89,"temp_min":12.59,"temp_max":15.04,"pressure":1025,"humidity":76,"sea_level":1025,"grnd_level":815},"visibility":10000,"wind":{"speed":1.77,"deg":21,"gust":2.36},"clouds":{"all":99},"dt":1758087729,"sys":{"type":2,"id":2099214,"country":"AD","sunrise":1758087337,"sunset":1758132046},"timezone":7200,"id":3041204,"name":"Canillo","cod":200}
```

## Method 2: Complete Certificate Chain Extraction

For more robust certificate verification, extract the complete certificate chain:

### Step 1: Extract Full Certificate Chain

```bash
# Extract complete certificate chain
echo | openssl s_client -showcerts -servername api.openweathermap.org -connect api.openweathermap.org:443 2>/dev/null | awk '/-----BEGIN CERTIFICATE-----/,/-----END CERTIFICATE-----/' > openweather_chain.pem
```

### Step 2: Split Chain into Individual Certificates

```bash
# Split certificate chain
awk 'BEGIN {cert_num=0} /-----BEGIN CERTIFICATE-----/ {cert_num++; filename="cert_" cert_num ".pem"} {print > filename}' openweather_chain.pem
```

### Step 3: Flash Multiple Certificates

Flash each certificate to different 4KB-aligned addresses:

```bash
# Flash root CA certificate
AT+BNFLASH_CERT=0x380000,@/certs/cert_1.pem

# Flash intermediate CA certificate (if present)
AT+BNFLASH_CERT=0x381000,@/certs/cert_2.pem
```

### Step 4: Verify Multiple Certificates

```
AT+BNCERT_LIST?
```

Expected response for multiple certificates:
```
+BNCERT_LIST:2,16
+BNCERT_ENTRY:0x00380000,1674,"CERTIFICATE"
+BNCERT_ENTRY:0x00381000,1456,"CERTIFICATE"
OK
```

## Method 3: Individual Certificates from System Root CA Bundle (Automated)

For maximum compatibility, use the included Python script to automatically extract and prepare individual certificates from the system root CA bundle. This method intelligently selects important Certificate Authorities and handles all the complex processing automatically.

### Step 1: Use the Certificate Generator Script

Instead of manually processing hundreds of certificates, use the automated script:

```bash
# Find your system's CA bundle location
openssl version -d
# Note the OpenSSL directory (usually /usr/ssl or /mingw64/ssl)

# Run the certificate generator script with your CA bundle path
python generate_ca_certificates.py --bundle /usr/ssl/cert.pem

# Alternative bundle locations if above doesn't exist:
# python generate_ca_certificates.py --bundle /mingw64/ssl/cert.pem
# python generate_ca_certificates.py --bundle /etc/ssl/cert.pem
```

The script will automatically:
- Extract individual certificates from the system bundle
- Filter for important Certificate Authorities (DigiCert, Let's Encrypt, etc.)
- Verify each certificate is under 4KB (ESP32 requirement)
- Generate properly named certificate files
- Create AT commands for flashing

### Step 2: Copy Generated Certificates to SD Card

```bash
# Copy all generated certificates to SD card
cp sd_card_certs/*.pem /path/to/sd/card/certs/
```

### Step 3: Flash Certificates Using Generated Commands

Use the automatically generated AT commands:

```bash
# Execute the commands from the generated file
cat sd_card_certs/flash_commands.txt
```

Example output:
```
AT+BNFLASH_CERT=0x380000,@/certs/DigiCert_Global_Root_CA.pem
AT+BNFLASH_CERT=0x381000,@/certs/ISRG_Root_X1.pem
AT+BNFLASH_CERT=0x382000,@/certs/Amazon_Root_CA_1.pem
# ... additional certificates
```

### Step 4: Verify Installation

Check the generated summary and verify certificates are flashed:

```bash
# Review the generated summary
cat sd_card_certs/certificate_summary.txt

# Verify certificates are registered on ESP32
AT+BNCERT_LIST?
```

**For detailed script options and troubleshooting, see `CERTIFICATE_GENERATOR_README.md`.**

## Certificate Management Commands

### List All Certificates
```
AT+BNCERT_LIST?
```

### List Valid Storage Addresses
```
AT+BNCERT_ADDR?
```

### Clear Specific Certificate
```
AT+BNCERT_CLEAR=0x380000
```

### Check Available Partition Space
```bash
# Calculate used space from BNCERT_LIST response
# Partition size: 256KB (0x40000 bytes)
# Each 4KB sector: 0x1000 bytes
# Total sectors: 64
```

## Troubleshooting

### Issue 1: Certificate Not Detected

**Symptoms:**
```
W (xxxxx) BNCERT_MGR: Certificate type detection failed - unrecognized format
```

**Solutions:**
1. Verify certificate is in valid PEM format:
   ```bash
   openssl x509 -in certificate.pem -text -noout
   ```

2. Check for proper PEM headers:
   ```bash
   head -1 certificate.pem  # Should show: -----BEGIN CERTIFICATE-----
   tail -1 certificate.pem  # Should show: -----END CERTIFICATE-----
   ```

3. Remove any extra whitespace or invalid characters:
   ```bash
   dos2unix certificate.pem  # Convert line endings
   ```

### Issue 2: Address Alignment Error

**Symptoms:**
```
ERROR: Address 0x380001 is not 4KB aligned. Must use: 0x380000, 0x381000, etc.
```

**Solution:**
Use only 4KB-aligned addresses:
- Valid: `0x380000`, `0x381000`, `0x382000`, etc.
- Invalid: `0x380001`, `0x380500`, `0x380FFF`

### Issue 3: HTTPS Connection Fails

**Symptoms:**
```
I (xxxxx) BNCURL_COMMON: No certificates found in partition
I (xxxxx) BNCURL_COMMON: Using hardcoded CA bundle for SSL verification
```

**Debug Steps:**
1. Verify certificates are registered:
   ```
   AT+BNCERT_LIST?
   ```

2. Check partition contains valid certificates:
   ```bash
   # Verify certificate was flashed correctly
   AT+BNCURL="GET","https://httpbin.org/get"  # Test with known working site
   ```

3. Enable debug logging to see certificate loading:
   ```
   # Look for log messages:
   I (xxxxx) BNCERT_MGR: Loaded certificate from 0x00380000 (xxxx bytes)
   I (xxxxx) BNCURL_COMMON: Using CA certificate from partition (xxxx bytes)
   ```

## Testing and Validation

### Comprehensive Test Suite

```bash
# Test 1: Basic HTTPS GET
AT+BNCURL="GET","https://api.openweathermap.org/data/2.5/weather?lat=42.567001&lon=1.598100&appid=e5eb4def9773176630cc8f18e75be406&lang=tr&units=metric"

# Test 2: Different HTTPS endpoint
AT+BNCURL="GET","https://httpbin.org/get"

# Test 3: HTTPS with headers
AT+BNCURL="GET","https://api.github.com/user","-h","User-Agent: ESP32-AT-Test"

# Test 4: Verify certificate usage in logs
# Look for: "Using CA certificate from partition" in debug output
```

### Performance Verification

```bash
# Test certificate loading time
# Compare response times with/without partition certificates

# Without partition certificates (uses hardcoded bundle)
AT+BNCERT_CLEAR=0x380000
AT+BNCURL="GET","https://httpbin.org/get"

# With partition certificates 
AT+BNFLASH_CERT=0x380000,@/certs/httpbin_ca.pem
AT+BNCURL="GET","https://httpbin.org/get"
```

## Conclusion

The BNCERT certificate management system provides a robust, automatic solution for HTTPS certificate handling in ESP32-AT applications. By following this guide, you can:

1. **Generate appropriate CA certificates** using standard OpenSSL tools
2. **Flash certificates efficiently** using simplified BNCERT commands  
3. **Automatically configure TLS** without manual intervention
4. **Troubleshoot common issues** with systematic debugging
5. **Implement secure HTTPS** connections with proper certificate validation

The system's automatic detection and classification of certificate types eliminates the complexity of manual TLS configuration while maintaining security and flexibility for various deployment scenarios.

For the OpenWeatherMap example, the system automatically detects the flashed CA certificate and uses it to establish a secure TLS connection, enabling reliable weather data retrieval over HTTPS.