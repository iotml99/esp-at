# ESP32C6 AT Commands Test Suite

This directory contains Python test scripts for testing the ESP32C6 AT commands functionality described in the main ATBN_Readme.md file.

## Files

- `test_at_commands.py` - Comprehensive test suite with automated testing
- `simple_test.py` - Simple example for manual testing **with pytest integration**
- `performance_test.py` - **Performance benchmarking with 50MB download tests**
- `test_with_config.py` - Configuration-driven test script
- `conftest.py` - Pytest configuration and shared fixtures
- `pytest.ini` - Pytest settings and markers
- `requirements.txt` - Python dependencies (includes pytest)
- `README.md` - This file

## Prerequisites

1. **Hardware Setup**: 
   - ESP32C6 with custom AT firmware flashed
   - SD card properly wired (see main readme for wiring)
   - USB-to-Serial converter connected to UART1 (GPIO 6/7)

2. **Python Dependencies**:
   ```bash
   pip install -r requirements.txt
   ```

## Quick Start

### 1. Simple Manual Testing

Use `simple_test.py` for basic command testing:

```bash
# Direct execution (traditional mode)
python simple_test.py

# Pytest execution (recommended)
pytest simple_test.py -v

# Run specific tests
pytest simple_test.py::test_basic_connectivity -v

# Run with markers
pytest simple_test.py -m "not wifi" -v  # Skip WiFi tests
```

**Before running:**
- Update the COM port in `config.ini` (default: COM5)
- Ensure ESP32C6 is connected and powered on

This will test:
- Basic AT connectivity
- Firmware version query
- SD card command help and operations
- HTTP command help
- WiFi connection (if credentials provided)

### 2. Pytest Features

The simple_test.py now includes full pytest integration:

**Test Functions:**
- `test_basic_connectivity()` - Basic AT command
- `test_firmware_version()` - Firmware version query
- `test_sd_card_help()` - SD card help command
- `test_sd_card_status()` - SD card status query
- `test_sd_card_mount()` - SD card mount (skips if no SD card)
- `test_http_command_help()` - HTTP command help
- `test_command_responses()` - Parametrized test for multiple commands
- `test_wifi_connection()` - WiFi connection (skips if no credentials)

**Pytest Markers:**
- `@pytest.mark.hardware` - Tests requiring hardware (auto-applied)
- `@pytest.mark.wifi` - Tests requiring WiFi connection
- `@pytest.mark.sd_card` - Tests requiring SD card
- `@pytest.mark.slow` - Tests that take longer to execute

**Pytest Options:**
```bash
# Verbose output
pytest simple_test.py -v

# Show test coverage for specific markers
pytest simple_test.py -m wifi -v

# Skip certain tests
pytest simple_test.py -m "not slow" -v

# Stop on first failure
pytest simple_test.py -x

# Show local variables on failure
pytest simple_test.py -l

# Run tests in parallel (requires pytest-xdist)
pytest simple_test.py -n auto
```

### 3. Performance Testing (Multi-Size Downloads)

Use `performance_test.py` for performance benchmarking:

```bash
# Direct execution (traditional mode)
python performance_test.py

# Pytest execution (recommended)
pytest performance_test.py -v

# Run only performance tests
pytest -m performance -v

# Run performance tests with detailed output
pytest performance_test.py -v -s

# Run specific performance test sizes
pytest performance_test.py::test_performance_download_small -v  # 1MB, 10MB tests
pytest performance_test.py::test_large_download_performance -v  # 80MB test
pytest performance_test.py::test_httpbin_api_performance -v     # API response tests
```

**Requirements:**
- WiFi credentials must be configured in `config.ini`
- SD card must be inserted and properly wired
- Stable internet connection for downloads
- Extended test time (up to 15 minutes for 80MB download)

**Performance Test Files:**
- **1MB Test**: `https://bones.ch/media/qr/1M.txt` - Quick performance validation
- **10MB Test**: `https://bones.ch/media/qr/10M.txt` - Medium file performance  
- **80MB Test**: `https://bones.ch/media/qr/80M.txt` - Main performance benchmark
- **HTTPBin API**: `https://httpbin.org/get`, `https://httpbin.org/json` - API response testing
- **HTTPBin Bytes**: `https://httpbin.org/bytes/1048576` - 1MB controlled test

**Performance Metrics Measured:**
- SD card mount time
- WiFi connection time  
- Download speed for different file sizes (MB/s and Mbps)
- Total download time per file size
- API response times
- Data transfer efficiency (chunks received)

### 4. Comprehensive Automated Testing

Use `test_at_commands.py` for full automated testing:

```bash
python test_at_commands.py
```

**Configuration needed:**
- Update `SERIAL_PORT` variable (default: COM3)
- Optionally set `WIFI_SSID` and `WIFI_PASSWORD` for WiFi/HTTP tests

This will test:
- Basic connectivity
- Firmware version
- WiFi connection (if credentials provided)
- HTTP operations (if WiFi connected)
- SD card mount/unmount operations

## Test Coverage

The test scripts cover all major functionality from the readme:

### Basic AT Commands
- `AT` - Basic connectivity test
- `AT+GMR` - Firmware version query

### WiFi Commands
- `AT+CWMODE=1` - Set station mode
- `AT+CWJAP="SSID","PASSWORD"` - Connect to WiFi
- `AT+CIFSR` - Get IP address

### SD Card Commands
- `AT+BNSD_MOUNT=?` - Command help
- `AT+BNSD_MOUNT?` - Mount status query
- `AT+BNSD_MOUNT` - Mount SD card
- `AT+BNSD_UNMOUNT` - Unmount SD card

