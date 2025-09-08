# ESP32C6 AT Firmware with SD Card Support - Client Setup Guide

This guide will help you set up and use the ESP32C6 with custom AT commands for SD card functionality and HTTP operations.

## Hardware Requirements

- ESP32C6 development board
- MicroSD card (formatted as FAT32)
- MicroSD card module/adapter (SPI interface)
- USB-to-Serial converter (for UART1 communication)
- Jumper wires
- Breadboard (optional)

## Pin Wiring Configuration

### SD Card SPI Connection
Connect your SD card module to the ESP32C6 using the following pin mapping:

| SD Card Module | ESP32C6 Pin | Function |
|----------------|-------------|----------|
| CS             | GPIO 16     | Chip Select |
| MOSI (DI)      | GPIO 17     | Master Out Slave In |
| CLK (SCK)      | GPIO 21     | Serial Clock |
| MISO (DO)      | GPIO 20     | Master In Slave Out |
| VCC            | 3.3V        | Power Supply |
| GND            | GND         | Ground |

### UART Connections

**Important:** The ESP32C6 uses two separate UART interfaces:
- **UART0** (default pins GPIO 16/17) - Used for firmware flashing and bootloader
- **UART1** (GPIO 6/7) - Used for AT commands during normal operation

#### UART0 Connection (for Flashing)
Most ESP32C6 development boards have UART0 connected to the onboard USB-to-Serial chip. Use the main USB port for flashing.

#### UART1 Connection (for AT Commands)
Connect your external USB-to-Serial converter to UART1:

| USB-to-Serial | ESP32C6 Pin | Function |
|---------------|-------------|----------|
| TX            | GPIO 6      | UART1 RX |
| RX            | GPIO 7      | UART1 TX |
| GND           | GND         | Ground |

**Important Notes:**
- Ensure your SD card is formatted as FAT32
- Use 3.3V power supply for the SD card module (not 5V)
- Double-check wiring before powering on
- Some SD card modules have built-in level shifters and voltage regulators

## Firmware Flashing

