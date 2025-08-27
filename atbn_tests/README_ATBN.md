# ESP32 ATBN Test Suite

A comprehensive test suite for ESP32 AT+BNCURL commands with environment variable configuration support.

## Overview

This test suite implements the complete ATBN command testing specification from `test.md`, providing automated testing for:

- Basic AT command connectivity
- WiFi connection and management
- SD card operations (mount/unmount/format)
- HTTP GET/POST/HEAD requests
- Header management and cookies
- Range downloads and resumable transfers
- Error handling and edge cases
- Protocol negotiation (HTTP/HTTPS)
- TLS/SSL certificate validation
- Web radio streaming (if implemented)
- Large file operations and performance testing

## Setup

### 1. Install Dependencies

```bash
pip install -r requirements.txt
```

Required packages:
- `pyserial` - Serial communication with ESP32
- `python-dotenv` - Environment variable management
- `pytest` - Test framework
- `pytest-html` - HTML test reports
- `pytest-timeout` - Test timeout handling

### 2. Environment Configuration

Copy the example environment file and configure your settings:

```bash
cp .env.example .env
```

Edit `.env` with your specific configuration:

```env
# WiFi Configuration
WIFI_SSID=YourWiFiNetwork
WIFI_PASSWORD=YourWiFiPassword

# Serial Configuration
SERIAL_PORT=COM5
SERIAL_BAUDRATE=115200
SERIAL_TIMEOUT=10

# Test Configuration
TEST_BASIC_COMMANDS=true
TEST_WIFI_CONNECTION=true
TEST_SD_CARD_OPERATIONS=true
TEST_HTTP_OPERATIONS=true
TEST_ATBN_COMMANDS=true
TEST_VERBOSE_MODE=true
TEST_COOKIE_OPERATIONS=true
TEST_RANGE_DOWNLOADS=true
```

### 3. Hardware Setup

Ensure your ESP32 is properly connected:
- UART1 connection (GPIO 6/7) for AT commands
- SD card properly wired and inserted (for SD tests)
- Stable WiFi network access
- Power supply adequate for WiFi and SD operations

## Running Tests

### Interactive Test Runner

For interactive testing with menu selection:

```bash
python run_atbn_tests.py
```

This provides a menu-driven interface to run specific test categories:
- Basic connectivity tests
- WiFi connection tests
- SD card operation tests
- HTTP GET/POST/HEAD tests
- Cookie management tests
- Error handling tests
- Full comprehensive suite

### Pytest Framework

Run all tests with pytest:

```bash
# Run all tests
pytest

# Run specific test categories
pytest -m wifi
pytest -m sd_card
pytest -m atbn

# Run with HTML report
pytest --html=reports/test_report.html

# Run specific test file
pytest test_atbn_comprehensive.py
pytest test_atbn_advanced.py
```

### Direct Script Execution

Run the comprehensive test suite directly:

```bash
python test_atbn_comprehensive.py
```

Run advanced tests (range downloads, large files, etc.):

```bash
python test_atbn_advanced.py
```

## Test Categories

### Basic Tests (`test_atbn_comprehensive.py`)

#### GET Tests (G1-G12)
- HTTPS → UART with framing
- HTTP → UART
- HTTPS → SD file saving
- HTTP with explicit port
- Single and multiple redirects
- 404 error handling
- Invalid host handling
- SD path validation

#### POST Tests (P1-P8, PE1-PE6)
- UART body → UART response
- UART body → SD file
- Empty body handling
- SD file → UART response
- SD file → SD file
- Content-Type headers
- Order independence validation
- Error cases (missing -du, duplicates, etc.)

#### HEAD Tests (H1-H5, HE1)
- Basic HEAD requests
- Redirect following
- Error responses
- Illegal parameter combinations

#### Header Tests (HC1-HC4)
- Multiple headers
- Duplicate headers (last wins policy)
- Custom Content-Type
- Malformed header handling

#### Cookie Tests (C1-C4)
- Save cookies to SD file
- Send cookies from SD file
- Cookie file overwriting
- Missing cookie file handling

### Advanced Tests (`test_atbn_advanced.py`)

#### Range Download Tests (R1-R8)
- Sequential range downloads with append
- Out-of-order range handling
- Range without destination file (error)
- Range with POST/HEAD (error)

#### Large File Tests
- Large file downloads (>10MB)
- Large POST uploads
- Chunked transfer validation
- Memory usage monitoring

#### Redirect Tests (D1-D3)
- GET 301/302 redirects
- POST 303 redirects (method change)
- POST 307/308 redirects (method preservation)

