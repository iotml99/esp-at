#!/usr/bin/env python3
"""
Comprehensive ESP32-AT Commands Test Suite
==========================================

This is the consolidated test suite for ESP32-AT firmware with custom ATBN commands.
It combines all functionality from previous separate test files into a single,
well-organized test suite with proper pytest integration.

Features:
- Basic AT command testing
- WiFi connectivity tests
- SD card operations
- HTTP/HTTPS requests with BNCURL
- Performance testing with large file downloads
- Webradio streaming tests
- WPS functionality
- Certificate flashing
- Configuration-driven testing

Requirements:
- python-dotenv
- pyserial
- pytest

Usage:
    pytest test_esp32_at_commands.py -v           # Run all tests
    pytest test_esp32_at_commands.py -m basic     # Basic tests only
    pytest test_esp32_at_commands.py -m wifi      # WiFi tests only
    pytest test_esp32_at_commands.py -m performance # Performance tests
    python test_esp32_at_commands.py              # Direct execution
"""

import os
import sys
import time
import serial
import logging
import pytest
import re
import configparser
from typing import Optional, Tuple, List, Dict, Any
from pathlib import Path
from dotenv import load_dotenv

# Load environment variables
env_path = Path(__file__).parent / '.env'
if env_path.exists():
    load_dotenv(env_path)

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
    handlers=[
        logging.StreamHandler(),
        logging.FileHandler('esp32_at_tests.log')
    ]
)
logger = logging.getLogger(__name__)


class ESP32ATTester:
    """
    Comprehensive ESP32-AT command tester with all functionality consolidated.
    """
    
    def __init__(self, port: str = None, baudrate: int = 115200, timeout: int = 30):
        """Initialize the tester with configuration."""
        self.port = port or os.getenv('SERIAL_PORT', 'COM3')
        self.baudrate = baudrate
        self.timeout = timeout
        self.conn = None
        self.is_connected = False
        self.wifi_connected = False
        self.sd_mounted = False
        self.performance_data = {}
        
        # WiFi credentials from environment
        self.wifi_ssid = os.getenv('WIFI_SSID')
        self.wifi_password = os.getenv('WIFI_PASSWORD')
        
    def connect(self) -> bool:
        """Connect to the ESP32 device."""
        try:
            self.conn = serial.Serial(self.port, self.baudrate, timeout=self.timeout)
            time.sleep(2)  # Allow device to stabilize
            self.is_connected = True
            logger.info(f"Connected to {self.port} at {self.baudrate} baud")
            return True
        except Exception as e:
            logger.error(f"Failed to connect to {self.port}: {e}")
            return False
    
    def disconnect(self):
        """Disconnect from the ESP32 device."""
        if self.conn:
            self.conn.close()
            self.is_connected = False
            logger.info("Disconnected from ESP32")
    
    def send_command(self, command: str, timeout: int = None) -> Tuple[bool, List[str]]:
        """
        Send AT command and return response.
        
        Args:
            command: AT command to send
            timeout: Timeout in seconds (uses default if None)
            
        Returns:
            Tuple of (success, response_lines)
        """
        if not self.conn:
            return False, ["No connection"]
        
        original_timeout = self.conn.timeout
        if timeout:
            self.conn.timeout = timeout
            
        try:
            # Clear any pending data
            self.conn.reset_input_buffer()
            
            # Send command
            cmd = command.strip()
            if not cmd.endswith('\\r\\n'):
                cmd += '\\r\\n'
            
            logger.debug(f"Sending: {cmd.strip()}")
            self.conn.write(cmd.encode())
            
            # Read response
            response_lines = []
            start_time = time.time()
            current_timeout = timeout or self.timeout
            
            while time.time() - start_time < current_timeout:
                try:
                    line = self.conn.readline().decode('utf-8', errors='ignore').strip()
                    if line:
                        response_lines.append(line)
                        logger.debug(f"Received: {line}")
                        
                        # Check for completion
                        if line in ['OK', 'ERROR', 'FAIL']:
                            break
                        if 'ERROR' in line or 'FAIL' in line:
                            break
                            
                except serial.SerialTimeoutException:
                    break
                except Exception as e:
                    logger.error(f"Error reading response: {e}")
                    break
            
            # Determine success
            success = any(line in ['OK', '+'] or line.startswith('+') for line in response_lines)
            if not success:
                success = len(response_lines) > 0 and not any('ERROR' in line or 'FAIL' in line for line in response_lines)
            
            return success, response_lines
            
        except Exception as e:
            logger.error(f"Error sending command '{command}': {e}")
            return False, [str(e)]
        finally:
            self.conn.timeout = original_timeout
    
    def wait_for_response(self, expected_text: str, timeout: int = 30) -> Tuple[bool, List[str]]:
        """Wait for specific text in response."""
        start_time = time.time()
        response_lines = []
        
        while time.time() - start_time < timeout:
            try:
                line = self.conn.readline().decode('utf-8', errors='ignore').strip()
                if line:
                    response_lines.append(line)
                    logger.debug(f"Waiting - received: {line}")
                    
                    if expected_text in line:
                        return True, response_lines
                        
            except Exception as e:
                logger.error(f"Error waiting for response: {e}")
                break
        
        return False, response_lines


