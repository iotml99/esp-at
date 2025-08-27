# ESP32-AT Commands Test Suite

Comprehensive test suite for ESP32-AT firmware with custom ATBN commands.

## Quick Start

### 1. Setup Environment

```bash
# Install dependencies
pip install -r requirements.txt

# Copy environment template
cp .env.example .env

# Edit .env with your settings
SERIAL_PORT=COM3
WIFI_SSID=YourWiFiName
WIFI_PASSWORD=YourWiFiPassword
```

### 2. Run Tests

```bash
# Run all tests
pytest test_esp32_at_commands.py -v

# Run specific test categories
pytest test_esp32_at_commands.py -m basic -v          # Basic tests only
pytest test_esp32_at_commands.py -m wifi -v           # WiFi tests only
pytest test_esp32_at_commands.py -m performance -v    # Performance tests

# Skip slow tests
pytest test_esp32_at_commands.py -m "not slow" -v

# Generate HTML report
pytest test_esp32_at_commands.py --html=report.html

# Run directly (non-pytest mode)
python test_esp32_at_commands.py
```

## Test Categories

- **Basic**: AT connectivity, firmware version
- **WiFi**: Connection, IP address queries  
- **SD Card**: Mount, unmount, status, space queries
- **HTTP**: GET, HEAD, POST requests with BNCURL
- **Performance**: Large file downloads (1MB, 10MB, 80MB)
- **Advanced**: Webradio, WPS, timeout configuration

## Requirements

- ESP32 with custom AT firmware
- SD card (for SD card and download tests)
- WiFi network (for network tests)
- Python 3.7+ with pyserial, pytest, python-dotenv

## Configuration

The test suite uses environment variables from `.env` file:

```bash
# Serial connection
SERIAL_PORT=COM3              # Windows
# SERIAL_PORT=/dev/ttyUSB0    # Linux
# SERIAL_PORT=/dev/cu.usbserial-* # macOS

# WiFi credentials (optional - WiFi tests will be skipped if not provided)
WIFI_SSID=YourNetworkName
WIFI_PASSWORD=YourNetworkPassword
```

## Hardware Setup

- ESP32 connected via USB-to-Serial converter
- UART1 (GPIO 6/7) used for AT commands
- SD card module connected (optional, for SD card tests)
- 115200 baud rate, 8N1, no flow control

## Test Results

Tests automatically skip if:
- Hardware not connected
- WiFi credentials not provided (for WiFi tests)
- SD card not detected (for SD card tests)

Example output:
```
test_esp32_at_commands.py::TestBasicATCommands::test_basic_connectivity PASSED
test_esp32_at_commands.py::TestWiFiFunctionality::test_wifi_connection PASSED
test_esp32_at_commands.py::TestSDCardOperations::test_sd_card_mount PASSED
test_esp32_at_commands.py::TestHTTPOperations::test_simple_http_get PASSED
test_esp32_at_commands.py::TestPerformance::test_large_file_download PASSED

=================== 25 passed, 3 skipped in 180.50s ===================
```

## Troubleshooting

1. **Connection Issues**: Check COM port, ensure ESP32 is powered
2. **AT Commands Fail**: Verify custom firmware is flashed
3. **WiFi Tests Skip**: Check WiFi credentials in `.env`
4. **SD Tests Skip**: Ensure SD card is inserted and properly wired
5. **Performance Tests Timeout**: Check internet connection, increase timeout values

For detailed debugging, check `esp32_at_tests.log` file.