#### Protocol Tests (PN1-PN2)
- HTTP protocol (no TLS)
- HTTPS with TLS negotiation
- Verbose output validation

#### Timeout and TLS Tests (E1-E5)
- Request timeouts
- Expired certificates
- Self-signed certificates
- Wrong hostname certificates

#### Parser Robustness (X1-X6)
- Unknown options
- Duplicate options
- Extraneous whitespace
- Very long URLs
- Malformed parameters

#### Integration Tests (I1-I3)
- Cookie-gated POST sequences
- Resumable downloads via ranges
- Complex header/cookie/redirect combinations

#### Web Radio Tests (Section 21)
- Basic streaming
- Stream stopping
- Pure binary output validation
- Error handling

## Test Results

### Console Output

Tests provide detailed console output with:
- Command sent/received logging
- Success/failure indicators
- Response validation
- Performance timing
- Error details

### HTML Reports

When running with pytest, HTML reports are generated:
- Test summary and statistics
- Detailed test results
- Failure analysis
- Test duration and performance

### Log Files

Detailed logs are written to:
- `atbn_tests.log` - Comprehensive test execution log
- Console output with timestamps

## Configuration Options

### Test Selection

Use environment variables to enable/disable test categories:

```env
TEST_BASIC_COMMANDS=true       # Basic AT connectivity
TEST_WIFI_CONNECTION=true      # WiFi setup and management
TEST_SD_CARD_OPERATIONS=true   # SD card mount/unmount
TEST_HTTP_OPERATIONS=true      # Basic HTTP requests
TEST_ATBN_COMMANDS=true        # Full ATBN command suite
TEST_VERBOSE_MODE=true         # Verbose output testing
TEST_COOKIE_OPERATIONS=true    # Cookie save/load
TEST_RANGE_DOWNLOADS=true      # Range and resumable downloads
```

### URL Configuration

Customize test URLs for your environment:

```env
TEST_HTTP_URL=http://httpbin.org/get
TEST_HTTPS_URL=https://httpbin.org/get
TEST_JSON_URL=https://httpbin.org/json
TEST_POST_URL=https://httpbin.org/post
TEST_LARGE_FILE_URL=https://httpbin.org/bytes/10240
```

### Performance Settings

Configure timeouts and performance thresholds:

```env
PERFORMANCE_TEST_TIMEOUT=800
MAX_DOWNLOAD_TIME=300
MIN_DOWNLOAD_SPEED=0.1
```

## Troubleshooting

### Common Issues

1. **Serial Connection Failed**
   - Check `SERIAL_PORT` in `.env`
   - Ensure ESP32 is connected and powered
   - Verify no other applications using the port

2. **WiFi Connection Failed**
   - Verify `WIFI_SSID` and `WIFI_PASSWORD` in `.env`
   - Check WiFi network availability
   - Ensure ESP32 supports the WiFi security type

3. **SD Card Tests Failed**
   - Verify SD card is properly inserted
   - Check SD card wiring (SPI connections)
   - Ensure SD card is FAT32 formatted

4. **HTTP Tests Failed**
   - Verify internet connectivity
   - Check if test URLs are accessible
   - Ensure firewall allows ESP32 traffic

### Debug Mode

Enable detailed logging by modifying the logging level:

```python
logging.basicConfig(level=logging.DEBUG)
```

### Manual Testing

For manual verification of specific commands:

```python
from test_atbn_comprehensive import ATBNTester

tester = ATBNTester()
tester.connect()
success, response = tester.send_command('AT+BNCURL="GET","https://httpbin.org/get"')
print(f"Success: {success}")
print(f"Response: {response}")
tester.disconnect()
```

## Test Specification Compliance

This test suite implements the complete test specification from `test.md`:

- ✅ All 21 major test categories covered
- ✅ Error handling and edge cases
- ✅ Protocol compliance validation
- ✅ Performance and stress testing
- ✅ Integration scenarios
- ✅ Real-world API testing

### Standards Compliance

- UART protocol framing validation
- HTTP/HTTPS protocol compliance
- TLS certificate validation
- Cookie RFC compliance
- Range request standards

## Contributing

When adding new tests:

1. Follow the existing naming convention
2. Add appropriate pytest markers
3. Include docstrings with test case IDs from `test.md`
4. Handle environment dependencies gracefully
5. Add configuration options to `.env.example`
6. Update this README with new test categories

## License

This test suite is part of the ESP-AT project and follows the same licensing terms.