### HTTP Commands (BNCURL)
- `AT+BNCURL=?` - Command help
- `AT+BNCURL=GET,"http://httpbin.org/get"` - Simple HTTP GET
- `AT+BNCURL=GET,"url",-dd,"/sdcard/file.json"` - HTTP GET with SD save

### Performance Testing
- **Multi-Size Download Tests** - 1MB, 10MB, and 80MB file downloads to SD card with timing
- **HTTPBin API Testing** - Fast API response validation with httpbin.org
- **Connection Performance** - WiFi connection and SD mount timing
- **Transfer Speed Measurement** - Throughput analysis across different file sizes
- **Reliable Test Sources** - bones.ch and httpbin.org for consistent testing
- **Comprehensive Metrics** - Download time, speed, chunk analysis, API response times
- `AT+BNSD_MOUNT` - Mount SD card
- `AT+BNSD_UNMOUNT` - Unmount SD card

### HTTP Commands (BNCURL)
- `AT+BNCURL=?` - Command help
- `AT+BNCURL=GET,"http://httpbin.org/get"` - Simple HTTP GET
- `AT+BNCURL=GET,"url",-dd,"/sdcard/file.json"` - HTTP GET with SD save

## Customizing Tests

### Changing Serial Port

Update the `SERIAL_PORT` variable in either script:

```python
SERIAL_PORT = "COM4"  # Windows
SERIAL_PORT = "/dev/ttyUSB0"  # Linux
SERIAL_PORT = "/dev/cu.usbserial-A50285BI"  # macOS
```

### Adding WiFi Credentials

In `test_at_commands.py`, update:

```python
WIFI_SSID = "YourWiFiName"
WIFI_PASSWORD = "YourWiFiPassword"
```

### Adding Custom Tests

You can extend the `ESP32C6Tester` class to add more test methods:

```python
def test_custom_command(self) -> bool:
    """Test a custom AT command."""
    success, response = self.send_command("AT+YOUR_COMMAND")
    return success and "expected_response" in " ".join(response)
```

## Expected Test Results

### Successful Output
```
2025-08-26 10:30:15 - INFO - Connected to COM3 at 115200 baud
2025-08-26 10:30:16 - INFO - Testing basic connectivity...
2025-08-26 10:30:16 - INFO - Sent: AT
2025-08-26 10:30:16 - INFO - Received: OK
...
==================================================
TEST RESULTS SUMMARY
==================================================
basic_connectivity: PASS
firmware_version: PASS
wifi_setup: PASS
http_operations: PASS
sd_card_operations: PASS
==================================================
OVERALL: ALL TESTS PASSED
```

### Common Issues

1. **Serial Connection Failed**:
   - Check COM port number
   - Ensure ESP32C6 is powered on
   - Verify UART1 connections (GPIO 6/7)

2. **AT Commands Not Recognized**:
   - Wrong baud rate (should be 115200)
   - Connected to wrong UART (use UART1, not UART0)
   - Firmware not properly flashed

3. **SD Card Tests Fail**:
   - SD card not inserted or not properly wired
   - SD card not formatted as FAT32
   - Check 3.3V power supply to SD card module

4. **WiFi/HTTP Tests Fail**:
   - WiFi credentials incorrect
   - Network connectivity issues
   - ESP32C6 not getting IP address

## Integration with CI/CD

The test scripts return appropriate exit codes:
- Exit code 0: All tests passed
- Exit code 1: Some tests failed

### Traditional Python Testing
```yaml
- name: Run AT Command Tests
  run: |
    cd atbn_tests
    pip install -r requirements.txt
    python test_at_commands.py
```

### Pytest Integration
```yaml
- name: Run Pytest AT Command Tests
  run: |
    cd atbn_tests
    pip install -r requirements.txt
    pytest simple_test.py -v --tb=short
```

### Advanced CI/CD with Pytest
```yaml
- name: Run AT Command Tests with Coverage
  run: |
    cd atbn_tests
    pip install -r requirements.txt
    pip install pytest-cov pytest-html
    pytest simple_test.py -v --cov=simple_test --html=test_report.html
```

## Pytest Features

The `simple_test.py` now includes full pytest integration:

### Fixtures
- `config` - Loads configuration from config.ini
- `tester_config` - Serial port configuration
- `wifi_config` - WiFi credentials configuration  
- `tester` - Connected ESP32C6 tester instance (session-scoped)

### Test Markers
- `hardware` - Tests requiring hardware connection (auto-applied)
- `wifi` - Tests requiring WiFi connection
- `sd_card` - Tests requiring SD card
- `slow` - Tests that take longer to execute

### Parametrized Tests
The `test_command_responses` function tests multiple AT commands with expected responses in a single parametrized test.

### Test Skipping
Tests automatically skip when:
- Hardware connection fails
- WiFi credentials not provided (for WiFi tests)
- SD card not available (for SD card mount test)

### Custom Pytest Configuration
- `pytest.ini` - Test discovery, output formatting, timeouts
- `conftest.py` - Shared fixtures, session hooks, automatic markers

## Serial Port Detection

To find available serial ports on your system:

**Windows (PowerShell):**
```powershell
Get-WmiObject -query "SELECT * FROM Win32_PnPEntity" | Where-Object {$_.Name -match "COM\d+"} | Select-Object Name, DeviceID
```

**Linux:**
```bash
ls /dev/ttyUSB* /dev/ttyACM*
```

**macOS:**
```bash
ls /dev/cu.*
```

## Troubleshooting

If tests fail, check:

1. **Hardware connections** - Verify wiring matches the main readme
2. **Firmware version** - Ensure custom AT firmware is properly flashed
3. **Serial settings** - 115200 baud, 8N1, no flow control
4. **UART selection** - Must use UART1 (GPIO 6/7) for AT commands
5. **SD card** - Properly formatted FAT32 and correctly wired

For more detailed troubleshooting, see the main ATBN_Readme.md file.