# =============================================================================
# PYTEST FIXTURES
# =============================================================================

@pytest.fixture(scope="session")
def config():
    """Load configuration from config.ini."""
    config = configparser.ConfigParser()
    config_file = Path(__file__).parent / 'config.ini'
    
    if config_file.exists():
        config.read(config_file)
        return config
    return None

@pytest.fixture(scope="session")
def tester(config):
    """Create and connect ESP32 tester instance."""
    port = 'COM3'  # Default
    
    if config:
        try:
            port = config.get('serial', 'port', fallback='COM3')
        except:
            pass
    
    # Override with environment variable if available
    port = os.getenv('SERIAL_PORT', port)
    
    tester = ESP32ATTester(port=port)
    
    if not tester.connect():
        pytest.skip(f"Cannot connect to ESP32 on {port}")
    
    yield tester
    tester.disconnect()

@pytest.fixture
def wifi_credentials():
    """Get WiFi credentials from environment."""
    ssid = os.getenv('WIFI_SSID')
    password = os.getenv('WIFI_PASSWORD')
    
    if not ssid:
        pytest.skip("WiFi credentials not configured")
    
    return ssid, password


# =============================================================================
# BASIC AT COMMAND TESTS
# =============================================================================

@pytest.mark.basic
class TestBasicATCommands:
    """Basic AT command functionality tests."""
    
    def test_basic_connectivity(self, tester):
        """Test basic AT command response."""
        success, response = tester.send_command("AT")
        assert success, f"Basic AT command failed: {response}"
        assert any("OK" in line for line in response), "No OK response received"
    
    def test_firmware_version(self, tester):
        """Test firmware version query."""
        success, response = tester.send_command("AT+GMR")
        assert success, f"Firmware version query failed: {response}"
        
        # Look for version information
        version_found = any("version" in line.lower() or "build" in line.lower() 
                          for line in response)
        assert version_found, f"No version information in response: {response}"
    
    def test_echo_disable(self, tester):
        """Test disabling echo."""
        success, response = tester.send_command("ATE0")
        assert success, f"Echo disable failed: {response}"


# =============================================================================
# WIFI FUNCTIONALITY TESTS
# =============================================================================

@pytest.mark.wifi
class TestWiFiFunctionality:
    """WiFi connection and management tests."""
    
    def test_wifi_mode_station(self, tester):
        """Set WiFi to station mode."""
        success, response = tester.send_command("AT+CWMODE=1")
        assert success, f"Failed to set station mode: {response}"
    
    def test_wifi_connection(self, tester, wifi_credentials):
        """Test WiFi connection."""
        ssid, password = wifi_credentials
        
        # Set station mode first
        success, _ = tester.send_command("AT+CWMODE=1")
        assert success, "Failed to set station mode"
        
        # Connect to WiFi
        cmd = f'AT+CWJAP="{ssid}","{password}"'
        success, response = tester.send_command(cmd, timeout=30)
        assert success, f"WiFi connection failed: {response}"
        
        tester.wifi_connected = True
    
    def test_ip_address_query(self, tester):
        """Query IP address (requires WiFi connection)."""
        if not tester.wifi_connected:
            pytest.skip("WiFi not connected")
        
        success, response = tester.send_command("AT+CIFSR")
        assert success, f"IP address query failed: {response}"
        
        # Look for IP address pattern
        ip_found = any(re.search(r'\\d+\\.\\d+\\.\\d+\\.\\d+', line) for line in response)
        assert ip_found, f"No IP address found in response: {response}"


