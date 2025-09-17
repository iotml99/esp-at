# CA Certificate Generator Script

This Python script automates the process of extracting and preparing CA certificates for the ESP32-AT BNCERT system.

## Quick Start

**Note: You must specify the CA bundle path using `--bundle` argument**

```bash
# Generate important CA certificates only (recommended)
python generate_ca_certificates.py --bundle /usr/ssl/cert.pem

# Generate all certificates from system bundle
python generate_ca_certificates.py --bundle /usr/ssl/cert.pem --all

# Use custom output directory
python generate_ca_certificates.py --bundle /etc/ssl/cert.pem --output my_certificates

# Enable verbose output
python generate_ca_certificates.py --bundle C:\Users\user\cert.pem --verbose
```

## What It Does

1. **Processes CA Bundle**: Reads the CA certificate bundle from the path you specify
2. **Extracts Important CAs**: Identifies and extracts certificates from major Certificate Authorities
3. **Size Validation**: Ensures each certificate is under 4KB (ESP32 requirement)
4. **SD Card Ready**: Creates files ready for copying to SD card
5. **AT Commands**: Generates ready-to-use AT+BNFLASH_CERT commands
6. **Summary Report**: Provides detailed information about generated certificates

## Output Files

After running the script, you'll get:

- **Individual .pem files**: One file per CA certificate
- **flash_commands.txt**: AT commands to flash certificates to ESP32
- **certificate_summary.txt**: Detailed report of what was generated

## Requirements

- Python 3.6 or later
- OpenSSL (included with Git Bash on Windows)
- CA bundle file (you must specify the path)

## Finding Your CA Bundle Path

The CA bundle location depends on your system:

### Windows
- **Git Bash/MSYS2**: `/usr/ssl/cert.pem` or `/mingw64/ssl/cert.pem`
- **User Profile**: `C:\Users\your_username\system_ca_bundle.pem`
- **OpenSSL Installation**: Check with `openssl version -d`

### Linux
- **Common locations**: `/etc/ssl/cert.pem` or `/etc/ssl/certs/ca-bundle.crt`
- **CentOS/RHEL**: `/etc/pki/tls/certs/ca-bundle.crt`
- **Ubuntu/Debian**: `/etc/ssl/certs/ca-certificates.crt`

### macOS
- **System**: `/System/Library/OpenSSL/cert.pem`
- **Homebrew**: `/usr/local/etc/openssl/cert.pem`

### Finding Your Bundle
Use this command to find OpenSSL's default certificate directory:
```bash
openssl version -d
```
The CA bundle is usually `cert.pem` in that directory.

## Important Certificate Authorities Included

The script extracts certificates for these major CAs:

- DigiCert (High Assurance EV Root, Global Root G2/G3)
- Let's Encrypt (Authority X3, ISRG Root X1)
- GeoTrust (Global CA, Primary CA)
- VeriSign (Class 3 Public Primary)
- Amazon Root CA (1, 2, 3, 4)
- GlobalSign Root CA & R3
- Google Trust Services
- Cloudflare Inc ECC CA-3
- Microsoft RSA Root CA 2017
- And many more...

## Usage with ESP32-AT

1. **Run the script with your CA bundle path**:
   ```bash
   python generate_ca_certificates.py --bundle /usr/ssl/cert.pem
   ```

2. **Copy files to SD card**:
   ```bash
   cp sd_card_certs/*.pem /path/to/sd/card/certs/
   ```

3. **Flash certificates to ESP32**:
   Use the commands from `flash_commands.txt`:
   ```
   AT+BNFLASH_CERT=0x380000,@/certs/DigiCert_Global_Root_CA.pem
   AT+BNFLASH_CERT=0x381000,@/certs/Let_s_Encrypt_Authority_X3.pem
   # ... and so on
   ```

4. **Verify installation**:
   ```
   AT+BNCERT_LIST?
   ```

5. **Test HTTPS connection**:
   ```
   AT+BNCURL="GET","https://httpbin.org/get"
   ```

## Command Line Options

```
--bundle, -b     Path to CA bundle file (REQUIRED)
--output, -o     Output directory (default: sd_card_certs)
--all, -a        Generate all certificates, not just important ones
--verbose, -v    Enable verbose output
--list-cas       List important CAs and exit
```

## Examples

```bash
# List which CAs are considered important
python generate_ca_certificates.py --list-cas

# Generate for specific bundle with verbose output
python generate_ca_certificates.py --bundle /usr/ssl/cert.pem --verbose

# Generate all certificates to custom directory
python generate_ca_certificates.py --bundle /etc/ssl/cert.pem --all --output all_certificates

# Windows examples
python generate_ca_certificates.py --bundle C:\Users\username\cert.pem
python generate_ca_certificates.py --bundle "C:\Program Files\Git\usr\ssl\cert.pem" --verbose
```

This script simplifies the certificate management process described in the main CA Certificate Bundle Guide.