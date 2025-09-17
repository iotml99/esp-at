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

✅ **Data Protection**: All communication is encrypted
✅ **Identity Verification**: Confirms you're talking to the right server
✅ **Integrity Checking**: Detects if data has been tampered with
✅ **Trust Establishment**: Creates a secure foundation for communication

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

**⚡ For a faster automated approach**, use the included Python script:

```bash
# Generate important CA certificates automatically
python generate_ca_certificates.py

# Copy generated files to SD card  
cp sd_card_certs/*.pem /path/to/sd/card/certs/

# Flash using generated AT commands
# (See sd_card_certs/flash_commands.txt)
```

This script automates the certificate extraction and preparation process described in Method 3 below. See `CERTIFICATE_GENERATOR_README.md` for full script documentation.

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
{"coord":{"lon":1.5981,"lat":42.567},"weather":[{"id":501,"main":"Rain","description":"orta şiddetli yağmur","icon":"10d"}],"base":"stations","main":{"temp":20.04,"feels_like":19.95,"temp_min":20.04,"temp_max":26.98,"pressure":1025,"humidity":71,"sea_level":1025,"grnd_level":816},"visibility":10000,"wind":{"speed":1.02,"deg":348,"gust":2.37},"rain":{"1h":2.37},"clouds":{"all":100},"dt":1751548805,"sys":{"type":2,"id":19636,"country":"AD","sunrise":1751516448,"sunset":1571273},"timezone":7200,"id":3041204,"name":"Canillo","cod":200}
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

## Method 3: Individual Certificates from System Root CA Bundle

For maximum compatibility, extract individual certificates from the system root CA bundle. Each certificate must be under 4KB to fit in a single flash sector:

### Step 1: Extract System CA Bundle

```bash
# On Windows with Git Bash, find and extract system CA bundle
openssl version -d
# Note the OpenSSL directory (usually /usr/ssl or /mingw64/ssl)

# Copy system CA bundle
cp /usr/ssl/cert.pem system_ca_bundle.pem

# Alternative locations if above doesn't exist:
# cp /mingw64/ssl/cert.pem system_ca_bundle.pem
# cp /etc/ssl/cert.pem system_ca_bundle.pem
```

### Step 2: Split Bundle into Individual Certificates

The system CA bundle contains hundreds of individual certificates. Split them into separate files:

```bash
# Create directory for individual certificates
mkdir -p individual_certs

# Split the bundle into individual certificate files
awk '
BEGIN { cert_count = 0; in_cert = 0 }
/-----BEGIN CERTIFICATE-----/ { 
    in_cert = 1; 
    cert_count++; 
    filename = "individual_certs/ca_cert_" sprintf("%03d", cert_count) ".pem" 
}
in_cert { print > filename }
/-----END CERTIFICATE-----/ { in_cert = 0; close(filename) }
' system_ca_bundle.pem

echo "Extracted $cert_count individual certificates"
```

### Step 3: Verify Certificate Sizes

Ensure each certificate is under 4KB (4096 bytes):

```bash
# Check sizes of all extracted certificates
for cert in individual_certs/*.pem; do
    size=$(wc -c < "$cert")
    if [ $size -gt 4096 ]; then
        echo "WARNING: $cert is $size bytes (exceeds 4KB limit)"
    else
        echo "$cert: $size bytes (OK)"
    fi
done

# Count certificates under 4KB
valid_certs=$(find individual_certs -name "*.pem" -exec wc -c {} + | awk '$1 <= 4096 {count++} END {print count+0}')
echo "Found $valid_certs certificates under 4KB limit"
```

### Step 4: Select and Flash Relevant Certificates

Instead of flashing all certificates, select only those relevant to your target servers:

```bash
# Method A: Flash certificates for specific issuers (recommended)
# Find certificates from major CAs used by common services
grep -l "DigiCert\|Let's Encrypt\|GeoTrust\|Verisign\|Comodo" individual_certs/*.pem

# Method B: Flash certificates by examining target server's chain
echo | openssl s_client -showcerts -servername api.openweathermap.org -connect api.openweathermap.org:443 2>/dev/null | openssl x509 -issuer -noout
# Use the issuer information to find the matching certificate from individual_certs/

# Method C: Flash first 64 certificates (maximum partition capacity)
# Each certificate uses one 4KB sector, partition has 64 sectors total
```

### Step 5: Flash Selected Certificates

Flash individual certificates to 4KB-aligned addresses:

```bash
# Example: Flash first 10 certificates for testing
cert_files=(individual_certs/ca_cert_001.pem individual_certs/ca_cert_002.pem individual_certs/ca_cert_003.pem individual_certs/ca_cert_004.pem individual_certs/ca_cert_005.pem individual_certs/ca_cert_006.pem individual_certs/ca_cert_007.pem individual_certs/ca_cert_008.pem individual_certs/ca_cert_009.pem individual_certs/ca_cert_010.pem)

base_addr=0x380000
for i in "${!cert_files[@]}"; do
    cert_file="${cert_files[$i]}"
    address=$(printf "0x%X" $((base_addr + i * 0x1000)))
    echo "Flashing $(basename "$cert_file") to address $address"
    # Copy to SD card
    cp "$cert_file" "/path/to/sd/card/certs/"
    # Flash command
    echo "AT+BNFLASH_CERT=$address,@/certs/$(basename "$cert_file")"
done
```