# =============================================================================
# SD CARD TESTS
# =============================================================================

@pytest.mark.sd_card
class TestSDCardOperations:
    """SD card mounting and operations tests."""
    
    def test_sd_card_help(self, tester):
        """Test SD card command help."""
        success, response = tester.send_command("AT+BNSD_MOUNT=?")
        assert success, f"SD card help command failed: {response}"
    
    def test_sd_card_status_query(self, tester):
        """Test SD card status query."""
        success, response = tester.send_command("AT+BNSD_MOUNT?")
        assert success, f"SD card status query failed: {response}"
    
    def test_sd_card_mount(self, tester):
        """Test SD card mounting."""
        success, response = tester.send_command("AT+BNSD_MOUNT", timeout=10)
        
        if not success:
            # Check if it's due to no SD card
            error_text = " ".join(response).lower()
            if "no sd card" in error_text or "not found" in error_text:
                pytest.skip("No SD card detected")
            else:
                pytest.fail(f"SD card mount failed: {response}")
        
        tester.sd_mounted = True
        assert success, f"SD card mount failed: {response}"
    
    def test_sd_card_space_query(self, tester):
        """Test SD card space query."""
        if not tester.sd_mounted:
            pytest.skip("SD card not mounted")
        
        success, response = tester.send_command("AT+BNSD_SPACE?")
        assert success, f"SD card space query failed: {response}"
    
    def test_sd_card_unmount(self, tester):
        """Test SD card unmounting."""
        if not tester.sd_mounted:
            pytest.skip("SD card not mounted")
        
        success, response = tester.send_command("AT+BNSD_UNMOUNT")
        assert success, f"SD card unmount failed: {response}"
        
        tester.sd_mounted = False


# =============================================================================
# HTTP/BNCURL TESTS
# =============================================================================

@pytest.mark.http
class TestHTTPOperations:
    """HTTP operations with BNCURL commands."""
    
    def test_bncurl_help(self, tester):
        """Test BNCURL help command."""
        success, response = tester.send_command("AT+BNCURL=?")
        assert success, f"BNCURL help command failed: {response}"
        
        # Check for usage information
        help_found = any("usage" in line.lower() or "example" in line.lower() 
                        for line in response)
        assert help_found, f"No help information found: {response}"
    
    def test_bncurl_status_query(self, tester):
        """Test BNCURL status query."""
        success, response = tester.send_command("AT+BNCURL?")
        assert success, f"BNCURL status query failed: {response}"
    
    @pytest.mark.requires_wifi
    def test_simple_http_get(self, tester):
        """Test simple HTTP GET request."""
        if not tester.wifi_connected:
            pytest.skip("WiFi not connected")
        
        cmd = 'AT+BNCURL=GET,"http://httpbin.org/get"'
        success, response = tester.send_command(cmd, timeout=30)
        assert success, f"HTTP GET request failed: {response}"
    
    @pytest.mark.requires_wifi
    def test_http_head_request(self, tester):
        """Test HTTP HEAD request."""
        if not tester.wifi_connected:
            pytest.skip("WiFi not connected")
        
        cmd = 'AT+BNCURL=HEAD,"http://httpbin.org/get"'
        success, response = tester.send_command(cmd, timeout=20)
        assert success, f"HTTP HEAD request failed: {response}"
    
    @pytest.mark.requires_wifi
    @pytest.mark.requires_sd
    def test_http_download_to_sd(self, tester):
        """Test HTTP GET with save to SD card."""
        if not tester.wifi_connected:
            pytest.skip("WiFi not connected")
        if not tester.sd_mounted:
            pytest.skip("SD card not mounted")
        
        cmd = 'AT+BNCURL=GET,"http://httpbin.org/json",-dd,"/sdcard/test.json"'
        success, response = tester.send_command(cmd, timeout=30)
        assert success, f"HTTP download to SD failed: {response}"