### Prerequisites
1. **Install ESP-IDF 5.4.2** - This includes Python, esptool, and all necessary tools
   - Follow the [ESP-IDF Installation Guide](https://docs.espressif.com/projects/esp-idf/en/v5.4.2/esp32c6/get-started/index.html)
   - ESP-IDF 5.4.2 comes with esptool pre-installed, so no separate installation needed
2. **Set up ESP-IDF environment** - Run the export script to set up environment variables
   - Windows: `%IDF_PATH%\export.bat`
   - Linux/macOS: `. $IDF_PATH/export.sh`
3. Ensure you have the compiled firmware file (`build.bin`)

### Flashing the Firmware

1. **Connect for Flashing:**
   - Use the main USB port on your ESP32C6 development board (UART0)
   - This is typically the same port used for power and programming

2. **Put ESP32C6 in Download Mode:**
   - Hold the BOOT button
   - Press and release the RESET button
   - Release the BOOT button
   - The device should now be in download mode

3. **Flash the Firmware:**
   Replace `COM4` with your actual COM port for UART0 (Windows) or `/dev/ttyUSB0` (Linux/Mac):

   ```bash
   esptool.py --chip esp32c6 -p COM4 -b 460800 --before=default_reset --after=hard_reset write_flash --flash_mode dio --flash_freq 80m --flash_size 4MB 0x0 build.bin
   ```

   **Command Breakdown:**
   - `--chip esp32c6`: Specifies the ESP32C6 chip
   - `-p COM4`: Serial port (change to your port)
   - `-b 460800`: Baud rate for flashing (faster than default)
   - `--flash_mode dio`: Dual I/O flash mode
   - `--flash_freq 80m`: 80MHz flash frequency
   - `--flash_size 4MB`: 4MB flash size
   - `0x0 build.bin`: Flash address and firmware file

3. **Verify Successful Flash:**
   Look for output similar to:
   ```
   Hash of data verified.
   
   Leaving...
   Hard resetting via RTS pin...
   ```

4. **Reset the Device:**
   After flashing, press the RESET button or power cycle the ESP32C6.

### Finding Your COM Port

**Windows:**
- Device Manager → Ports (COM & LPT)
- Look for "USB Serial Device" or "USB-to-Serial Comm Port"

**Linux:**
```bash
ls /dev/ttyUSB*
# or
dmesg | grep tty
```

**macOS:**
```bash
ls /dev/cu.usbserial*
# or
ls /dev/cu.SLAB_USBtoUART
```

## Serial Communication Setup

**Important:** After flashing, disconnect from UART0 and connect your serial terminal to UART1 (GPIO 6/7) for AT command communication.

### Terminal Configuration
Use any serial terminal software (PuTTY, Tera Term, Arduino Serial Monitor, etc.) connected to UART1 with these settings:

- **Baud Rate:** 115200
- **Data Bits:** 8
- **Stop Bits:** 1
- **Parity:** None
- **Flow Control:** None
- **Line Ending:** CR+LF (\r\n)

## Step-by-Step Usage Guide

### 1. Basic AT Command Testing

First, verify communication with the ESP32C6:

```
AT
```
**Expected Response:**
```
OK
```

### 2. Check Firmware Version

```
AT+GMR
```
**Expected Response:**
```
AT version:4.2.0.0-dev(s-8a71d66 - ESP32C6 - Jul 30 2025 07:11:15)
SDK version:v5.4.1-643-g8ad0d3d8f2-dirty
compile time(0fc8fde0):Aug 24 2025 21:09:20
Bin version:v4.2.0.0-dev(ESP32C6-4MB)

OK
```

### 3. WiFi Connection

#### Set WiFi Mode to Station
```
AT+CWMODE=1
```
**Expected Response:**
```
OK
```

#### Connect to WiFi Network
Replace `"YourSSID"` and `"YourPassword"` with your actual WiFi credentials:

```
AT+CWJAP="YourSSID","YourPassword"
```
**Expected Response:**
```
WIFI CONNECTED
WIFI GOT IP

OK
```

#### Verify IP Address
```
AT+CIFSR
```
**Expected Response:**
```
+CIFSR:STAIP,"192.168.1.100"
+CIFSR:STAMAC,"aa:bb:cc:dd:ee:ff"

OK
```

**Wait for "WIFI GOT IP" before proceeding to custom commands!**

### 4. SD Card Operations

#### Test SD Card Mount Command
```
AT+BNSD_MOUNT=?
```
**Expected Response:**
```
AT+BNSD_MOUNT=? - Test SD card mount command

OK
```

#### Check SD Card Mount Status
```
AT+BNSD_MOUNT?
```
**Expected Response:**
```
AT+BNSD_MOUNT? - SD card mount status: UNMOUNTED

OK
```

#### Mount SD Card
```
AT+BNSD_MOUNT
```
**Expected Success Response:**
```
SD card mounted successfully at /sdcard

OK
```

**Expected Error Response (if SD card not detected):**
```
Failed to mount SD card: ESP_ERR_NOT_FOUND

ERROR
```

#### Unmount SD Card
```
AT+BNSD_UNMOUNT
```
**Expected Response:**
```
SD card unmounted successfully

OK
```

### 5. HTTP Operations with BNCURL

#### URL Configuration with BNURLCFG

For very long URLs that might exceed AT command line limits, you can use the AT+BNURLCFG command to pre-configure a URL, then reference it using "." in AT+BNCURL commands.

##### Set URL Configuration
```
AT+BNURLCFG="https://api.example.com/very/long/endpoint/with/many/parameters?param1=value1&param2=value2"
```
**Expected Response:**
```
OK
```

##### Query Current URL Configuration
```
AT+BNURLCFG?
```
**Expected Response:**
```
+BNURLCFG:"https://api.example.com/very/long/endpoint/with/many/parameters?param1=value1&param2=value2"
OK
```

##### Use Configured URL in BNCURL
```
AT+BNCURL="GET","."
```
**Expected Response:**
```
INFO: Using configured URL: https://api.example.com/very/long/endpoint/with/many/parameters?param1=value1&param2=value2
[... normal BNCURL response ...]
SEND OK

OK
```

##### Example Workflow for Long URLs
```
# 1. Configure the URL
AT+BNURLCFG="https://httpbin.org/get?very_long_parameter=some_very_long_value_that_makes_the_url_exceed_normal_limits"

# 2. Use the configured URL with additional options
AT+BNCURL="GET",".","dd","@/sdcard/response.json"

# 3. Check what URL is currently configured
AT+BNURLCFG?
```

#### Test BNCURL Command Help
```
AT+BNCURL=?
```
**Expected Response:**
```
Usage:
  AT+BNCURL?                                    Query last HTTP code/URL
  AT+BNCURL                                     Execute default request (internal URL)
  AT+BNCURL="GET","<url>"[,"<options>"...]    Perform HTTP GET
  AT+BNURLCFG="<url>"                          Configure URL for use with "."
URL:
  Use "." to reference URL set by AT+BNURLCFG
Options:
  -dd <filepath>   Save body to SD card file (requires mounted SD)
Examples:
  AT+BNCURL="GET","http://httpbin.org/get"       Stream to UART (HTTP)
  AT+BNCURL="GET","https://httpbin.org/get"      Stream to UART (HTTPS)
  AT+BNCURL="GET","http://httpbin.org/get","-dd","/sdcard/response.json"   Save to file (HTTP)
  AT+BNCURL="GET","https://httpbin.org/get","-dd","/sdcard/response.json"  Save to file (HTTPS)
  AT+BNURLCFG="https://httpbin.org/get"          Configure URL
  AT+BNCURL="GET","."                           Use configured URL
Note: Try HTTP first if HTTPS has TLS issues

OK
```

#### Simple HTTP GET Request (Stream to UART)
```
AT+BNCURL="GET","http://httpbin.org/get"
```
**Expected Response Format:**
```
+LEN:XXX,
+POST:1024,{raw JSON data...}
+POST:512,{remaining data...}
SEND OK

OK
```

#### HTTP GET Request Saving to SD Card
**Note: SD card must be mounted first!**

```
AT+BNCURL="GET","http://httpbin.org/get","-dd","/sdcard/test_response.json"
```
**Expected Response:**
```
+BNCURL: Saving to file: /sdcard/test_response.json
+LEN:XXX,
+BNCURL: File saved (XXX bytes)
SEND OK

OK
```

## Output Format Explanation

### BNCURL Response Format

When streaming to UART:
- `+LEN:XXX,` - Announces the total content length in bytes
- `+POST:YYY,<data>` - Data chunks where YYY is chunk size, followed by raw data
- `SEND OK` - Successful completion
- `SEND FAIL` - Failed during data transfer

When saving to SD card:
- `+BNCURL: Saving to file: <filepath>` - Confirms file save operation
- `+LEN:XXX,` - Total content length
- `+BNCURL: File saved (XXX bytes)` - Confirms successful save
- `SEND OK` - Successful completion

### Error Messages

Common error responses:
- `+BNCURL: ERROR SD card not mounted` - SD card must be mounted first
- `+BNCURL: ERROR cannot open file for writing` - File path issue or SD card full
- `+BNCURL: ERROR length-unknown (no Content-Length)` - Server didn't provide content length
- `+BNCURL: ERROR XX <description>` - Various curl/network errors

## Troubleshooting

### URL Configuration Issues

1. **ERROR: Invalid URL format:** URL must start with `http://` or `https://`
2. **ERROR: URL too long:** Maximum URL length is 256 characters
3. **ERROR: No URL configured:** Must use `AT+BNURLCFG` before using "." in `AT+BNCURL`
4. **Example of proper URL format:**
   ```
   ✓ AT+BNURLCFG="https://httpbin.org/get"
   ✓ AT+BNURLCFG="http://api.example.com/endpoint"
   ✗ AT+BNURLCFG="ftp://example.com"              # Wrong protocol
   ✗ AT+BNURLCFG="www.example.com"                # Missing protocol
   ```

### SD Card Issues
1. **Mount fails:** Check wiring, ensure 3.3V power, verify SD card is FAT32 formatted
2. **File write fails:** Check SD card has free space, verify file path syntax

### WiFi Issues
1. **Connection fails:** Verify SSID/password, check signal strength
2. **No IP:** Wait longer, check router DHCP settings

### HTTP Issues
1. **TLS/HTTPS errors:** Try HTTP first, check certificate issues
2. **Timeout errors:** Check internet connection, try shorter URLs
3. **DNS errors:** Verify router DNS settings

### Serial Communication Issues
1. **No response:** Check baud rate (115200), verify TX/RX connections
2. **Garbled text:** Wrong baud rate or loose connections
3. **Command not recognized:** Check line endings (CR+LF)

## Example Test Sequence

Here's a complete test sequence you can follow:

```bash
# 1. Basic connectivity
AT

# 2. Set WiFi mode
AT+CWMODE=1

# 3. Connect to WiFi (replace with your credentials)
AT+CWJAP="YourWiFi","YourPassword"

# 4. Wait for "WIFI GOT IP", then check IP
AT+CIFSR

# 5. Mount SD card
AT+BNSD_MOUNT

# 6. Test simple HTTP request
AT+BNCURL="GET","http://httpbin.org/get"

# 7. Test saving to SD card
AT+BNCURL="GET","https://httpbin.org/json","-dd","/sdcard/test.json"

# 8. Check last request status
AT+BNCURL?

# 9. Unmount SD card
AT+BNSD_UNMOUNT
```

## Custom Commands Summary

| Command | Purpose | Parameters |
|---------|---------|------------|
| `AT+BNSD_MOUNT` | Mount SD card | None |
| `AT+BNSD_UNMOUNT` | Unmount SD card | None |
| `AT+BNCURL` | HTTP operations | Method, URL, optional -dd filepath |