### Step 6: Smart Certificate Selection

For optimal compatibility, select certificates based on common web services:

```bash
# Create a list of important CA certificates by issuer
important_cas=(
    "DigiCert High Assurance EV Root CA"
    "DigiCert Global Root CA"
    "Let's Encrypt Authority X3"
    "GeoTrust Global CA"
    "VeriSign Class 3 Public Primary Certification Authority"
    "Comodo CA Limited"
    "Baltimore CyberTrust Root"
    "AddTrust External CA Root"
)

# Find and extract these specific certificates
for ca in "${important_cas[@]}"; do
    echo "Looking for: $ca"
    grep -l "$ca" individual_certs/*.pem | head -1
done > selected_certificates.txt

# Flash the selected certificates
addr=0x380000
while IFS= read -r cert_file; do
    if [ -f "$cert_file" ]; then
        size=$(wc -c < "$cert_file")
        echo "Flashing $(basename "$cert_file") ($size bytes) to $addr"
        cp "$cert_file" "/path/to/sd/card/certs/"
        echo "AT+BNFLASH_CERT=$addr,@/certs/$(basename "$cert_file")"
        addr=$(printf "0x%X" $((addr + 0x1000)))
    fi
done < selected_certificates.txt
```

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

### Issue 4: Certificate Size Too Large

**Symptoms:**
```
ERROR: Certificate size xxxx exceeds maximum 4096 bytes per sector
```

**Solution:**
Each certificate must be under 4KB (4096 bytes). Large certificate chains must be split into individual certificates:

```bash
# Check certificate size before flashing
wc -c certificate.pem

# If certificate is too large, split certificate chain into individual certificates
awk '
BEGIN { cert_count = 0; in_cert = 0 }
/-----BEGIN CERTIFICATE-----/ { 
    in_cert = 1; 
    cert_count++; 
    filename = "cert_" sprintf("%03d", cert_count) ".pem" 
}
in_cert { print > filename }
/-----END CERTIFICATE-----/ { in_cert = 0; close(filename) }
' large_certificate_chain.pem

# Verify each individual certificate is under 4KB
for cert in cert_*.pem; do
    size=$(wc -c < "$cert")
    if [ $size -le 4096 ]; then
        echo "$cert: $size bytes (OK for flashing)"
    else
        echo "$cert: $size bytes (TOO LARGE - cannot flash)"
    fi
done

# Flash individual certificates to separate 4KB sectors
AT+BNFLASH_CERT=0x380000,@/certs/cert_001.pem
AT+BNFLASH_CERT=0x381000,@/certs/cert_002.pem
AT+BNFLASH_CERT=0x382000,@/certs/cert_003.pem
```

## Security Considerations

### Certificate Validation
- The system only validates PEM/DER format, not authenticity
- Always verify certificates come from trusted sources
- Regularly update certificates before expiration

### Storage Security
- Certificates stored in dedicated flash partition (0x380000-0x3C0000)
- No encryption at rest - consider this for sensitive applications
- Partition can be read by firmware but not external access

### Network Security
- Always use HTTPS URLs for sensitive data
- Certificate verification prevents man-in-the-middle attacks
- System falls back to hardcoded CA bundle if partition certificates fail

## Advanced Usage

### Automated Certificate Updates

Create a script to automatically update certificates:

```bash
#!/bin/bash
# update_certificates.sh

# Download latest certificate for target server
echo | openssl s_client -showcerts -servername api.openweathermap.org -connect api.openweathermap.org:443 2>/dev/null | openssl x509 -outform PEM > /tmp/new_cert.pem

# Compare with existing certificate
if ! cmp -s /tmp/new_cert.pem /path/to/current/cert.pem; then
    echo "Certificate updated, flashing to ESP32..."
    cp /tmp/new_cert.pem /sd/card/certs/openweather_ca.pem
    # Send AT command to reflash certificate
    echo "AT+BNCERT_CLEAR=0x380000" > /dev/ttyUSB0
    echo "AT+BNFLASH_CERT=0x380000,@/certs/openweather_ca.pem" > /dev/ttyUSB0
fi
```

### Multiple Server Certificates

For applications connecting to multiple HTTPS servers:

```bash
# Generate certificates for multiple servers
servers=("api.openweathermap.org" "httpbin.org" "api.github.com")
addr=0x380000

for server in "${servers[@]}"; do
    echo "Processing $server..."
    
    # Extract certificate and verify size
    echo | openssl s_client -showcerts -servername $server -connect $server:443 2>/dev/null | openssl x509 -outform PEM > "${server}_ca.pem"
    
    # Check certificate size (must be under 4KB)
    size=$(wc -c < "${server}_ca.pem")
    if [ $size -gt 4096 ]; then
        echo "ERROR: Certificate for $server is $size bytes (exceeds 4KB limit)"
        continue
    fi
    
    echo "Certificate for $server: $size bytes (OK)"
    echo "Flashing to address $addr..."
    
    # Copy to SD card and flash
    cp "${server}_ca.pem" "/sd/card/certs/"
    printf "AT+BNFLASH_CERT=0x%X,@/certs/%s_ca.pem\n" $addr $server
    
    # Increment to next 4KB boundary
    addr=$((addr + 0x1000))
done

# Verify all certificates are registered
echo "Verifying flashed certificates:"
echo "AT+BNCERT_LIST?"
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