# =============================================================================
# PERFORMANCE TESTS
# =============================================================================

@pytest.mark.performance
@pytest.mark.slow
class TestPerformance:
    """Performance testing with large file downloads."""
    
    @pytest.mark.requires_wifi
    @pytest.mark.requires_sd
    def test_small_file_download(self, tester):
        """Test small file download performance (1MB)."""
        if not tester.wifi_connected:
            pytest.skip("WiFi not connected")
        if not tester.sd_mounted:
            pytest.skip("SD card not mounted")
        
        start_time = time.time()
        cmd = 'AT+BNCURL=GET,"https://httpbin.org/bytes/1048576",-dd,"/sdcard/test_1mb.bin"'
        success, response = tester.send_command(cmd, timeout=60)
        end_time = time.time()
        
        assert success, f"1MB download failed: {response}"
        
        download_time = end_time - start_time
        speed_mbps = (1 * 8) / download_time  # 1MB converted to Mbps
        
        logger.info(f"1MB download completed in {download_time:.2f}s ({speed_mbps:.2f} Mbps)")
        tester.performance_data['1mb_download'] = {
            'time': download_time,
            'speed_mbps': speed_mbps
        }
    
    @pytest.mark.requires_wifi
    @pytest.mark.requires_sd
    def test_medium_file_download(self, tester):
        """Test medium file download performance (10MB)."""
        if not tester.wifi_connected:
            pytest.skip("WiFi not connected")
        if not tester.sd_mounted:
            pytest.skip("SD card not mounted")
        
        start_time = time.time()
        cmd = 'AT+BNCURL=GET,"https://bones.ch/media/qr/10M.txt",-dd,"/sdcard/test_10mb.txt"'
        success, response = tester.send_command(cmd, timeout=300)
        end_time = time.time()
        
        assert success, f"10MB download failed: {response}"
        
        download_time = end_time - start_time
        speed_mbps = (10 * 8) / download_time  # 10MB converted to Mbps
        
        logger.info(f"10MB download completed in {download_time:.2f}s ({speed_mbps:.2f} Mbps)")
        tester.performance_data['10mb_download'] = {
            'time': download_time,
            'speed_mbps': speed_mbps
        }
    
    @pytest.mark.requires_wifi
    @pytest.mark.requires_sd
    @pytest.mark.very_slow
    def test_large_file_download(self, tester):
        """Test large file download performance (50MB+)."""
        if not tester.wifi_connected:
            pytest.skip("WiFi not connected")
        if not tester.sd_mounted:
            pytest.skip("SD card not mounted")
        
        start_time = time.time()
        cmd = 'AT+BNCURL=GET,"https://bones.ch/media/qr/80M.txt",-dd,"/sdcard/test_80mb.txt"'
        success, response = tester.send_command(cmd, timeout=900)  # 15 minutes
        end_time = time.time()
        
        assert success, f"80MB download failed: {response}"
        
        download_time = end_time - start_time
        speed_mbps = (80 * 8) / download_time  # 80MB converted to Mbps
        
        logger.info(f"80MB download completed in {download_time:.2f}s ({speed_mbps:.2f} Mbps)")
        tester.performance_data['80mb_download'] = {
            'time': download_time,
            'speed_mbps': speed_mbps
        }
    
    def test_performance_summary(self, tester):
        """Display performance test summary."""
        if not tester.performance_data:
            pytest.skip("No performance data available")
        
        logger.info("\\n" + "="*50)
        logger.info("PERFORMANCE TEST SUMMARY")
        logger.info("="*50)
        
        for test_name, data in tester.performance_data.items():
            logger.info(f"{test_name}: {data['time']:.2f}s @ {data['speed_mbps']:.2f} Mbps")
        
        logger.info("="*50)


# =============================================================================
# ADVANCED FEATURES TESTS
# =============================================================================

@pytest.mark.advanced
class TestAdvancedFeatures:
    """Advanced features testing."""
    
    def test_bncurl_timeout_config(self, tester):
        """Test BNCURL timeout configuration."""
        # Query current timeout
        success, response = tester.send_command("AT+BNCURL_TIMEOUT?")
        assert success, f"Timeout query failed: {response}"
        
        # Set new timeout
        success, response = tester.send_command("AT+BNCURL_TIMEOUT=60")
        assert success, f"Timeout set failed: {response}"
    
    def test_bncurl_statistics(self, tester):
        """Test BNCURL statistics query."""
        success, response = tester.send_command("AT+BNCURL_STATS?")
        assert success, f"Statistics query failed: {response}"
    
    @pytest.mark.requires_wifi
    def test_webradio_help(self, tester):
        """Test webradio command help."""
        success, response = tester.send_command("AT+BNWEBRADIO=?")
        assert success, f"Webradio help failed: {response}"
    
    def test_wps_help(self, tester):
        """Test WPS command help."""
        success, response = tester.send_command("AT+BNWPS=?")
        assert success, f"WPS help failed: {response}"


# =============================================================================
# PARAMETRIZED TESTS
# =============================================================================

@pytest.mark.parametrize("command,expected_in_response", [
    ("AT", "OK"),
    ("AT+GMR", "version"),
    ("AT+BNSD_MOUNT=?", "Test SD card"),
    ("AT+BNCURL=?", "Usage"),
    ("AT+BNWEBRADIO=?", "Usage"),
    ("AT+BNWPS=?", "OK"),
])
def test_command_help_responses(tester, command, expected_in_response):
    """Parametrized test for command help responses."""
    success, response = tester.send_command(command)
    assert success, f"Command {command} failed: {response}"
    
    response_text = " ".join(response).lower()
    assert expected_in_response.lower() in response_text, \
        f"Expected '{expected_in_response}' not found in response: {response}"


# =============================================================================
# MAIN EXECUTION FOR DIRECT RUNNING
# =============================================================================

def main():
    """Main function for direct execution."""
    print("ESP32-AT Commands Test Suite")
    print("=" * 50)
    
    # Load configuration
    config = configparser.ConfigParser()
    config_file = Path(__file__).parent / 'config.ini'
    
    if config_file.exists():
        config.read(config_file)
        port = config.get('serial', 'port', fallback='COM3')
    else:
        port = os.getenv('SERIAL_PORT', 'COM3')
    
    # Create tester
    tester = ESP32ATTester(port=port)
    
    if not tester.connect():
        print(f"❌ Failed to connect to ESP32 on {port}")
        return 1
    
    print(f"✅ Connected to ESP32 on {port}")
    
    # Run basic tests
    tests_passed = 0
    tests_total = 0
    
    test_functions = [
        ("Basic Connectivity", lambda: tester.send_command("AT")),
        ("Firmware Version", lambda: tester.send_command("AT+GMR")),
        ("SD Card Help", lambda: tester.send_command("AT+BNSD_MOUNT=?")),
        ("BNCURL Help", lambda: tester.send_command("AT+BNCURL=?")),
    ]
    
    for test_name, test_func in test_functions:
        tests_total += 1
        try:
            success, response = test_func()
            if success:
                print(f"✅ {test_name}: PASS")
                tests_passed += 1
            else:
                print(f"❌ {test_name}: FAIL - {response}")
        except Exception as e:
            print(f"❌ {test_name}: ERROR - {e}")
    
    # Summary
    print("\\n" + "=" * 50)
    print(f"RESULTS: {tests_passed}/{tests_total} tests passed")
    print("=" * 50)
    
    tester.disconnect()
    
    return 0 if tests_passed == tests_total else 1


if __name__ == "__main__":
    sys.exit(main())
